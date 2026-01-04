#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_lassign(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"lassign list ?varName ...?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listArg = ops->list.at(interp, args, 0);
  FeatherObj list = ops->list.from(interp, listArg);
  if (list == 0) {
    return TCL_ERROR;
  }
  size_t listLen = ops->list.length(interp, list);

  size_t numVars = argc - 1;
  FeatherObj emptyStr = ops->string.intern(interp, "", 0);

  for (size_t i = 0; i < numVars; i++) {
    FeatherObj varName = ops->list.at(interp, args, i + 1);
    FeatherObj value;
    if (i < listLen) {
      value = ops->list.at(interp, list, i);
    } else {
      value = emptyStr;
    }
    if (feather_set_var(ops, interp, varName, value) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  if (numVars >= listLen) {
    ops->interp.set_result(interp, emptyStr);
  } else {
    FeatherObj remaining = ops->list.create(interp);
    for (size_t i = numVars; i < listLen; i++) {
      ops->list.push(interp, remaining, ops->list.at(interp, list, i));
    }
    ops->interp.set_result(interp, remaining);
  }

  return TCL_OK;
}
