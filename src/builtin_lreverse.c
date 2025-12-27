#include "feather.h"

FeatherResult feather_builtin_lreverse(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"lreverse list\"", 39);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listArg = ops->list.shift(interp, args);
  FeatherObj list = ops->list.from(interp, listArg);
  if (list == 0) {
    return TCL_ERROR;
  }

  size_t len = ops->list.length(interp, list);
  FeatherObj result = ops->list.create(interp);

  for (size_t i = len; i > 0; i--) {
    FeatherObj elem = ops->list.at(interp, list, i - 1);
    result = ops->list.push(interp, result, elem);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
