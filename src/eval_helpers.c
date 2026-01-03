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
  feather_error_expected(ops, interp, "boolean value", resultObj);
  return TCL_ERROR;
}

/**
 * feather_foreach_impl implements shared foreach/lmap iteration logic.
 */
FeatherResult feather_foreach_impl(const FeatherHostOps *ops,
                                    FeatherInterp interp,
                                    FeatherObj args,
                                    const char *cmdName,
                                    FeatherIterCallback callback,
                                    void *ctx) {
  size_t argc = ops->list.length(interp, args);

  // Need at least: varlist list body
  // Or: varlist1 list1 varlist2 list2 ... body
  // Minimum is 3 args, and must be odd (pairs + body)
  if (argc < 3 || (argc % 2) == 0) {
    // Build error message using string builder
    FeatherObj builder = ops->string.builder_new(interp, 128);
    const char *msg1 = "wrong # args: should be \"";
    while (*msg1) {
      ops->string.builder_append_byte(interp, builder, *msg1++);
    }
    const char *cmd = cmdName;
    while (*cmd) {
      ops->string.builder_append_byte(interp, builder, *cmd++);
    }
    const char *msg2 = " varList list ?varList list ...? command\"";
    while (*msg2) {
      ops->string.builder_append_byte(interp, builder, *msg2++);
    }
    FeatherObj msg = ops->string.builder_finish(interp, builder);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Number of varlist/list pairs
  size_t numPairs = (argc - 1) / 2;
  FeatherObj body = ops->list.at(interp, args, argc - 1);

  // Validate all varlists are non-empty
  for (size_t i = 0; i < numPairs; i++) {
    FeatherObj varlist = ops->list.at(interp, args, i * 2);
    FeatherObj varlistAsList = ops->list.from(interp, varlist);
    if (ops->list.length(interp, varlistAsList) == 0) {
      // Build error message
      FeatherObj builder = ops->string.builder_new(interp, 64);
      const char *cmd = cmdName;
      while (*cmd) {
        ops->string.builder_append_byte(interp, builder, *cmd++);
      }
      const char *msg2 = " varlist is empty";
      while (*msg2) {
        ops->string.builder_append_byte(interp, builder, *msg2++);
      }
      FeatherObj msg = ops->string.builder_finish(interp, builder);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Calculate maximum iterations needed across all lists
  size_t maxIters = 0;
  for (size_t i = 0; i < numPairs; i++) {
    FeatherObj varlist = ops->list.at(interp, args, i * 2);
    FeatherObj varlistAsList = ops->list.from(interp, varlist);
    size_t numVars = ops->list.length(interp, varlistAsList);

    FeatherObj list = ops->list.at(interp, args, i * 2 + 1);
    FeatherObj listAsList = ops->list.from(interp, list);
    size_t listLen = ops->list.length(interp, listAsList);

    // Number of iterations for this pair: ceiling of listLen / numVars
    size_t iters = (listLen + numVars - 1) / numVars;
    if (iters > maxIters) {
      maxIters = iters;
    }
  }

  // Iterate
  FeatherObj emptyStr = ops->string.intern(interp, "", 0);

  for (size_t iter = 0; iter < maxIters; iter++) {
    // Set variables for each varlist/list pair
    for (size_t pair = 0; pair < numPairs; pair++) {
      FeatherObj varlist = ops->list.at(interp, args, pair * 2);
      FeatherObj varlistAsList = ops->list.from(interp, varlist);
      size_t numVars = ops->list.length(interp, varlistAsList);

      FeatherObj list = ops->list.at(interp, args, pair * 2 + 1);
      FeatherObj listAsList = ops->list.from(interp, list);
      size_t listLen = ops->list.length(interp, listAsList);

      for (size_t v = 0; v < numVars; v++) {
        FeatherObj varName = ops->list.at(interp, varlistAsList, v);
        size_t listIdx = iter * numVars + v;
        FeatherObj value;
        if (listIdx < listLen) {
          value = ops->list.at(interp, listAsList, listIdx);
        } else {
          value = emptyStr;
        }
        ops->var.set(interp, varName, value);
      }
    }

    // Execute body
    FeatherResult rc = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

    if (rc == TCL_BREAK) {
      // break - exit loop normally
      break;
    } else if (rc == TCL_CONTINUE) {
      // continue - go to next iteration (skip callback)
      continue;
    } else if (rc != TCL_OK) {
      // Error - propagate
      return rc;
    }

    // Call callback if provided (lmap uses this to accumulate results)
    if (callback != NULL) {
      FeatherObj bodyResult = ops->interp.get_result(interp);
      callback(ops, interp, bodyResult, ctx);
    }
  }

  return TCL_OK;
}
