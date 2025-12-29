#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_set(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    // No arguments - error
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the variable name (first argument)
  FeatherObj varName = ops->list.shift(interp, args);

  // Resolve the variable name (handles qualified names)
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, varName, &ns, &localName);

  if (argc == 1) {
    // One argument: get variable value
    FeatherObj value;
    if (ops->list.is_nil(interp, ns)) {
      // Unqualified - frame-local lookup (with trace firing)
      value = feather_get_var(ops, interp, localName);
    } else {
      // Qualified - namespace lookup
      value = ops->ns.get_var(interp, ns, localName);
      // Fire traces for qualified variable access
      feather_fire_var_traces(ops, interp, varName, "read");
    }

    if (ops->list.is_nil(interp, value)) {
      // Variable doesn't exist - build error message
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

  // Two arguments: set variable value
  if (argc > 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj value = ops->list.shift(interp, args);

  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local (with trace firing)
    feather_set_var(ops, interp, localName, value);
  } else {
    // Qualified - namespace
    ops->ns.set_var(interp, ns, localName, value);
    // Fire traces for qualified variable access
    feather_fire_var_traces(ops, interp, varName, "write");
  }

  ops->interp.set_result(interp, value);
  return TCL_OK;
}
