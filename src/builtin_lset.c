#include "tclc.h"
#include "internal.h"

// Parse an index like "end", "end-N", or integer
static TclResult parse_index(const TclHostOps *ops, TclInterp interp,
                             TclObj indexObj, size_t listLen, int64_t *out) {
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
        TclObj msg = ops->string.intern(interp, "bad index \"", 11);
        msg = ops->string.concat(interp, msg, indexObj);
        TclObj suffix = ops->string.intern(interp, "\"", 1);
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
    TclObj msg = ops->string.intern(interp, "bad index \"", 11);
    msg = ops->string.concat(interp, msg, indexObj);
    TclObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  return TCL_OK;
}

TclResult tcl_builtin_lset(const TclHostOps *ops, TclInterp interp,
                            TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lset varName index ?index ...? value\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Use at() for positional access to avoid mutation issues
  TclObj varName = ops->list.at(interp, args, 0);
  TclObj indexObj = ops->list.at(interp, args, 1);
  TclObj newValue = ops->list.at(interp, args, argc - 1);

  // Get current value - error if variable doesn't exist
  TclObj current = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, current)) {
    TclObj msg = ops->string.intern(interp, "can't read \"", 12);
    msg = ops->string.concat(interp, msg, varName);
    TclObj suffix = ops->string.intern(interp, "\": no such variable", 19);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // For now, support single index only (not nested)

  // Convert to list
  TclObj list = ops->list.from(interp, current);
  size_t listLen = ops->list.length(interp, list);

  // Parse index
  int64_t index;
  if (parse_index(ops, interp, indexObj, listLen, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  // Check bounds
  if (index < 0 || (size_t)index >= listLen) {
    TclObj msg = ops->string.intern(interp, "list index out of range", 23);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Rebuild list with new value at index
  TclObj result = ops->list.create(interp);
  for (size_t i = 0; i < listLen; i++) {
    if ((int64_t)i == index) {
      result = ops->list.push(interp, result, newValue);
    } else {
      TclObj elem = ops->list.at(interp, list, i);
      result = ops->list.push(interp, result, elem);
    }
  }

  // Store back in variable
  ops->var.set(interp, varName, result);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
