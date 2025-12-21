#include "tclc.h"

TclResult tcl_builtin_lindex(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"lindex list index\"", 43);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj list = ops->list.shift(interp, args);
  TclObj indexObj = ops->list.shift(interp, args);

  // Convert index to integer
  int64_t index;
  if (ops->integer.get(interp, indexObj, &index) != TCL_OK) {
    size_t valLen;
    const char *valStr = ops->string.get(interp, indexObj, &valLen);
    TclObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
    TclObj part2 = ops->string.intern(interp, valStr, valLen);
    TclObj part3 = ops->string.intern(interp, "\"", 1);
    TclObj msg = ops->string.concat(interp, part1, part2);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Convert to list
  TclObj listCopy = ops->list.from(interp, list);

  // Out of bounds returns empty string
  if (index < 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  size_t len = ops->list.length(interp, listCopy);
  if ((size_t)index >= len) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Get element at index
  TclObj elem = ops->list.at(interp, listCopy, (size_t)index);
  if (ops->list.is_nil(interp, elem)) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  ops->interp.set_result(interp, elem);
  return TCL_OK;
}
