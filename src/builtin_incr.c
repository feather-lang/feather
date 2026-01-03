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

  // TCL 8.5+ auto-initializes unset variables to 0
  int64_t current = 0;
  if (!ops->list.is_nil(interp, currentVal)) {
    // Variable exists - convert to integer
    if (ops->integer.get(interp, currentVal, &current) != TCL_OK) {
      feather_error_expected(ops, interp, "integer", currentVal);
      return TCL_ERROR;
    }
  }
  // If variable doesn't exist (nil), current stays 0

  // Get increment (default 1)
  int64_t increment = 1;
  if (argc == 2) {
    FeatherObj incrVal = ops->list.shift(interp, args);
    if (ops->integer.get(interp, incrVal, &increment) != TCL_OK) {
      feather_error_expected(ops, interp, "integer", incrVal);
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
