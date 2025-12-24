#include "feather.h"

FeatherResult feather_builtin_list(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  // list command just returns its arguments as a proper list
  // The args are already a list, so we just need to return them
  ops->interp.set_result(interp, args);
  return TCL_OK;
}
