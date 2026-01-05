#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_unset(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"unset ?-nocomplain? ?--? ?name ...?\"", 61);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  int nocomplain = 0;

  // Check for -nocomplain option
  while (ops->list.length(interp, args) > 0) {
    FeatherObj first = ops->list.at(interp, args, 0);

    if (feather_obj_eq_literal(ops, interp, first, "-nocomplain")) {
      nocomplain = 1;
      ops->list.shift(interp, args);
    } else if (feather_obj_eq_literal(ops, interp, first, "--")) {
      ops->list.shift(interp, args);
      break;
    } else {
      break;
    }
  }

  // Unset each variable
  size_t numVars = ops->list.length(interp, args);
  for (size_t i = 0; i < numVars; i++) {
    FeatherObj varName = ops->list.shift(interp, args);

    // Check if variable exists
    if (ops->var.exists(interp, varName) != TCL_OK) {
      if (!nocomplain) {
        FeatherObj msg = ops->string.intern(interp, "can't unset \"", 13);
        msg = ops->string.concat(interp, msg, varName);
        FeatherObj suffix = ops->string.intern(interp, "\": no such variable", 19);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    } else {
      feather_unset_var(ops, interp, varName);
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

void feather_register_unset_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Delete variables",
    "This command removes one or more variables. If an error occurs during "
    "variable deletion, any variables after the named one causing the error "
    "are not deleted. An error can occur when the named variable does not "
    "exist.\n\n"
    "Note: Feather does not support TCL-style arrays. Array syntax like "
    "\"myArray(key)\" is not supported.");
  spec = feather_usage_add(ops, interp, spec, e);

  // -nocomplain flag
  e = feather_usage_flag(ops, interp, "-nocomplain", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Suppress errors for non-existent variables. The option may not be "
    "abbreviated, in order to disambiguate it from possible variable names");
  spec = feather_usage_add(ops, interp, spec, e);

  // -- flag (end of options marker)
  e = feather_usage_flag(ops, interp, "--", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Indicates the end of the options. Use this if you wish to remove a "
    "variable with the same name as any of the options");
  spec = feather_usage_add(ops, interp, spec, e);

  // Variable names (zero or more)
  e = feather_usage_arg(ops, interp, "?name?...");
  e = feather_usage_help(ops, interp, e,
    "Zero or more variable names to delete");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "unset myVar",
    "Delete a single variable",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "unset var1 var2 var3",
    "Delete multiple variables at once",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "unset -nocomplain optionalVar",
    "Delete a variable that might not exist, without error",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "unset -- -nocomplain",
    "Delete a variable literally named \"-nocomplain\"",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "unset", spec);
}
