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
    "Create and initialize a namespace variable",
    "This command is normally used within a namespace eval command to create "
    "one or more variables within a namespace. Each variable name is initialized "
    "with value. The value for the last variable is optional.\n\n"
    "If a variable name does not exist, it is created. In this case, if value is "
    "specified, it is assigned to the newly created variable. If the variable "
    "already exists, it is set to value if value is specified or left unchanged "
    "if no value is given. Normally, name is unqualified (does not include the "
    "names of any containing namespaces), and the variable is created in the "
    "current namespace. If name includes any namespace qualifiers, the variable "
    "is created in the specified namespace.\n\n"
    "If the variable command is executed inside a procedure, it creates local "
    "variables linked to the corresponding namespace variables (and therefore "
    "these variables are listed by info vars). In this way the variable command "
    "resembles the global command, although the global command resolves variable "
    "names with respect to the global namespace instead of the current namespace "
    "of the procedure. If any values are given, they are used to modify the "
    "values of the associated namespace variables. If a namespace variable does "
    "not exist, it is created and optionally initialized.\n\n"
    "Note: Feather does not support TCL's undefined variable state where variables "
    "are visible to namespace which but not to info exists. Variables created "
    "without values may not behave exactly as in standard TCL. Feather also does "
    "not support TCL-style arrays, so name cannot reference an array element.");
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

  // See Also
  e = feather_usage_section(ops, interp, "See Also",
    "global, namespace, upvar");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "variable", spec);
}
