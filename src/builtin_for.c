#include "feather.h"
#include "internal.h"

// Helper to evaluate a condition expression using expr
// (Shared logic with while - could be factored out later)
static FeatherResult eval_for_condition(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj condition, int *result) {
  // Build args list with the condition for expr
  FeatherObj exprArgs = ops->list.create(interp);
  exprArgs = ops->list.push(interp, exprArgs, condition);

  // Call expr builtin
  FeatherObj exprCmd = ops->string.intern(interp, "expr", 4);
  FeatherResult rc = feather_builtin_expr(ops, interp, exprCmd, exprArgs);
  if (rc != TCL_OK) {
    return rc;
  }

  FeatherObj resultObj = ops->interp.get_result(interp);

  // Check for boolean literals using helper
  if (feather_obj_to_bool_literal(ops, interp, resultObj, result)) {
    return TCL_OK;
  }

  // Try as integer
  int64_t intVal;
  if (ops->integer.get(interp, resultObj, &intVal) == TCL_OK) {
    *result = (intVal != 0) ? 1 : 0;
    return TCL_OK;
  }

  // Invalid boolean expression
  FeatherObj part1 = ops->string.intern(interp, "expected boolean value but got \"", 32);
  FeatherObj part3 = ops->string.intern(interp, "\"", 1);
  FeatherObj msg = ops->string.concat(interp, part1, resultObj);
  msg = ops->string.concat(interp, msg, part3);
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}

FeatherResult feather_builtin_for(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 4) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"for start test next command\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj start = ops->list.at(interp, args, 0);
  FeatherObj test = ops->list.at(interp, args, 1);
  FeatherObj next = ops->list.at(interp, args, 2);
  FeatherObj body = ops->list.at(interp, args, 3);

  // Execute the init script
  FeatherResult rc = feather_script_eval_obj(ops, interp, start, TCL_EVAL_LOCAL);
  if (rc != TCL_OK) {
    return rc;
  }

  while (1) {
    // Evaluate condition
    int condResult;
    rc = eval_for_condition(ops, interp, test, &condResult);
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
      // continue was invoked - still execute 'next', then go to next iteration
    } else if (rc != TCL_OK) {
      // Error occurred - propagate
      return rc;
    }

    // Execute the 'next' script (increment/update)
    rc = feather_script_eval_obj(ops, interp, next, TCL_EVAL_LOCAL);
    if (rc == TCL_BREAK) {
      // break was invoked in next - exit loop normally
      break;
    } else if (rc != TCL_OK) {
      // Error or continue - propagate (continue in next is an error per TCL)
      return rc;
    }
  }

  // for returns empty string on normal completion
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}
