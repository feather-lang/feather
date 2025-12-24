#include "feather.h"
#include "internal.h"

// Helper to evaluate a condition expression using expr
static FeatherResult eval_condition(const FeatherHostOps *ops, FeatherInterp interp,
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
  FeatherObj part1 = ops->string.intern(interp, "expected boolean value but got \"", 32);
  FeatherObj part2 = ops->string.intern(interp, str, len);
  FeatherObj part3 = ops->string.intern(interp, "\"", 1);
  FeatherObj msg = ops->string.concat(interp, part1, part2);
  msg = ops->string.concat(interp, msg, part3);
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}

// Helper to check if a string equals a keyword
static int str_eq(const char *s, size_t len, const char *keyword) {
  size_t i = 0;
  while (i < len && keyword[i] && s[i] == keyword[i]) {
    i++;
  }
  return i == len && keyword[i] == '\0';
}

FeatherResult feather_builtin_if(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?\"", 92);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Make a copy of args since we'll be shifting
  FeatherObj argsCopy = ops->list.from(interp, args);

  while (ops->list.length(interp, argsCopy) > 0) {
    // Get condition
    FeatherObj condition = ops->list.shift(interp, argsCopy);

    // Check for 'else' keyword
    size_t condLen;
    const char *condStr = ops->string.get(interp, condition, &condLen);
    if (str_eq(condStr, condLen, "else")) {
      // else clause - must have body
      if (ops->list.length(interp, argsCopy) == 0) {
        FeatherObj msg = ops->string.intern(interp,
          "wrong # args: should be \"if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?\"", 92);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj body = ops->list.shift(interp, argsCopy);
      return feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
    }

    // Check for 'elseif' keyword
    if (str_eq(condStr, condLen, "elseif")) {
      // elseif - get the actual condition
      if (ops->list.length(interp, argsCopy) == 0) {
        FeatherObj msg = ops->string.intern(interp,
          "wrong # args: should be \"if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?\"", 92);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      condition = ops->list.shift(interp, argsCopy);
    }

    // Need body after condition
    if (ops->list.length(interp, argsCopy) == 0) {
      FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?\"", 92);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Check for optional 'then' keyword
    FeatherObj next = ops->list.shift(interp, argsCopy);
    size_t nextLen;
    const char *nextStr = ops->string.get(interp, next, &nextLen);
    FeatherObj body;
    if (str_eq(nextStr, nextLen, "then")) {
      // Skip 'then', get body
      if (ops->list.length(interp, argsCopy) == 0) {
        FeatherObj msg = ops->string.intern(interp,
          "wrong # args: should be \"if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?\"", 92);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      body = ops->list.shift(interp, argsCopy);
    } else {
      // 'next' is the body
      body = next;
    }

    // Evaluate condition
    int condResult;
    FeatherResult rc = eval_condition(ops, interp, condition, &condResult);
    if (rc != TCL_OK) {
      return rc;
    }

    if (condResult) {
      // Condition is true, execute body as script
      return feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
    }

    // Condition is false, continue to next clause (elseif/else)
    // argsCopy already points to the next element
  }

  // No condition matched and no else clause
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}
