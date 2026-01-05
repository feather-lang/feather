#include "feather.h"
#include "internal.h"

void feather_register_append_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Append to variable",
    "Appends all of the value arguments to the current value of variable "
    "varName. If varName doesn't exist, it is created with the concatenation "
    "of all values as its value. The new value is stored in varName and "
    "returned as the result of this command.\n\n"
    "If no value arguments are provided, append returns the current value of "
    "the variable, or creates the variable with an empty string if it doesn't "
    "exist.\n\n"
    "This command provides an efficient way to build up strings. Using "
    "\"append x $y\" is more efficient than \"set x $x$y\" for long strings, "
    "as it may avoid copying the entire string.\n\n"
    "Note: Feather does not support TCL-style arrays. The varName must refer "
    "to a scalar variable.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<varName>");
  e = feather_usage_help(ops, interp, e,
    "Name of the variable to append to. May be qualified with namespace "
    "(e.g., ::foo::bar).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?value?...");
  e = feather_usage_help(ops, interp, e,
    "Zero or more values to append to the variable. All values are concatenated "
    "in order.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x \"hello\"\n"
    "append x \" \" \"world\"\n"
    "# Returns: hello world",
    "Append multiple values to an existing variable",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "append newVar \"first\" \"second\"\n"
    "# Returns: firstsecond\n"
    "# Creates newVar with value \"firstsecond\"",
    "Create a new variable with appended values",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x \"test\"\n"
    "append x\n"
    "# Returns: test",
    "Append with no values returns current value",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "append", spec);
}

FeatherResult feather_builtin_append(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"append varName ?value ...?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  // Get current value or empty string
  // feather_get_var handles qualified names and fires read traces
  // Note: for append, variable may not exist, so nil is ok (we create it)
  FeatherObj current;
  FeatherResult res = feather_get_var(ops, interp, varName, &current);
  if (res != TCL_OK) {
    return res;  // Read trace error already set
  }

  FeatherObj result;
  if (ops->list.is_nil(interp, current)) {
    result = ops->string.intern(interp, "", 0);
  } else {
    result = current;
  }

  // Append all values
  size_t numValues = ops->list.length(interp, args);
  for (size_t i = 0; i < numValues; i++) {
    FeatherObj value = ops->list.shift(interp, args);
    result = ops->string.concat(interp, result, value);
  }

  // Store back in variable
  // feather_set_var handles qualified names and fires write traces
  res = feather_set_var(ops, interp, varName, result);
  if (res != TCL_OK) {
    return res;  // Write trace error already set
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
