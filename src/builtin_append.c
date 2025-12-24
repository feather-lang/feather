#include "tclc.h"
#include "internal.h"

TclResult tcl_builtin_append(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"append varName ?value ...?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);

  // Get current value or empty string
  TclObj current = ops->var.get(interp, varName);
  TclObj result;
  if (ops->list.is_nil(interp, current)) {
    result = ops->string.intern(interp, "", 0);
  } else {
    result = current;
  }

  // Append all values
  size_t numValues = ops->list.length(interp, args);
  for (size_t i = 0; i < numValues; i++) {
    TclObj value = ops->list.shift(interp, args);
    result = ops->string.concat(interp, result, value);
  }

  // Store back in variable
  ops->var.set(interp, varName, result);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
