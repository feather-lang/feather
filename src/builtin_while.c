#include "tclc.h"
#include "internal.h"

// Helper to evaluate a condition expression using expr
static TclResult eval_while_condition(const TclHostOps *ops, TclInterp interp,
                                       TclObj condition, int *result) {
  // Build args list with the condition for expr
  TclObj exprArgs = ops->list.create(interp);
  exprArgs = ops->list.push(interp, exprArgs, condition);

  // Call expr builtin
  TclObj exprCmd = ops->string.intern(interp, "expr", 4);
  TclResult rc = tcl_builtin_expr(ops, interp, exprCmd, exprArgs);
  if (rc != TCL_OK) {
    return rc;
  }

  TclObj resultObj = ops->interp.get_result(interp);

  // Check for boolean literals
  size_t len;
  const char *str = ops->string.get(interp, resultObj, &len);

  // Check for true/false/yes/no
  if (len == 4 && str[0] == 't' && str[1] == 'r' && str[2] == 'u' && str[3] == 'e') {
    *result = 1;
    return TCL_OK;
  }
  if (len == 5 && str[0] == 'f' && str[1] == 'a' && str[2] == 'l' && str[3] == 's' && str[4] == 'e') {
    *result = 0;
    return TCL_OK;
  }
  if (len == 3 && str[0] == 'y' && str[1] == 'e' && str[2] == 's') {
    *result = 1;
    return TCL_OK;
  }
  if (len == 2 && str[0] == 'n' && str[1] == 'o') {
    *result = 0;
    return TCL_OK;
  }

  // Try as integer
  int64_t intVal;
  if (ops->integer.get(interp, resultObj, &intVal) == TCL_OK) {
    *result = (intVal != 0) ? 1 : 0;
    return TCL_OK;
  }

  // Invalid boolean expression
  TclObj part1 = ops->string.intern(interp, "expected boolean value but got \"", 32);
  TclObj part2 = ops->string.intern(interp, str, len);
  TclObj part3 = ops->string.intern(interp, "\"", 1);
  TclObj msg = ops->string.concat(interp, part1, part2);
  msg = ops->string.concat(interp, msg, part3);
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}

TclResult tcl_builtin_while(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"while test command\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj condition = ops->list.shift(interp, args);
  TclObj body = ops->list.shift(interp, args);

  while (1) {
    // Evaluate condition
    int condResult;
    TclResult rc = eval_while_condition(ops, interp, condition, &condResult);
    if (rc != TCL_OK) {
      return rc;
    }

    if (!condResult) {
      // Condition is false, exit loop
      break;
    }

    // Execute body - get string and evaluate as script
    size_t bodyLen;
    const char *bodyStr = ops->string.get(interp, body, &bodyLen);
    rc = tcl_eval_string(ops, interp, bodyStr, bodyLen, TCL_EVAL_LOCAL);

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
