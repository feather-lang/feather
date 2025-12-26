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
    size_t opt_len;
    const char *opt_str = ops->string.get(interp, opt, &opt_len);

    if (opt_len > 0 && opt_str[0] == '-') {
      if (feather_str_eq(opt_str, opt_len, "-nobackslashes")) {
        flags &= ~TCL_SUBST_BACKSLASHES;
      } else if (feather_str_eq(opt_str, opt_len, "-nocommands")) {
        flags &= ~TCL_SUBST_COMMANDS;
      } else if (feather_str_eq(opt_str, opt_len, "-novariables")) {
        flags &= ~TCL_SUBST_VARIABLES;
      } else {
        char errbuf[128];
        size_t err_len = 0;
        const char *prefix = "bad option \"";
        for (size_t j = 0; prefix[j]; j++) errbuf[err_len++] = prefix[j];
        for (size_t j = 0; j < opt_len && err_len < 100; j++) errbuf[err_len++] = opt_str[j];
        const char *suffix = "\": must be -nobackslashes, -nocommands, or -novariables";
        for (size_t j = 0; suffix[j] && err_len < 127; j++) errbuf[err_len++] = suffix[j];
        ops->interp.set_result(interp, ops->string.intern(interp, errbuf, err_len));
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
          char errbuf[128];
          size_t err_len = 0;
          const char *prefix = "can't read \"";
          for (size_t j = 0; prefix[j]; j++) errbuf[err_len++] = prefix[j];
          for (size_t j = 0; j < name_len && err_len < 100; j++) errbuf[err_len++] = name_start[j];
          const char *suffix = "\": no such variable";
          for (size_t j = 0; suffix[j] && err_len < 127; j++) errbuf[err_len++] = suffix[j];
          ops->interp.set_result(interp, ops->string.intern(interp, errbuf, err_len));
          return TCL_ERROR;
        }
        size_t val_len;
        const char *val_str = ops->string.get(interp, value, &val_len);
        result = append_str(ops, interp, result, val_str, val_len);
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
            size_t subst_len;
            const char *subst_str = ops->string.get(interp, subst_idx, &subst_len);

            char fullname[512];
            size_t fn_len = 0;
            for (size_t j = 0; j < name_len && fn_len < 500; j++) fullname[fn_len++] = name_start[j];
            fullname[fn_len++] = '(';
            for (size_t j = 0; j < subst_len && fn_len < 510; j++) fullname[fn_len++] = subst_str[j];
            fullname[fn_len++] = ')';

            FeatherObj full_name = ops->string.intern(interp, fullname, fn_len);
            FeatherObj value = ops->var.get(interp, full_name);
            if (ops->list.is_nil(interp, value)) {
              char errbuf[128];
              size_t err_len = 0;
              const char *prefix = "can't read \"";
              for (size_t j = 0; prefix[j]; j++) errbuf[err_len++] = prefix[j];
              for (size_t j = 0; j < fn_len && err_len < 100; j++) errbuf[err_len++] = fullname[j];
              const char *suffix = "\": no such variable";
              for (size_t j = 0; suffix[j] && err_len < 127; j++) errbuf[err_len++] = suffix[j];
              ops->interp.set_result(interp, ops->string.intern(interp, errbuf, err_len));
              return TCL_ERROR;
            }
            size_t val_len;
            const char *val_str = ops->string.get(interp, value, &val_len);
            result = append_str(ops, interp, result, val_str, val_len);
          } else {
            char fullname[512];
            size_t fn_len = 0;
            for (size_t j = 0; j < name_len && fn_len < 500; j++) fullname[fn_len++] = name_start[j];
            fullname[fn_len++] = '(';
            for (size_t j = 0; j < idx_len && fn_len < 510; j++) fullname[fn_len++] = idx_start[j];
            fullname[fn_len++] = ')';

            FeatherObj full_name = ops->string.intern(interp, fullname, fn_len);
            FeatherObj value = ops->var.get(interp, full_name);
            if (ops->list.is_nil(interp, value)) {
              char errbuf[128];
              size_t err_len = 0;
              const char *prefix = "can't read \"";
              for (size_t j = 0; prefix[j]; j++) errbuf[err_len++] = prefix[j];
              for (size_t j = 0; j < fn_len && err_len < 100; j++) errbuf[err_len++] = fullname[j];
              const char *suffix = "\": no such variable";
              for (size_t j = 0; suffix[j] && err_len < 127; j++) errbuf[err_len++] = suffix[j];
              ops->interp.set_result(interp, ops->string.intern(interp, errbuf, err_len));
              return TCL_ERROR;
            }
            size_t val_len;
            const char *val_str = ops->string.get(interp, value, &val_len);
            result = append_str(ops, interp, result, val_str, val_len);
          }
        } else {
          FeatherObj name = ops->string.intern(interp, name_start, name_len);
          FeatherObj value = ops->var.get(interp, name);
          if (ops->list.is_nil(interp, value)) {
            char errbuf[128];
            size_t err_len = 0;
            const char *prefix = "can't read \"";
            for (size_t j = 0; prefix[j]; j++) errbuf[err_len++] = prefix[j];
            for (size_t j = 0; j < name_len && err_len < 100; j++) errbuf[err_len++] = name_start[j];
            const char *suffix = "\": no such variable";
            for (size_t j = 0; suffix[j] && err_len < 127; j++) errbuf[err_len++] = suffix[j];
            ops->interp.set_result(interp, ops->string.intern(interp, errbuf, err_len));
            return TCL_ERROR;
          }
          size_t val_len;
          const char *val_str = ops->string.get(interp, value, &val_len);
          result = append_str(ops, interp, result, val_str, val_len);
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
          size_t result_len;
          const char *result_str = ops->string.get(interp, cmd_result, &result_len);
          result = append_str(ops, interp, result, result_str, result_len);
        }
        p = close + 1;
        seg_start = p;
      } else if (eval_result == TCL_ERROR) {
        return TCL_ERROR;
      } else if (eval_result >= 5) {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          size_t result_len;
          const char *result_str = ops->string.get(interp, cmd_result, &result_len);
          result = append_str(ops, interp, result, result_str, result_len);
        }
        p = close + 1;
        seg_start = p;
      } else {
        FeatherObj cmd_result = ops->interp.get_result(interp);
        if (!ops->list.is_nil(interp, cmd_result)) {
          size_t result_len;
          const char *result_str = ops->string.get(interp, cmd_result, &result_len);
          result = append_str(ops, interp, result, result_str, result_len);
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
