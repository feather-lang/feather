#include "tclc.h"

TclResult tcl_builtin_incr(const TclHostOps *ops, TclInterp interp,
                            TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"incr varName ?increment?\"", 50);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get variable name
  TclObj varName = ops->list.shift(interp, args);

  // Get current value
  TclObj currentVal = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, currentVal)) {
    // Variable doesn't exist
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, varName, &nameLen);
    TclObj part1 = ops->string.intern(interp, "can't read \"", 12);
    TclObj part2 = ops->string.intern(interp, nameStr, nameLen);
    TclObj part3 = ops->string.intern(interp, "\": no such variable", 19);
    TclObj msg = ops->string.concat(interp, part1, part2);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Convert current value to integer
  int64_t current;
  if (ops->integer.get(interp, currentVal, &current) != TCL_OK) {
    size_t valLen;
    const char *valStr = ops->string.get(interp, currentVal, &valLen);
    TclObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
    TclObj part2 = ops->string.intern(interp, valStr, valLen);
    TclObj part3 = ops->string.intern(interp, "\"", 1);
    TclObj msg = ops->string.concat(interp, part1, part2);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get increment (default 1)
  int64_t increment = 1;
  if (argc == 2) {
    TclObj incrVal = ops->list.shift(interp, args);
    if (ops->integer.get(interp, incrVal, &increment) != TCL_OK) {
      size_t valLen;
      const char *valStr = ops->string.get(interp, incrVal, &valLen);
      TclObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
      TclObj part2 = ops->string.intern(interp, valStr, valLen);
      TclObj part3 = ops->string.intern(interp, "\"", 1);
      TclObj msg = ops->string.concat(interp, part1, part2);
      msg = ops->string.concat(interp, msg, part3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Compute new value and store
  int64_t newVal = current + increment;
  TclObj newObj = ops->integer.create(interp, newVal);
  ops->var.set(interp, varName, newObj);
  ops->interp.set_result(interp, newObj);
  return TCL_OK;
}
