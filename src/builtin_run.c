#include "tclc.h"
#include "internal.h"

// A simple test command that just evaluates its argument as a script
TclResult tcl_builtin_run(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"run script\"", 36);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj script = ops->list.shift(interp, args);

  size_t bodyLen;
  const char *bodyStr = ops->string.get(interp, script, &bodyLen);
  return tcl_eval_string(ops, interp, bodyStr, bodyLen, TCL_EVAL_LOCAL);
}
