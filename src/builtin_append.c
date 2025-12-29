#include "feather.h"
#include "internal.h"

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

  // Resolve the variable name (handles qualified names like ::varname)
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, varName, &ns, &localName);

  // Get current value or empty string
  FeatherObj current;
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local lookup
    current = ops->var.get(interp, localName);
  } else {
    // Qualified - namespace lookup
    current = ops->ns.get_var(interp, ns, localName);
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
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    ops->var.set(interp, localName, result);
  } else {
    // Qualified - namespace
    ops->ns.set_var(interp, ns, localName, result);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
