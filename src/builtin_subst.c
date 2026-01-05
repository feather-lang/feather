#include "feather.h"
#include "host.h"
#include "internal.h"
#include "charclass.h"
#include "unicode.h"

static FeatherObj append_literal(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj result, const char *s, size_t len) {
  if (len == 0) return result;
  FeatherObj seg = ops->string.intern(interp, s, len);
  if (ops->list.is_nil(interp, result)) {
    return seg;
  }
  return ops->string.concat(interp, result, seg);
}

static FeatherObj append_slice(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj result, FeatherObj str, size_t start, size_t end) {
  if (start >= end) return result;
  FeatherObj seg = ops->string.slice(interp, str, start, end);
  if (ops->list.is_nil(interp, result)) {
    return seg;
  }
  return ops->string.concat(interp, result, seg);
}

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

static int option_equals(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj opt, const char *lit) {
  size_t len = 0;
  while (lit[len]) len++;
  FeatherObj lit_obj = ops->string.intern(interp, lit, len);
  return ops->string.equal(interp, opt, lit_obj);
}

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

static FeatherResult process_backslash_subst_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj str, size_t pos, size_t len,
                                           char *buf, size_t *out_len, size_t *consumed) {
  if (pos >= len) {
    buf[0] = '\\';
    *out_len = 1;
    *consumed = 0;
    return TCL_OK;
  }

  int c = ops->string.byte_at(interp, str, pos);

  switch (c) {
    case 'a': buf[0] = '\a'; *out_len = 1; *consumed = 1; return TCL_OK;
    case 'b': buf[0] = '\b'; *out_len = 1; *consumed = 1; return TCL_OK;
    case 'f': buf[0] = '\f'; *out_len = 1; *consumed = 1; return TCL_OK;
    case 'n': buf[0] = '\n'; *out_len = 1; *consumed = 1; return TCL_OK;
    case 'r': buf[0] = '\r'; *out_len = 1; *consumed = 1; return TCL_OK;
    case 't': buf[0] = '\t'; *out_len = 1; *consumed = 1; return TCL_OK;
    case 'v': buf[0] = '\v'; *out_len = 1; *consumed = 1; return TCL_OK;
    case '\\': buf[0] = '\\'; *out_len = 1; *consumed = 1; return TCL_OK;
    case '\n': {
      buf[0] = ' ';
      *out_len = 1;
      *consumed = 1;
      while (pos + *consumed < len) {
        int ch = ops->string.byte_at(interp, str, pos + *consumed);
        if (ch != ' ' && ch != '\t') break;
        (*consumed)++;
      }
      return TCL_OK;
    }
    case 'x': {
      if (pos + 1 < len) {
        int val = 0;
        size_t i = 1;
        while (i < 3 && pos + i < len) {
          int ch = ops->string.byte_at(interp, str, pos + i);
          if (!feather_is_hex_digit(ch)) break;
          val = val * 16 + feather_hex_value(ch);
          i++;
        }
        if (i > 1) {
          buf[0] = (char)val;
          *out_len = 1;
          *consumed = i;
          return TCL_OK;
        }
      }
      buf[0] = 'x';
      *out_len = 1;
      *consumed = 1;
      return TCL_OK;
    }
    case 'u': {
      // \uNNNN - 16-bit Unicode escape (4 hex digits)
      uint32_t codepoint = 0;
      size_t i = 1;
      for (; i <= 4 && pos + i < len; i++) {
        int ch = ops->string.byte_at(interp, str, pos + i);
        if (!feather_is_hex_digit(ch)) break;
        codepoint = codepoint * 16 + feather_hex_value(ch);
      }
      if (i != 5) {
        // Didn't get exactly 4 hex digits
        FeatherObj msg = ops->string.intern(interp, "missing hexadecimal digits for \\u escape", 40);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      *out_len = feather_utf8_encode(codepoint, buf);
      *consumed = 5; // 'u' + 4 hex digits
      return TCL_OK;
    }
    case 'U': {
      // \UNNNNNNNN - 32-bit Unicode escape (8 hex digits)
      uint32_t codepoint = 0;
      size_t i = 1;
      for (; i <= 8 && pos + i < len; i++) {
        int ch = ops->string.byte_at(interp, str, pos + i);
        if (!feather_is_hex_digit(ch)) break;
        codepoint = codepoint * 16 + feather_hex_value(ch);
      }
      if (i != 9) {
        // Didn't get exactly 8 hex digits
        FeatherObj msg = ops->string.intern(interp, "missing hexadecimal digits for \\U escape", 40);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      *out_len = feather_utf8_encode(codepoint, buf);
      *consumed = 9; // 'U' + 8 hex digits
      return TCL_OK;
    }
    default:
      if (c >= '0' && c <= '7') {
        int val = c - '0';
        size_t i = 1;
        while (i < 3 && pos + i < len) {
          int ch = ops->string.byte_at(interp, str, pos + i);
          if (ch < '0' || ch > '7') break;
          val = val * 8 + (ch - '0');
          i++;
        }
        buf[0] = (char)(val & 0xFF);
        *out_len = 1;
        *consumed = i;
        return TCL_OK;
      }
      buf[0] = (char)c;
      *out_len = 1;
      *consumed = 1;
      return TCL_OK;
  }
}

static size_t find_close_bracket_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj str, size_t pos, size_t len) {
  int depth = 1;
  while (pos < len) {
    int c = ops->string.byte_at(interp, str, pos);
    if (c == '[') {
      depth++;
    } else if (c == ']') {
      depth--;
      if (depth == 0) return pos;
    } else if (c == '\\' && pos + 1 < len) {
      pos++;
    } else if (c == '{') {
      int brace_depth = 1;
      pos++;
      while (pos < len && brace_depth > 0) {
        int ch = ops->string.byte_at(interp, str, pos);
        if (ch == '{') brace_depth++;
        else if (ch == '}') brace_depth--;
        else if (ch == '\\' && pos + 1 < len) pos++;
        pos++;
      }
      continue;
    } else if (c == '"') {
      pos++;
      while (pos < len) {
        int ch = ops->string.byte_at(interp, str, pos);
        if (ch == '"') break;
        if (ch == '\\' && pos + 1 < len) pos++;
        pos++;
      }
      if (pos < len) pos++;
      continue;
    }
    pos++;
  }
  return len; // not found
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
  size_t len = ops->string.byte_length(interp, str_obj);

  size_t pos = 0;
  FeatherObj result = 0;
  size_t seg_start = 0;

  while (pos < len) {
    int c = ops->string.byte_at(interp, str_obj, pos);

    if (c == '\\' && (flags & TCL_SUBST_BACKSLASHES)) {
      if (pos > seg_start) {
        result = append_slice(ops, interp, result, str_obj, seg_start, pos);
      }
      pos++;
      char escape_buf[4];
      size_t escape_len;
      size_t consumed;
      FeatherResult res = process_backslash_subst_obj(ops, interp, str_obj, pos, len, escape_buf, &escape_len, &consumed);
      if (res != TCL_OK) {
        return res;
      }
      result = append_literal(ops, interp, result, escape_buf, escape_len);
      pos += consumed;
      seg_start = pos;

    } else if (c == '$' && (flags & TCL_SUBST_VARIABLES)) {
      if (pos > seg_start) {
        result = append_slice(ops, interp, result, str_obj, seg_start, pos);
      }
      pos++;

      if (pos >= len) {
        result = append_literal(ops, interp, result, "$", 1);
        seg_start = pos;
        continue;
      }

      c = ops->string.byte_at(interp, str_obj, pos);

      if (c == '{') {
        pos++;
        size_t name_start = pos;
        while (pos < len && ops->string.byte_at(interp, str_obj, pos) != '}') pos++;
        if (pos >= len) {
          FeatherObj msg = ops->string.intern(interp, "missing close-brace for variable name", 37);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        FeatherObj name = ops->string.slice(interp, str_obj, name_start, pos);
        pos++;
        FeatherObj value;
        feather_get_var(ops, interp, name, &value);
        if (ops->list.is_nil(interp, value)) {
          ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, name));
          return TCL_ERROR;
        }
        result = append_obj(ops, interp, result, value);
        seg_start = pos;

      } else if (feather_is_varname_char(c)) {
        size_t name_start = pos;
        while (pos < len) {
          int ch = ops->string.byte_at(interp, str_obj, pos);
          if (feather_is_varname_char(ch)) {
            pos++;
          } else if (ch == ':' && pos + 1 < len && ops->string.byte_at(interp, str_obj, pos + 1) == ':') {
            pos += 2;
          } else {
            break;
          }
        }

        c = (pos < len) ? ops->string.byte_at(interp, str_obj, pos) : -1;

        if (c == '(') {
          pos++;
          size_t idx_start = pos;
          int paren_depth = 1;
          while (pos < len && paren_depth > 0) {
            int ch = ops->string.byte_at(interp, str_obj, pos);
            if (ch == '(') paren_depth++;
            else if (ch == ')') paren_depth--;
            if (paren_depth > 0) pos++;
          }
          size_t idx_end = pos;
          if (pos < len) pos++;

          // Build full array name with substituted index
          FeatherObj name_part = ops->string.slice(interp, str_obj, name_start, idx_start - 1);
          FeatherObj idx_part = ops->string.slice(interp, str_obj, idx_start, idx_end);

          if (flags & TCL_SUBST_COMMANDS) {
            FeatherResult subst_res = feather_subst_obj(ops, interp, idx_part, TCL_SUBST_ALL);
            if (subst_res != TCL_OK) return TCL_ERROR;
            idx_part = ops->interp.get_result(interp);
          }

          // Build full name: name(idx)
          FeatherObj builder = ops->string.builder_new(interp, 64);
          ops->string.builder_append_obj(interp, builder, name_part);
          ops->string.builder_append_byte(interp, builder, '(');
          ops->string.builder_append_obj(interp, builder, idx_part);
          ops->string.builder_append_byte(interp, builder, ')');
          FeatherObj full_name = ops->string.builder_finish(interp, builder);

          FeatherObj value;
          feather_get_var(ops, interp, full_name, &value);
          if (ops->list.is_nil(interp, value)) {
            ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, full_name));
            return TCL_ERROR;
          }
          result = append_obj(ops, interp, result, value);
        } else {
          FeatherObj name = ops->string.slice(interp, str_obj, name_start, pos);
          FeatherObj value;
          feather_get_var(ops, interp, name, &value);
          if (ops->list.is_nil(interp, value)) {
            ops->interp.set_result(interp, build_no_such_variable_error(ops, interp, name));
            return TCL_ERROR;
          }
          result = append_obj(ops, interp, result, value);
        }
        seg_start = pos;
      } else {
        result = append_literal(ops, interp, result, "$", 1);
        seg_start = pos;
      }

    } else if (c == '[' && (flags & TCL_SUBST_COMMANDS)) {
      if (pos > seg_start) {
        result = append_slice(ops, interp, result, str_obj, seg_start, pos);
      }
      pos++;

      size_t close = find_close_bracket_obj(ops, interp, str_obj, pos, len);
      if (close >= len) {
        FeatherObj msg = ops->string.intern(interp, "missing close-bracket", 21);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      FeatherObj cmd_script = ops->string.slice(interp, str_obj, pos, close);
      FeatherResult eval_result = feather_script_eval_obj(ops, interp, cmd_script, TCL_EVAL_LOCAL);

      if (eval_result == TCL_BREAK) {
        if (ops->list.is_nil(interp, result)) {
          result = ops->string.intern(interp, "", 0);
        }
        ops->interp.set_result(interp, result);
        return TCL_OK;
      } else if (eval_result == TCL_CONTINUE) {
        pos = close + 1;
        seg_start = pos;
        continue;
      } else if (eval_result == TCL_RETURN) {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          result = append_obj(ops, interp, result, cmd_result);
        }
        pos = close + 1;
        seg_start = pos;
      } else if (eval_result == TCL_ERROR) {
        return TCL_ERROR;
      } else if (eval_result >= 5) {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          result = append_obj(ops, interp, result, cmd_result);
        }
        pos = close + 1;
        seg_start = pos;
      } else {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          result = append_obj(ops, interp, result, cmd_result);
        }
        pos = close + 1;
        seg_start = pos;
      }

    } else {
      pos++;
    }
  }

  if (pos > seg_start) {
    result = append_slice(ops, interp, result, str_obj, seg_start, pos);
  }

  if (ops->list.is_nil(interp, result)) {
    result = ops->string.intern(interp, "", 0);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_subst_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Perform backslash, command, and variable substitutions",
    "Performs backslash, command, and variable substitutions on string and returns "
    "the fully-substituted result. The substitutions are performed in exactly the "
    "same way that they would be performed by the TCL parser on a script.\n\n"
    "Backslash substitution replaces backslash sequences with their corresponding "
    "characters (such as \\n for newline, \\t for tab, \\xNN for hex codes, \\uNNNN "
    "for 16-bit Unicode, and \\UNNNNNNNN for 32-bit Unicode).\n\n"
    "Command substitution replaces bracketed commands [cmd] with their results. "
    "If a command substitution encounters break, the substitution stops and returns "
    "the result accumulated so far. If it encounters continue, an empty string is "
    "substituted for that command. If it encounters return or a custom return code, "
    "the returned value is substituted.\n\n"
    "Variable substitution replaces variable references ($varName, ${varName}, or "
    "$varName(index)) with their values. Note that the array-style syntax $varName(index) "
    "is processed by subst but Feather does not support TCL-style arrays as separate "
    "data structures.\n\n"
    "The optional switches control which substitutions are performed. If none are specified, "
    "all three types of substitutions are performed.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-nobackslashes?");
  e = feather_usage_help(ops, interp, e, "Disable backslash substitution");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-nocommands?");
  e = feather_usage_help(ops, interp, e, "Disable command substitution");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-novariables?");
  e = feather_usage_help(ops, interp, e, "Disable variable substitution");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<string>");
  e = feather_usage_help(ops, interp, e, "The string to perform substitutions on");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x 10\nsubst {The value is $x}",
    "Variable substitution:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "subst {2 + 2 = [expr {2 + 2}]}",
    "Command substitution:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "subst {Line 1\\nLine 2\\tTabbed}",
    "Backslash substitution:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "subst -nocommands {Value: $x [ignored]}",
    "Disable command substitution (brackets are literal):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "subst {Copyright \\u00A9 2026}",
    "Unicode escape (16-bit):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "subst", spec);
}
