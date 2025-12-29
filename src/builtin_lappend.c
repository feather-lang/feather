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

  // Get current value or create empty list
  // feather_get_var handles qualified names and fires read traces
  FeatherObj current = feather_get_var(ops, interp, varName);

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
  // feather_set_var handles qualified names and fires write traces
  feather_set_var(ops, interp, varName, list);

  ops->interp.set_result(interp, list);
  return TCL_OK;
}
