#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_foreach(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  FeatherResult rc = feather_foreach_impl(ops, interp, args, "foreach", NULL, NULL);

  if (rc == TCL_OK) {
    // foreach returns empty string on normal completion
    FeatherObj emptyStr = ops->string.intern(interp, "", 0);
    ops->interp.set_result(interp, emptyStr);
  }

  return rc;
}
