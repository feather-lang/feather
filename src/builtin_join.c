#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_join(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"join list ?joinString?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Default separator is space
  const char *sep = " ";
  size_t sepLen = 1;
  if (argc == 2) {
    FeatherObj sepObj = ops->list.shift(interp, args);
    sep = ops->string.get(interp, sepObj, &sepLen);
  }

  // Handle empty list
  if (listLen == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Handle single element
  if (listLen == 1) {
    ops->interp.set_result(interp, ops->list.at(interp, list, 0));
    return TCL_OK;
  }

  // Build result by concatenating elements with separator
  FeatherObj result = ops->list.at(interp, list, 0);
  FeatherObj sepObj = ops->string.intern(interp, sep, sepLen);

  for (size_t i = 1; i < listLen; i++) {
    result = ops->string.concat(interp, result, sepObj);
    FeatherObj elem = ops->list.at(interp, list, i);
    result = ops->string.concat(interp, result, elem);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
