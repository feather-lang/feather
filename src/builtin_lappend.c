#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_lappend(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lappend varName ?value ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  // Resolve the variable name (handles qualified names like ::varname)
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, varName, &ns, &localName);

  // Get current value or create empty list
  FeatherObj current;
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local lookup
    current = ops->var.get(interp, localName);
  } else {
    // Qualified - namespace lookup
    current = ops->ns.get_var(interp, ns, localName);
  }

  FeatherObj list;
  if (ops->list.is_nil(interp, current)) {
    list = ops->list.create(interp);
  } else {
    list = ops->list.from(interp, current);
  }

  // Append all values
  size_t numValues = ops->list.length(interp, args);
  for (size_t i = 0; i < numValues; i++) {
    FeatherObj value = ops->list.shift(interp, args);
    list = ops->list.push(interp, list, value);
  }

  // Store back in variable
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    ops->var.set(interp, localName, list);
  } else {
    // Qualified - namespace
    ops->ns.set_var(interp, ns, localName, list);
  }

  ops->interp.set_result(interp, list);
  return TCL_OK;
}
