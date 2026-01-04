#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_set(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  if (argc == 1) {
    // One argument: get variable value
    // feather_get_var handles qualified names and fires traces
    FeatherObj value;
    FeatherResult res = feather_get_var(ops, interp, varName, &value);
    if (res != TCL_OK) {
      return res;  // Read trace error already set
    }

    if (ops->list.is_nil(interp, value)) {
      FeatherObj part1 = ops->string.intern(interp, "can't read \"", 12);
      FeatherObj part3 = ops->string.intern(interp, "\": no such variable", 19);
      FeatherObj msg = ops->string.concat(interp, part1, varName);
      msg = ops->string.concat(interp, msg, part3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    ops->interp.set_result(interp, value);
    return TCL_OK;
  }

  if (argc > 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Two arguments: set variable value
  // feather_set_var handles qualified names and fires traces
  FeatherObj value = ops->list.shift(interp, args);
  FeatherResult res = feather_set_var(ops, interp, varName, value);
  if (res != TCL_OK) {
    return res;  // Write trace error already set
  }

  ops->interp.set_result(interp, value);
  return TCL_OK;
}

void feather_register_set_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Required argument: varName
  FeatherObj e = feather_usage_arg(ops, interp, "<varName>");
  e = feather_usage_help(ops, interp, e, "Name of the variable to read or write");
  e = feather_usage_long_help(ops, interp, e,
    "The variable name. If not fully qualified (starting with ::), "
    "the variable is looked up first in the current namespace, then in the global namespace.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Optional argument: value
  e = feather_usage_arg(ops, interp, "?value?");
  e = feather_usage_help(ops, interp, e, "Value to store in the variable");
  e = feather_usage_long_help(ops, interp, e,
    "If provided, assigns this value to the variable and returns it. "
    "If omitted, returns the current value of the variable.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "set greeting hello",
    "Assign a value",
    "Sets the variable 'greeting' to 'hello' and returns 'hello'.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set greeting",
    "Read a value",
    "Returns the current value of the variable 'greeting'.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x [expr {$a + $b}]",
    "Store computed value",
    "Evaluates the expression and stores the result in 'x'.");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "set", spec);
}
