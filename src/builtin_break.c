#include "feather.h"

FeatherResult feather_builtin_break(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 0) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"break\"", 32);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_BREAK;
}
