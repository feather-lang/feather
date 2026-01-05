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

void feather_register_incr_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Increment the value of a variable",
    "Increments the value of the variable named varName by increment. If "
    "increment is not specified, the variable is incremented by 1. Both the "
    "variable's current value and the increment must be integers. If the "
    "variable does not exist, it is automatically initialized to 0 before "
    "incrementing.\n\n"
    "The new value is stored in the variable and also returned as the result "
    "of this command. The variable name may be a namespace-qualified name.\n\n"
    "Note: Feather does not support TCL-style arrays. The varName must refer "
    "to a scalar variable. Array syntax like \"myArray(key)\" is not supported.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<varName>");
  e = feather_usage_help(ops, interp, e, "The name of the variable to increment. May be namespace-qualified.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?increment?");
  e = feather_usage_help(ops, interp, e, "The amount to add to the variable (default: 1). May be negative to decrement.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x 5\n"
    "incr x",
    "Increment x by 1, returns 6",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set count 10\n"
    "incr count 5",
    "Increment count by 5, returns 15",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "incr total -3",
    "Decrement total by 3 (negative increment)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "incr uninitialized",
    "Auto-initializes uninitialized variable to 0, then increments to 1",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "incr value 0",
    "Validate that value contains an integer (zero increment)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "incr", spec);
}
