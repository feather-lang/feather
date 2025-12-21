#include "tclc.h"

TclResult tcl_builtin_break(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 0) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"break\"", 32);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_BREAK;
}
