#include "tclc.h"
#include "internal.h"

TclResult tcl_builtin_lappend(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lappend varName ?value ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);

  // Get current value or create empty list
  TclObj current = ops->var.get(interp, varName);
  TclObj list;
  if (ops->list.is_nil(interp, current)) {
    list = ops->list.create(interp);
  } else {
    list = ops->list.from(interp, current);
  }

  // Append all values
  size_t numValues = ops->list.length(interp, args);
  for (size_t i = 0; i < numValues; i++) {
    TclObj value = ops->list.shift(interp, args);
    list = ops->list.push(interp, list, value);
  }

  // Store back in variable
  ops->var.set(interp, varName, list);
  ops->interp.set_result(interp, list);
  return TCL_OK;
}
