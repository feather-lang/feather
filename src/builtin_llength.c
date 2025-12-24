#include "feather.h"

FeatherResult feather_builtin_llength(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"llength list\"", 38);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj list = ops->list.shift(interp, args);

  // Convert to list and get length
  FeatherObj listCopy = ops->list.from(interp, list);
  size_t len = ops->list.length(interp, listCopy);

  FeatherObj result = ops->integer.create(interp, (int64_t)len);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
