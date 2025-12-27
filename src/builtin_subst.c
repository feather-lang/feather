#include "feather.h"
#include "host.h"
#include "internal.h"

static FeatherObj append_str(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj result, const char *s, size_t len) {
  if (len == 0) return result;
  FeatherObj seg = ops->string.intern(interp, s, len);
  if (ops->list.is_nil(interp, result)) {
    return seg;
  }
  return ops->string.concat(interp, result, seg);
}

/**
 * append_obj appends an object's string value to the result using concat().
 * This avoids string.get() by delegating string extraction to the host.
 */
static FeatherObj append_obj(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj result, FeatherObj obj) {
  if (ops->list.is_nil(interp, obj)) {
    return result;
  }
  if (ops->list.is_nil(interp, result)) {
    return obj;
  }
  return ops->string.concat(interp, result, obj);
}

/**
 * Helper to check if an option object equals a specific literal.
 * Uses ops->string.equal to avoid string.get().
 */
static int option_equals(const FeatherHostOps *ops, FeatherInterp interp,
                         FeatherObj opt, const char *lit) {
  FeatherObj lit_obj = ops->string.intern(interp, lit, 0);
  // Calculate length by scanning to null
  size_t len = 0;
  while (lit[len]) len++;
  lit_obj = ops->string.intern(interp, lit, len);
  return ops->string.equal(interp, opt, lit_obj);
}

/**
 * Build error message: "bad option \"<opt>\": must be -nobackslashes, -nocommands, or -novariables"
 * Uses string builder to avoid string.get() on opt.
 */
static FeatherObj build_bad_option_error(const FeatherHostOps *ops, FeatherInterp interp,
                                         FeatherObj opt) {
  FeatherObj builder = ops->string.builder_new(interp, 128);
  const char *prefix = "bad option \"";
  for (size_t i = 0; prefix[i]; i++) {
    ops->string.builder_append_byte(interp, builder, prefix[i]);
  }
  ops->string.builder_append_obj(interp, builder, opt);
  const char *suffix = "\": must be -nobackslashes, -nocommands, or -novariables";
  for (size_t i = 0; suffix[i]; i++) {
    ops->string.builder_append_byte(interp, builder, suffix[i]);
  }
  return ops->string.builder_finish(interp, builder);
}

/**
 * Build error message: "can't read \"<name>\": no such variable"
 * Uses string builder to avoid string.get() on name.
 */
static FeatherObj build_no_such_variable_error(const FeatherHostOps *ops, FeatherInterp interp,
                                               FeatherObj name) {
  FeatherObj builder = ops->string.builder_new(interp, 128);
  const char *prefix = "can't read \"";
  for (size_t i = 0; prefix[i]; i++) {
    ops->string.builder_append_byte(interp, builder, prefix[i]);
  }
  ops->string.builder_append_obj(interp, builder, name);
  const char *suffix = "\": no such variable";
  for (size_t i = 0; suffix[i]; i++) {
    ops->string.builder_append_byte(interp, builder, suffix[i]);
  }
  return ops->string.builder_finish(interp, builder);
}

static size_t process_backslash_subst(const char *p, const char *end,
                                       char *buf, size_t *out_len) {
  if (p >= end) {
    buf[0] = '\\';
    *out_len = 1;
    return 0;
  }

  switch (*p) {
    case 'a': buf[0] = '\a'; *out_len = 1; return 1;
    case 'b': buf[0] = '\b'; *out_len = 1; return 1;
    case 'f': buf[0] = '\f'; *out_len = 1; return 1;
    case 'n': buf[0] = '\n'; *out_len = 1; return 1;
    case 'r': buf[0] = '\r'; *out_len = 1; return 1;
    case 't': buf[0] = '\t'; *out_len = 1; return 1;
    case 'v': buf[0] = '\v'; *out_len = 1; return 1;
    case '\\': buf[0] = '\\'; *out_len = 1; return 1;
    case '\n': {
      buf[0] = ' ';
      *out_len = 1;
      size_t consumed = 1;
      while (p + consumed < end && (p[consumed] == ' ' || p[consumed] == '\t')) {
        consumed++;
      }
      return consumed;
    }
    case 'x': {
      if (p + 1 < end) {
        int val = 0;
        size_t i = 1;
        while (i < 3 && p + i < end) {
          char c = p[i];
          if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
          else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
          else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
          else break;
          i++;
        }
        if (i > 1) {
          buf[0] = (char)val;
          *out_len = 1;
          return i;
        }
      }
      buf[0] = 'x';
      *out_len = 1;
      return 1;
    }
    default:
      if (*p >= '0' && *p <= '7') {
        int val = *p - '0';
        size_t i = 1;
        while (i < 3 && p + i < end && p[i] >= '0' && p[i] <= '7') {
          val = val * 8 + (p[i] - '0');
          i++;
        }
        buf[0] = (char)(val & 0xFF);
        *out_len = 1;
        return i;
      }
      buf[0] = *p;
      *out_len = 1;
      return 1;
  }
}

static const char *find_close_bracket(const char *p, const char *end) {
  int depth = 1;
  while (p < end) {
    if (*p == '[') {
      depth++;
    } else if (*p == ']') {
      depth--;
      if (depth == 0) return p;
    } else if (*p == '\\' && p + 1 < end) {
      p++;
    } else if (*p == '{') {
      int brace_depth = 1;
      p++;
      while (p < end && brace_depth > 0) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
        else if (*p == '\\' && p + 1 < end) p++;
        p++;
      }
      continue;
    } else if (*p == '"') {
      p++;
      while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) p++;
        p++;
      }
      if (p < end) p++;
      continue;
    }
    p++;
  }
  return NULL;
}

static int subst_is_varname_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

FeatherResult feather_builtin_subst(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  ops = feather_get_ops(ops);

  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"subst ?-nobackslashes? ?-nocommands? ?-novariables? string\"", 84);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  int flags = TCL_SUBST_ALL;
  size_t i = 0;

  while (i < argc - 1) {
    FeatherObj opt = ops->list.at(interp, args, i);

    // Check if starts with '-' using byte_at (avoids string.get)
    int first_byte = ops->string.byte_at(interp, opt, 0);
    if (first_byte == '-') {
      if (option_equals(ops, interp, opt, "-nobackslashes")) {
        flags &= ~TCL_SUBST_BACKSLASHES;
      } else if (option_equals(ops, interp, opt, "-nocommands")) {
        flags &= ~TCL_SUBST_COMMANDS;
      } else if (option_equals(ops, interp, opt, "-novariables")) {
        flags &= ~TCL_SUBST_VARIABLES;
      } else {
        ops->interp.set_result(interp, build_bad_option_error(ops, interp, opt));
        return TCL_ERROR;
      }
      i++;
    } else {
      break;
    }
  }

  if (i != argc - 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"subst ?-nobackslashes? ?-nocommands? ?-novariables? string\"", 84);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str_obj = ops->list.at(interp, args, i);
  size_t len;
  // NOTE: This string.get() remains for pointer-based parsing (like B5/B6 pattern)
  const char *str = ops->string.get(interp, str_obj, &len);

  const char *p = str;
  const char *end = str + len;
  FeatherObj result = 0;
  const char *seg_start = p;

  while (p < end) {
    if (*p == '\\' && (flags & TCL_SUBST_BACKSLASHES)) {
      if (p > seg_start) {
        result = append_str(ops, interp, result, seg_start, p - seg_start);
      }
      p++;
      char escape_buf[4];
      size_t escape_len;
      size_t consumed = process_backslash_subst(p, end, escape_buf, &escape_len);
      result = append_str(ops, interp, result, escape_buf, escape_len);
      p += consumed;
      seg_start = p;

    } else if (*p == '$' && (flags & TCL_SUBST_VARIABLES)) {
      if (p > seg_start) {
        result = append_str(ops, interp, result, seg_start, p - seg_start);
      }
      p++;

      if (p >= end) {
        result = append_str(ops, interp, result, "$", 1);
        seg_start = p;
        continue;
      }

      if (*p == '{') {
        p++;
        const char *name_start = p;
        while (p < end && *p != '}') p++;
        if (p >= end) {
          FeatherObj msg = ops->string.intern(interp, "missing close-brace for variable name", 37);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        size_t name_len = p - name_start;
        p++;
        FeatherObj name = ops->string.intern(interp, name_start, name_len);
        FeatherObj value = ops->var.get(interp, name);
        if (ops->list.is_nil(interp, value)) {
          ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, name));
          return TCL_ERROR;
        }
        // Use concat directly via append_obj - avoids string.get on value
        result = append_obj(ops, interp, result, value);
        seg_start = p;

      } else if (subst_is_varname_char(*p)) {
        const char *name_start = p;
        while (p < end && (subst_is_varname_char(*p) || *p == ':')) {
          if (*p == ':' && p + 1 < end && p[1] == ':') {
            p += 2;
          } else if (*p == ':') {
            break;
          } else {
            p++;
          }
        }
        size_t name_len = p - name_start;

        if (p < end && *p == '(') {
          p++;
          const char *idx_start = p;
          int paren_depth = 1;
          while (p < end && paren_depth > 0) {
            if (*p == '(') paren_depth++;
            else if (*p == ')') paren_depth--;
            if (paren_depth > 0) p++;
          }
          size_t idx_len = p - idx_start;
          if (p < end) p++;

          if (flags & TCL_SUBST_COMMANDS) {
            FeatherResult subst_res = feather_subst(ops, interp, idx_start, idx_len, TCL_SUBST_ALL);
            if (subst_res != TCL_OK) return TCL_ERROR;
            FeatherObj subst_idx = ops->interp.get_result(interp);

            // Build full array name: name(subst_idx) using builder
            FeatherObj builder = ops->string.builder_new(interp, 64);
            for (size_t j = 0; j < name_len; j++) {
              ops->string.builder_append_byte(interp, builder, name_start[j]);
            }
            ops->string.builder_append_byte(interp, builder, '(');
            ops->string.builder_append_obj(interp, builder, subst_idx);
            ops->string.builder_append_byte(interp, builder, ')');
            FeatherObj full_name = ops->string.builder_finish(interp, builder);

            FeatherObj value = ops->var.get(interp, full_name);
            if (ops->list.is_nil(interp, value)) {
              ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, full_name));
              return TCL_ERROR;
            }
            // Use concat directly - avoids string.get on value
            result = append_obj(ops, interp, result, value);
          } else {
            // Build full array name without substitution
            FeatherObj builder = ops->string.builder_new(interp, 64);
            for (size_t j = 0; j < name_len; j++) {
              ops->string.builder_append_byte(interp, builder, name_start[j]);
            }
            ops->string.builder_append_byte(interp, builder, '(');
            for (size_t j = 0; j < idx_len; j++) {
              ops->string.builder_append_byte(interp, builder, idx_start[j]);
            }
            ops->string.builder_append_byte(interp, builder, ')');
            FeatherObj full_name = ops->string.builder_finish(interp, builder);

            FeatherObj value = ops->var.get(interp, full_name);
            if (ops->list.is_nil(interp, value)) {
              ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, full_name));
              return TCL_ERROR;
            }
            // Use concat directly - avoids string.get on value
            result = append_obj(ops, interp, result, value);
          }
        } else {
          FeatherObj name = ops->string.intern(interp, name_start, name_len);
          FeatherObj value = ops->var.get(interp, name);
          if (ops->list.is_nil(interp, value)) {
            ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, name));
            return TCL_ERROR;
          }
          // Use concat directly - avoids string.get on value
          result = append_obj(ops, interp, result, value);
        }
        seg_start = p;
      } else {
        result = append_str(ops, interp, result, "$", 1);
        seg_start = p;
      }

    } else if (*p == '[' && (flags & TCL_SUBST_COMMANDS)) {
      if (p > seg_start) {
        result = append_str(ops, interp, result, seg_start, p - seg_start);
      }
      p++;

      const char *close = find_close_bracket(p, end);
      if (close == NULL) {
        FeatherObj msg = ops->string.intern(interp, "missing close-bracket", 21);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      size_t cmd_len = close - p;
      FeatherResult eval_result = feather_script_eval(ops, interp, p, cmd_len, TCL_EVAL_LOCAL);

      if (eval_result == TCL_BREAK) {
        if (ops->list.is_nil(interp, result)) {
          result = ops->string.intern(interp, "", 0);
        }
        ops->interp.set_result(interp, result);
        return TCL_OK;
      } else if (eval_result == TCL_CONTINUE) {
        p = close + 1;
        seg_start = p;
        continue;
      } else if (eval_result == TCL_RETURN) {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          // Use concat directly - avoids string.get on cmd_result
          result = append_obj(ops, interp, result, cmd_result);
        }
        p = close + 1;
        seg_start = p;
      } else if (eval_result == TCL_ERROR) {
        return TCL_ERROR;
      } else if (eval_result >= 5) {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          // Use concat directly - avoids string.get on cmd_result
          result = append_obj(ops, interp, result, cmd_result);
        }
        p = close + 1;
        seg_start = p;
      } else {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          // Use concat directly - avoids string.get on cmd_result
          result = append_obj(ops, interp, result, cmd_result);
        }
        p = close + 1;
        seg_start = p;
      }

    } else {
      p++;
    }
  }

  if (p > seg_start) {
    result = append_str(ops, interp, result, seg_start, p - seg_start);
  }

  if (ops->list.is_nil(interp, result)) {
    result = ops->string.intern(interp, "", 0);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
