#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_incr(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"incr varName ?increment?\"", 50);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  // Get current value
  // feather_get_var handles qualified names and fires read traces
  FeatherObj currentVal;
  FeatherResult res = feather_get_var(ops, interp, varName, &currentVal);
  if (res != TCL_OK) {
    return res;  // Read trace error already set
  }

  if (ops->list.is_nil(interp, currentVal)) {
    FeatherObj part1 = ops->string.intern(interp, "can't read \"", 12);
    FeatherObj part3 = ops->string.intern(interp, "\": no such variable", 19);
    FeatherObj msg = ops->string.concat(interp, part1, varName);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Convert current value to integer
  int64_t current;
  if (ops->integer.get(interp, currentVal, &current) != TCL_OK) {
    FeatherObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
    FeatherObj part3 = ops->string.intern(interp, "\"", 1);
    FeatherObj msg = ops->string.concat(interp, part1, currentVal);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get increment (default 1)
  int64_t increment = 1;
  if (argc == 2) {
    FeatherObj incrVal = ops->list.shift(interp, args);
    if (ops->integer.get(interp, incrVal, &increment) != TCL_OK) {
      FeatherObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
      FeatherObj part3 = ops->string.intern(interp, "\"", 1);
      FeatherObj msg = ops->string.concat(interp, part1, incrVal);
      msg = ops->string.concat(interp, msg, part3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Compute new value and store
  int64_t newVal = current + increment;
  FeatherObj newObj = ops->integer.create(interp, newVal);

  // Store back in variable
  // feather_set_var handles qualified names and fires write traces
  res = feather_set_var(ops, interp, varName, newObj);
  if (res != TCL_OK) {
    return res;  // Write trace error already set
  }

  ops->interp.set_result(interp, newObj);
  return TCL_OK;
}
