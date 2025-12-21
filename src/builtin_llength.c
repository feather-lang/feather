#include "tclc.h"

TclResult tcl_builtin_llength(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"llength list\"", 38);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj list = ops->list.shift(interp, args);

  // Convert to list and get length
  TclObj listCopy = ops->list.from(interp, list);
  size_t len = ops->list.length(interp, listCopy);

  TclObj result = ops->integer.create(interp, (int64_t)len);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
