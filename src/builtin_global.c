#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_global(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // global with no args is a no-op
  if (argc == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // global only has effect inside a proc (level > 0)
  // At level 0 (global scope), it's a no-op
  size_t level = ops->frame.level(interp);
  if (level == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  FeatherObj global_ns = ops->string.intern(interp, "::", 2);

  for (size_t i = 0; i < argc; i++) {
    FeatherObj varname = ops->list.at(interp, args, i);

    // Check for array element syntax - not allowed
    if (feather_obj_contains_char(ops, interp, varname, '(')) {
      FeatherObj msg = ops->string.intern(interp, "can't use \"", 11);
      msg = ops->string.concat(interp, msg, varname);
      FeatherObj suffix = ops->string.intern(interp, "\" as variable name: must be a scalar variable", 45);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Determine local name and target namespace/name
    // If qualified (contains ::), local name is the tail (unqualified part)
    // Target is the fully resolved namespace variable
    FeatherObj local_name;
    FeatherObj target_ns;
    FeatherObj target_name;

    if (feather_obj_is_qualified(ops, interp, varname)) {
      // Has namespace qualifiers - resolve it
      FeatherResult res = feather_obj_resolve_variable(ops, interp, varname,
                                                       &target_ns, &target_name);
      if (res != TCL_OK) {
        return res;
      }

      // Local name is just the tail (unqualified name)
      local_name = target_name;

      // If target_ns is nil, that means it was unqualified (shouldn't happen
      // since we checked feather_obj_is_qualified), use global
      if (ops->list.is_nil(interp, target_ns)) {
        target_ns = global_ns;
      }
    } else {
      // Unqualified - target is global namespace, both names are the same
      local_name = varname;
      target_ns = global_ns;
      target_name = varname;
    }

    // Create link from local variable to namespace variable
    ops->var.link_ns(interp, local_name, target_ns, target_name);
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

void feather_register_global_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Access global variables",
    "Creates links from local variables in a procedure to variables in the global namespace "
    "(or other namespaces if qualified names are used).\n\n"
    "This command has no effect unless executed inside a procedure body. When called at "
    "global scope or with no arguments, it is a no-op.\n\n"
    "If a variable name is namespace-qualified (contains ::), the link is created to that "
    "namespace variable, but the local variable name is just the unqualified tail.\n\n"
    "Note: Array element syntax like \"varName(index)\" is not supported and will cause an error.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?varName?...");
  e = feather_usage_help(ops, interp, e,
    "One or more variable names to link to the global (or specified) namespace. "
    "May be namespace-qualified (e.g., ::foo or ns::var).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc myProc {} {\n"
    "    global x y\n"
    "    set x 10\n"
    "}",
    "Link local variables x and y to global variables",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc myProc {} {\n"
    "    global ::ns::var\n"
    "    set var 42\n"
    "}",
    "Link local variable 'var' to namespace variable '::ns::var'",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "global", spec);
}
