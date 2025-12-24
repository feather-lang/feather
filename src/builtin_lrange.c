#include "feather.h"

// Parse an index like "end", "end-N", or integer
static FeatherResult parse_index(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj indexObj, size_t listLen, int64_t *out) {
  size_t len;
  const char *str = ops->string.get(interp, indexObj, &len);

  // Check for "end"
  if (len == 3 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd') {
    *out = (int64_t)listLen - 1;
    return TCL_OK;
  }

  // Check for "end-N"
  if (len > 4 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd' && str[3] == '-') {
    int64_t offset = 0;
    for (size_t i = 4; i < len; i++) {
      if (str[i] < '0' || str[i] > '9') {
        FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
        msg = ops->string.concat(interp, msg, indexObj);
        FeatherObj suffix = ops->string.intern(interp, "\"", 1);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      offset = offset * 10 + (str[i] - '0');
    }
    *out = (int64_t)listLen - 1 - offset;
    return TCL_OK;
  }

  // Try integer
  if (ops->integer.get(interp, indexObj, out) != TCL_OK) {
    FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
    msg = ops->string.concat(interp, msg, indexObj);
    FeatherObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  return TCL_OK;
}

FeatherResult feather_builtin_lrange(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lrange list first last\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj firstObj = ops->list.shift(interp, args);
  FeatherObj lastObj = ops->list.shift(interp, args);

  // Convert to list
  FeatherObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Parse indices
  int64_t first, last;
  if (parse_index(ops, interp, firstObj, listLen, &first) != TCL_OK) {
    return TCL_ERROR;
  }
  if (parse_index(ops, interp, lastObj, listLen, &last) != TCL_OK) {
    return TCL_ERROR;
  }

  // Clamp indices
  if (first < 0) first = 0;
  if (last >= (int64_t)listLen) last = (int64_t)listLen - 1;

  // If range is empty or invalid, return empty list
  if (first > last || listLen == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Use slice for efficient O(n) extraction where n is slice size
  FeatherObj result = ops->list.slice(interp, list, (size_t)first, (size_t)last);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
