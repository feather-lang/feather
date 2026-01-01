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
  while (ops->list.length(interp, args_copy) > 0) {
    FeatherObj name = ops->list.shift(interp, args_copy);

    // Check if name is qualified - if so, it's an error
    if (feather_obj_is_qualified(ops, interp, name)) {
      FeatherObj msg = ops->string.intern(interp, "can't define \"", 14);
      msg = ops->string.concat(interp, msg, name);
      FeatherObj suffix = ops->string.intern(interp, "\": name refers to an element in another namespace", 49);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Check if there's a value
    int has_value = ops->list.length(interp, args_copy) > 0;
    FeatherObj value = 0;
    if (has_value) {
      value = ops->list.shift(interp, args_copy);
    }

    // If value provided, set it in namespace storage using qualified name
    // Construct fully qualified name: current_ns + :: + name (or just ::name for global)
    if (has_value) {
      FeatherObj qualifiedName;
      FeatherObj globalNs = ops->string.intern(interp, "::", 2);
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

  // Return empty result
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}
