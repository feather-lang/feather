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

void feather_register_if_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Execute scripts conditionally",
    "The if command evaluates expr1 as an expression (using the same rules "
    "as the expr command). If the value is true (non-zero integer, or the "
    "literal strings \"true\" or \"yes\"), then body1 is executed by passing "
    "it to the interpreter. Otherwise, expr1 is false (0, \"false\", or "
    "\"no\"), and the command continues with the next clause.\n\n"
    "The optional then keyword is accepted for readability but has no effect.\n\n"
    "The elseif keyword introduces additional condition-body pairs. If the "
    "first condition is false, the elseif conditions are evaluated in order "
    "until one evaluates to true, at which point its corresponding body is "
    "executed.\n\n"
    "The optional else keyword introduces a final body that is executed if "
    "no previous condition was true. If no condition matches and there is no "
    "else clause, the if command returns an empty string.\n\n"
    "The return value of the if command is the result of the body script that "
    "was executed, or an empty string if no body was executed.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<expr1>");
  e = feather_usage_help(ops, interp, e,
    "Expression to evaluate. The expression is evaluated using the same rules "
    "as the expr command. A true value is any non-zero integer, or the literal "
    "strings \"true\" or \"yes\". A false value is 0, \"false\", or \"no\".");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?then?");
  e = feather_usage_help(ops, interp, e,
    "Optional keyword for readability. Has no effect on execution.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<body1>");
  e = feather_usage_help(ops, interp, e,
    "Script to execute if expr1 is true.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?elseif expr2 ?then? body2 ...?");
  e = feather_usage_help(ops, interp, e,
    "Zero or more additional condition-body pairs. Each elseif clause consists "
    "of the keyword \"elseif\", followed by an expression, optionally followed "
    "by the keyword \"then\", followed by a body script.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?else bodyN?");
  e = feather_usage_help(ops, interp, e,
    "Optional final clause. If present, bodyN is executed if no previous "
    "condition was true.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x 10\n"
    "if {$x > 5} {\n"
    "    puts \"x is greater than 5\"\n"
    "}",
    "Basic if statement with expression:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x 3\n"
    "if {$x > 10} then {\n"
    "    puts \"large\"\n"
    "} elseif {$x > 5} then {\n"
    "    puts \"medium\"\n"
    "} else {\n"
    "    puts \"small\"\n"
    "}",
    "If-elseif-else with optional then keywords:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set result [if {$x == 0} {\n"
    "    expr 1\n"
    "} else {\n"
    "    expr {1 / $x}\n"
    "}]",
    "Using if as an expression (returns the result of executed body):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "if true {\n"
    "    puts \"this always runs\"\n"
    "}",
    "Using boolean literals:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "if", spec);
}
