#include "feather.h"
#include "internal.h"

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

FeatherResult feather_builtin_lset(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lset varName index ?index ...? value\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Use at() for positional access to avoid mutation issues
  FeatherObj varName = ops->list.at(interp, args, 0);
  FeatherObj indexObj = ops->list.at(interp, args, 1);
  FeatherObj newValue = ops->list.at(interp, args, argc - 1);

  // Get current value - error if variable doesn't exist
  FeatherObj current = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, current)) {
    FeatherObj msg = ops->string.intern(interp, "can't read \"", 12);
    msg = ops->string.concat(interp, msg, varName);
    FeatherObj suffix = ops->string.intern(interp, "\": no such variable", 19);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // For now, support single index only (not nested)

  // Convert to list
  FeatherObj list = ops->list.from(interp, current);
  size_t listLen = ops->list.length(interp, list);

  // Parse index
  int64_t index;
  if (parse_index(ops, interp, indexObj, listLen, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  // Check bounds
  if (index < 0 || (size_t)index >= listLen) {
    FeatherObj msg = ops->string.intern(interp, "list index out of range", 23);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Use set_at for O(1) in-place modification
  if (ops->list.set_at(interp, list, (size_t)index, newValue) != TCL_OK) {
    FeatherObj msg = ops->string.intern(interp, "list index out of range", 23);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Store back in variable
  ops->var.set(interp, varName, list);
  ops->interp.set_result(interp, list);
  return TCL_OK;
}
