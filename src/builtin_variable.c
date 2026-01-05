#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_variable(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"variable ?name value ...? name ?value?\"", 64);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get current namespace
  FeatherObj current_ns = ops->ns.current(interp);

  // Process name/value pairs
  FeatherObj args_copy = ops->list.from(interp, args);
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);

  while (ops->list.length(interp, args_copy) > 0) {
    FeatherObj name = ops->list.shift(interp, args_copy);

    // Check if there's a value
    int has_value = ops->list.length(interp, args_copy) > 0;
    FeatherObj value = 0;
    if (has_value) {
      value = ops->list.shift(interp, args_copy);
    }

    // Check if name is qualified
    if (feather_obj_is_qualified(ops, interp, name)) {
      // Qualified name - create link to variable in specified namespace
      FeatherObj target_ns, simple_name;
      feather_obj_split_command(ops, interp, name, &target_ns, &simple_name);

      // Handle nil namespace (means global)
      if (ops->list.is_nil(interp, target_ns)) {
        target_ns = globalNs;
      }

      // Check if target namespace exists
      if (!ops->ns.exists(interp, target_ns)) {
        FeatherObj msg = ops->string.intern(interp, "can't access \"", 14);
        msg = ops->string.concat(interp, msg, name);
        FeatherObj suffix = ops->string.intern(interp, "\": parent namespace doesn't exist", 33);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // If value provided, set it
      if (has_value) {
        FeatherResult res = feather_set_var(ops, interp, name, value);
        if (res != TCL_OK) {
          return res;
        }
      }

      // Create link from local simple_name to target namespace variable
      ops->var.link_ns(interp, simple_name, target_ns, simple_name);
    } else {
      // Unqualified name - create variable in current namespace
      if (has_value) {
        FeatherObj qualifiedName;
        if (ops->string.equal(interp, current_ns, globalNs)) {
          // In global namespace: ::name
          qualifiedName = ops->string.concat(interp, globalNs, name);
        } else {
          // In other namespace: ns::name
          FeatherObj sep = ops->string.intern(interp, "::", 2);
          qualifiedName = ops->string.concat(interp, current_ns, sep);
          qualifiedName = ops->string.concat(interp, qualifiedName, name);
        }
        // feather_set_var handles qualified names and fires traces
        FeatherResult res = feather_set_var(ops, interp, qualifiedName, value);
        if (res != TCL_OK) {
          return res;  // Write trace error already set
        }
      }

      // Create link from local variable to namespace variable
      ops->var.link_ns(interp, name, current_ns, name);
    }
  }

  // Return empty result
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

void feather_register_variable_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description
  FeatherObj e = feather_usage_about(ops, interp,
    "Create and link namespace variables",
    "Creates namespace variables and links them to local procedure variables. "
    "Accepts one or more name/value pairs, where the final value is optional.\n\n"
    "When executed inside a procedure, variable creates a local variable linked "
    "to the namespace variable, making the namespace variable accessible by the "
    "local name.\n\n"
    "Qualified names (e.g., ::ns::varname) create links to variables in other "
    "namespaces. If the target namespace doesn't exist, an error is raised.\n\n"
    "Note: Feather does not support TCL's undefined variable state. Variables "
    "created without values may not behave exactly as in standard TCL.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Arguments
  e = feather_usage_arg(ops, interp, "<name>");
  e = feather_usage_help(ops, interp, e,
    "Variable name. May be simple (varname) or qualified (::ns::varname). "
    "When called inside a procedure, creates a local link to the namespace variable.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?value?");
  e = feather_usage_help(ops, interp, e,
    "Initial value for the variable. If provided, sets the namespace variable "
    "to this value. The final variable in the argument list may omit the value.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?name value ...?");
  e = feather_usage_help(ops, interp, e,
    "Additional name/value pairs. Each pair creates and initializes a namespace "
    "variable with a local link.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "namespace eval myns {\n"
    "    variable counter 0\n"
    "}",
    "Create a namespace variable with initial value:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc increment {} {\n"
    "    variable counter\n"
    "    incr counter\n"
    "}",
    "Link to namespace variable inside a procedure:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "variable name1 value1 name2 value2 name3",
    "Create multiple variables (last one without value):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc accessOther {} {\n"
    "    variable ::someNS::myvar\n"
    "    return $myvar\n"
    "}",
    "Link to variable in a different namespace using qualified name:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "variable", spec);
}
