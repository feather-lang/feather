#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_unset(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"unset ?-nocomplain? ?--? ?name ...?\"", 61);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  int nocomplain = 0;

  // Check for -nocomplain option
  while (ops->list.length(interp, args) > 0) {
    FeatherObj first = ops->list.at(interp, args, 0);

    if (feather_obj_eq_literal(ops, interp, first, "-nocomplain")) {
      nocomplain = 1;
      ops->list.shift(interp, args);
    } else if (feather_obj_eq_literal(ops, interp, first, "--")) {
      ops->list.shift(interp, args);
      break;
    } else {
      break;
    }
  }

  // Unset each variable
  size_t numVars = ops->list.length(interp, args);
  for (size_t i = 0; i < numVars; i++) {
    FeatherObj varName = ops->list.shift(interp, args);

    // Check if variable exists
    if (ops->var.exists(interp, varName) != TCL_OK) {
      if (!nocomplain) {
        FeatherObj msg = ops->string.intern(interp, "can't unset \"", 13);
        msg = ops->string.concat(interp, msg, varName);
        FeatherObj suffix = ops->string.intern(interp, "\": no such variable", 19);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    } else {
      ops->var.unset(interp, varName);
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}
