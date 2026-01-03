#include "feather.h"
#include "internal.h"

/**
 * feather_eval_bool_condition evaluates an expression and converts to boolean.
 */
FeatherResult feather_eval_bool_condition(const FeatherHostOps *ops,
                                           FeatherInterp interp,
                                           FeatherObj condition,
                                           int *result) {
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
