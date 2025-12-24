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

  // Get current value or empty string
  FeatherObj current = ops->var.get(interp, varName);
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
  ops->var.set(interp, varName, result);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
