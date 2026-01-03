#include "feather.h"
#include "internal.h"

// Callback for lmap: appends body result to accumulator list
static void lmap_callback(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj bodyResult, void *ctx) {
  FeatherObj *result = (FeatherObj *)ctx;
  *result = ops->list.push(interp, *result, bodyResult);
}

FeatherResult feather_builtin_lmap(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  FeatherObj result = ops->list.create(interp);
  FeatherResult rc = feather_foreach_impl(ops, interp, args, "lmap", lmap_callback, &result);

  if (rc == TCL_OK) {
    // lmap returns the accumulated list
    ops->interp.set_result(interp, result);
  }

  return rc;
}
