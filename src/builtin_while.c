#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_while(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"while test command\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj condition = ops->list.shift(interp, args);
  FeatherObj body = ops->list.shift(interp, args);

  while (1) {
    // Evaluate condition
    int condResult;
    FeatherResult rc = feather_eval_bool_condition(ops, interp, condition, &condResult);
    if (rc != TCL_OK) {
      return rc;
    }

    if (!condResult) {
      // Condition is false, exit loop
      break;
    }

    // Execute body as a script
    rc = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

    if (rc == TCL_BREAK) {
      // break was invoked - exit loop normally
      break;
    } else if (rc == TCL_CONTINUE) {
      // continue was invoked - go to next iteration
      continue;
    } else if (rc != TCL_OK) {
      // Error occurred - propagate
      return rc;
    }
  }

  // while returns empty string on normal completion
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}
