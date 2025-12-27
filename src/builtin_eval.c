#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_eval(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp,
      "wrong # args: should be \"eval arg ?arg ...?\"", 44));
    return TCL_ERROR;
  }

  FeatherObj script;

  if (argc == 1) {
    script = ops->list.at(interp, args, 0);
  } else {
    FeatherObj concatArgs = ops->list.create(interp);
    for (size_t i = 0; i < argc; i++) {
      ops->list.push(interp, concatArgs, ops->list.at(interp, args, i));
    }
    FeatherResult res = feather_builtin_concat(ops, interp, cmd, concatArgs);
    if (res != TCL_OK) return res;
    script = ops->interp.get_result(interp);
  }

  size_t len;
  const char *src = ops->string.get(interp, script, &len);

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return feather_script_eval(ops, interp, src, len, 0);
}
