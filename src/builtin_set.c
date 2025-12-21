#include "tclc.h"

TclResult tcl_builtin_set(const TclHostOps *ops, TclInterp interp,
                          TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    // No arguments - error
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the variable name (first argument)
  TclObj varName = ops->list.shift(interp, args);

  if (argc == 1) {
    // One argument: get variable value
    TclObj value = ops->var.get(interp, varName);
    if (ops->list.is_nil(interp, value)) {
      // Variable doesn't exist - build error message
      size_t nameLen;
      const char *nameStr = ops->string.get(interp, varName, &nameLen);

      // Build: can't read "X": no such variable
      // We need to concatenate strings
      TclObj part1 = ops->string.intern(interp, "can't read \"", 12);
      TclObj part2 = ops->string.intern(interp, nameStr, nameLen);
      TclObj part3 = ops->string.intern(interp, "\": no such variable", 19);

      TclObj msg = ops->string.concat(interp, part1, part2);
      msg = ops->string.concat(interp, msg, part3);

      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    ops->interp.set_result(interp, value);
    return TCL_OK;
  }

  // Two arguments: set variable value
  TclObj value = ops->list.shift(interp, args);
  ops->var.set(interp, varName, value);
  ops->interp.set_result(interp, value);
  return TCL_OK;
}
