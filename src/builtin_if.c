#include "feather.h"
#include "internal.h"

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
    if (feather_obj_eq_literal(ops, interp, condition, "else")) {
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
    if (feather_obj_eq_literal(ops, interp, condition, "elseif")) {
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
    FeatherObj body;
    if (feather_obj_eq_literal(ops, interp, next, "then")) {
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
    FeatherResult rc = feather_eval_bool_condition(ops, interp, condition, &condResult);
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
