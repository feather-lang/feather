#include "feather.h"
#include "internal.h"
#include "error_trace.h"

// Helper macro
#define S(lit) (lit), feather_strlen(lit)

// Parse return code from string: ok=0, error=1, return=2, break=3, continue=4
// Returns -1 if invalid
static int try_parse_code(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj codeObj) {
  // Try as integer first
  int64_t intVal;
  if (ops->integer.get(interp, codeObj, &intVal) == TCL_OK) {
    return (int)intVal;
  }

  // Check named codes using feather_obj_eq_literal
  if (feather_obj_eq_literal(ops, interp, codeObj, "ok")) return 0;
  if (feather_obj_eq_literal(ops, interp, codeObj, "error")) return 1;
  if (feather_obj_eq_literal(ops, interp, codeObj, "return")) return 2;
  if (feather_obj_eq_literal(ops, interp, codeObj, "break")) return 3;
  if (feather_obj_eq_literal(ops, interp, codeObj, "continue")) return 4;

  return -1; // invalid
}

// Check if errorcode list matches pattern prefix
// pattern {A B} matches errorcode {A B C} or {A B}
// pattern {} matches everything
static int match_errorcode(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj pattern, FeatherObj errorcode) {
  FeatherObj patList = ops->list.from(interp, pattern);
  FeatherObj errList = ops->list.from(interp, errorcode);

  size_t patLen = ops->list.length(interp, patList);
  size_t errLen = ops->list.length(interp, errList);

  // Pattern must not be longer than errorcode
  if (patLen > errLen) return 0;

  // Each element of pattern must match corresponding element of errorcode
  for (size_t i = 0; i < patLen; i++) {
    FeatherObj patElem = ops->list.at(interp, patList, i);
    FeatherObj errElem = ops->list.at(interp, errList, i);

    if (ops->string.compare(interp, patElem, errElem) != 0) {
      return 0;
    }
  }

  return 1;
}

// Get -errorcode from options dict (format: {-code X -errorcode Y ...})
static FeatherObj get_errorcode(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj options) {
  if (ops->list.is_nil(interp, options)) {
    return ops->string.intern(interp, S(""));
  }

  FeatherObj optsCopy = ops->list.from(interp, options);
  size_t optsLen = ops->list.length(interp, optsCopy);

  for (size_t i = 0; i + 1 < optsLen; i += 2) {
    FeatherObj key = ops->list.at(interp, optsCopy, i);
    FeatherObj val = ops->list.at(interp, optsCopy, i + 1);

    if (feather_obj_eq_literal(ops, interp, key, "-errorcode")) {
      return val;
    }
  }

  return ops->string.intern(interp, S(""));
}

// Check if a script is the fallthrough marker "-"
static int is_fallthrough(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj script) {
  return feather_obj_eq_literal(ops, interp, script, "-");
}

// Add -during key to current return options
// duringOptions is the original exception's options dict to be stored under -during
static void add_during_to_options(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherResult code, FeatherObj duringOptions) {
  // Get current return options
  FeatherObj currentOpts = ops->interp.get_return_options(interp, code);

  // Build new options with -during appended
  FeatherObj newOpts = ops->list.create(interp);

  // Copy existing options if any
  if (!ops->list.is_nil(interp, currentOpts)) {
    FeatherObj optsCopy = ops->list.from(interp, currentOpts);
    size_t optsLen = ops->list.length(interp, optsCopy);
    for (size_t i = 0; i < optsLen; i++) {
      FeatherObj elem = ops->list.at(interp, optsCopy, i);
      newOpts = ops->list.push(interp, newOpts, elem);
    }
  } else {
    // No options exist, add basic -code
    newOpts = ops->list.push(interp, newOpts, ops->string.intern(interp, S("-code")));
    newOpts = ops->list.push(interp, newOpts, ops->integer.create(interp, (int64_t)code));
  }

  // Append -during key and value
  newOpts = ops->list.push(interp, newOpts, ops->string.intern(interp, S("-during")));

  // duringOptions might be nil for successful body, so provide default
  FeatherObj duringValue = duringOptions;
  if (ops->list.is_nil(interp, duringValue)) {
    duringValue = ops->list.create(interp);
    duringValue = ops->list.push(interp, duringValue, ops->string.intern(interp, S("-code")));
    duringValue = ops->list.push(interp, duringValue, ops->integer.create(interp, 0));
  }
  newOpts = ops->list.push(interp, newOpts, duringValue);

  // Set the modified options
  ops->interp.set_return_options(interp, newOpts);
}

FeatherResult feather_builtin_try(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // try body ?handler...? ?finally script?
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"try body ?handler ...? ?finally script?\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the body
  FeatherObj body = ops->list.at(interp, args, 0);

  // Parse handlers and finally clause
  // First, find the finally clause if present
  FeatherObj finallyScript = 0; // nil
  size_t handlerEnd = argc;

  // Check for finally at the end
  if (argc >= 2) {
    FeatherObj lastArg = ops->list.at(interp, args, argc - 1);

    // Check if second-to-last is "finally"
    if (argc >= 3) {
      FeatherObj secondLast = ops->list.at(interp, args, argc - 2);
      if (feather_obj_eq_literal(ops, interp, secondLast, "finally")) {
        finallyScript = lastArg;
        handlerEnd = argc - 2;
      }
    }

    // Check if last arg itself is "finally" (missing script)
    if (feather_obj_eq_literal(ops, interp, lastArg, "finally") && finallyScript == 0) {
      FeatherObj msg = ops->string.intern(
          interp, S("wrong # args to finally clause: must be \"finally script\""));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Evaluate the body
  FeatherResult bodyCode = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
  FeatherObj bodyResult = ops->interp.get_result(interp);
  FeatherObj bodyOptions = ops->interp.get_return_options(interp, bodyCode);

  // Handle TCL_RETURN specially
  FeatherResult effectiveCode = bodyCode;
  if (bodyCode == TCL_RETURN) {
    // Parse -code and -level from options
    int returnCode = TCL_OK;
    int level = 1;

    if (!ops->list.is_nil(interp, bodyOptions)) {
      FeatherObj optsCopy = ops->list.from(interp, bodyOptions);
      size_t optsLen = ops->list.length(interp, optsCopy);

      for (size_t i = 0; i + 1 < optsLen; i += 2) {
        FeatherObj key = ops->list.at(interp, optsCopy, i);
        FeatherObj val = ops->list.at(interp, optsCopy, i + 1);

        if (feather_obj_eq_literal(ops, interp, key, "-code")) {
          int64_t intVal;
          if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
            returnCode = (int)intVal;
          }
        } else if (feather_obj_eq_literal(ops, interp, key, "-level")) {
          int64_t intVal;
          if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
            level = (int)intVal;
          }
        }
      }
    }

    // Decrement level
    level--;
    if (level <= 0) {
      effectiveCode = (FeatherResult)returnCode;
    }
  }

  // Finalize error state before matching handlers (transfers accumulated trace to opts)
  if (effectiveCode == TCL_ERROR && feather_error_is_active(ops, interp)) {
    feather_error_finalize(ops, interp);
    // Update bodyOptions to include the newly added error info
    bodyOptions = ops->interp.get_return_options(interp, effectiveCode);
  }

  // Try to find a matching handler
  FeatherResult handlerResult = effectiveCode;
  FeatherObj handlerResultObj = bodyResult;
  int handlerMatched = 0;

  // For fallthrough: save the varList from the first matching handler
  FeatherObj savedVarList = 0;
  int pendingFallthrough = 0;

  size_t i = 1;
  while (i < handlerEnd) {
    FeatherObj handlerType = ops->list.at(interp, args, i);

    if (feather_obj_eq_literal(ops, interp, handlerType, "on")) {
      // on code variableList script
      if (i + 3 > handlerEnd) {
        FeatherObj msg = ops->string.intern(
            interp, S("wrong # args to on clause: must be \"on code variableList script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      FeatherObj codeObj = ops->list.at(interp, args, i + 1);
      FeatherObj varList = ops->list.at(interp, args, i + 2);
      FeatherObj script = ops->list.at(interp, args, i + 3);

      // Parse the code
      int code = try_parse_code(ops, interp, codeObj);
      if (code < 0) {
        // Build error message
        FeatherObj prefix = ops->string.intern(interp, S("bad completion code \""));
        FeatherObj suffix = ops->string.intern(interp, S("\": must be ok, error, return, break, continue, or an integer"));
        FeatherObj msg = ops->string.concat(interp, prefix, codeObj);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Check varList length
      FeatherObj varListParsed = ops->list.from(interp, varList);
      size_t varListLen = ops->list.length(interp, varListParsed);
      if (varListLen > 2) {
        FeatherObj msg = ops->string.intern(
            interp, S("wrong # args: should be \"on code ?resultVar ?optionsVar?? script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Check if we have a pending fallthrough or this handler matches
      int thisMatches = (int)effectiveCode == code;

      if (pendingFallthrough || thisMatches) {
        // If this is a new match (not fallthrough), save the varList
        if (!pendingFallthrough && thisMatches) {
          savedVarList = varList;
        }

        // Check for fallthrough ("-")
        if (is_fallthrough(ops, interp, script)) {
          pendingFallthrough = 1;
          i += 4;
          continue;
        }

        // Use saved varList if we had a fallthrough, otherwise use this handler's varList
        FeatherObj useVarList = pendingFallthrough ? savedVarList : varList;
        FeatherObj useVarListParsed = ops->list.from(interp, useVarList);
        size_t useVarListLen = ops->list.length(interp, useVarListParsed);

        handlerMatched = 1;
        pendingFallthrough = 0;

        // Bind variables
        if (useVarListLen >= 1) {
          FeatherObj resultVar = ops->list.at(interp, useVarListParsed, 0);
          if (feather_set_var(ops, interp, resultVar, bodyResult) != TCL_OK) {
            return TCL_ERROR;
          }
        }
        if (useVarListLen >= 2) {
          FeatherObj optsVar = ops->list.at(interp, useVarListParsed, 1);
          if (ops->list.is_nil(interp, bodyOptions)) {
            FeatherObj defaultOpts = ops->list.create(interp);
            defaultOpts = ops->list.push(interp, defaultOpts, ops->string.intern(interp, S("-code")));
            defaultOpts = ops->list.push(interp, defaultOpts, ops->integer.create(interp, (int64_t)effectiveCode));
            if (feather_set_var(ops, interp, optsVar, defaultOpts) != TCL_OK) {
              return TCL_ERROR;
            }
          } else {
            if (feather_set_var(ops, interp, optsVar, bodyOptions) != TCL_OK) {
              return TCL_ERROR;
            }
          }
        }

        // Execute handler script
        handlerResult = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
        handlerResultObj = ops->interp.get_result(interp);

        // If handler raised an exception, add -during key with body's options
        if (handlerResult != TCL_OK) {
          add_during_to_options(ops, interp, handlerResult, bodyOptions);
        } else {
          // Handler succeeded - clear return options to avoid stale error state
          FeatherObj emptyOpts = ops->list.create(interp);
          ops->interp.set_return_options(interp, emptyOpts);
        }

        break; // Handler executed, stop searching
      }

      i += 4;
    } else if (feather_obj_eq_literal(ops, interp, handlerType, "trap")) {
      // trap pattern variableList script
      if (i + 3 > handlerEnd) {
        FeatherObj msg = ops->string.intern(
            interp, S("wrong # args to trap clause: must be \"trap pattern variableList script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      FeatherObj pattern = ops->list.at(interp, args, i + 1);
      FeatherObj varList = ops->list.at(interp, args, i + 2);
      FeatherObj script = ops->list.at(interp, args, i + 3);

      // Check varList length
      FeatherObj varListParsed = ops->list.from(interp, varList);
      size_t varListLen = ops->list.length(interp, varListParsed);
      if (varListLen > 2) {
        FeatherObj msg = ops->string.intern(
            interp, S("wrong # args: should be \"trap pattern ?resultVar ?optionsVar?? script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // trap only matches errors (code 1)
      int thisMatches = 0;
      if ((int)effectiveCode == TCL_ERROR) {
        // Get errorcode from options
        FeatherObj errorcode = get_errorcode(ops, interp, bodyOptions);
        thisMatches = match_errorcode(ops, interp, pattern, errorcode);
      }

      // Check if we have a pending fallthrough or this handler matches
      if (pendingFallthrough || thisMatches) {
        // If this is a new match (not fallthrough), save the varList
        if (!pendingFallthrough && thisMatches) {
          savedVarList = varList;
        }

        // Check for fallthrough ("-")
        if (is_fallthrough(ops, interp, script)) {
          pendingFallthrough = 1;
          i += 4;
          continue;
        }

        // Use saved varList if we had a fallthrough, otherwise use this handler's varList
        FeatherObj useVarList = pendingFallthrough ? savedVarList : varList;
        FeatherObj useVarListParsed = ops->list.from(interp, useVarList);
        size_t useVarListLen = ops->list.length(interp, useVarListParsed);

        handlerMatched = 1;
        pendingFallthrough = 0;

        // Bind variables
        if (useVarListLen >= 1) {
          FeatherObj resultVar = ops->list.at(interp, useVarListParsed, 0);
          if (feather_set_var(ops, interp, resultVar, bodyResult) != TCL_OK) {
            return TCL_ERROR;
          }
        }
        if (useVarListLen >= 2) {
          FeatherObj optsVar = ops->list.at(interp, useVarListParsed, 1);
          if (ops->list.is_nil(interp, bodyOptions)) {
            FeatherObj defaultOpts = ops->list.create(interp);
            defaultOpts = ops->list.push(interp, defaultOpts, ops->string.intern(interp, S("-code")));
            defaultOpts = ops->list.push(interp, defaultOpts, ops->integer.create(interp, 1));
            if (feather_set_var(ops, interp, optsVar, defaultOpts) != TCL_OK) {
              return TCL_ERROR;
            }
          } else {
            if (feather_set_var(ops, interp, optsVar, bodyOptions) != TCL_OK) {
              return TCL_ERROR;
            }
          }
        }

        // Execute handler script
        handlerResult = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
        handlerResultObj = ops->interp.get_result(interp);

        // If handler raised an exception, add -during key with body's options
        if (handlerResult != TCL_OK) {
          add_during_to_options(ops, interp, handlerResult, bodyOptions);
        } else {
          // Handler succeeded - clear return options to avoid stale error state
          FeatherObj emptyOpts = ops->list.create(interp);
          ops->interp.set_return_options(interp, emptyOpts);
        }

        break; // Handler executed, stop searching
      }

      i += 4;
    } else if (feather_obj_eq_literal(ops, interp, handlerType, "finally")) {
      // finally must be at the end - if we got here, it's malformed
      FeatherObj msg = ops->string.intern(
          interp, S("finally clause must be at the end"));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    } else {
      // Unknown handler type
      FeatherObj prefix = ops->string.intern(interp, S("bad handler type \""));
      FeatherObj suffix = ops->string.intern(interp, S("\": must be on, trap, or finally"));
      FeatherObj msg = ops->string.concat(interp, prefix, handlerType);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Execute finally clause if present
  FeatherResult finalResult = handlerResult;
  FeatherObj finalResultObj = handlerResultObj;

  if (!ops->list.is_nil(interp, finallyScript) && finallyScript != 0) {
    // Save the previous result's options before executing finally
    // This will be either the body's options (if no handler) or handler's result options
    FeatherObj previousOptions;
    if (handlerMatched) {
      // Handler was executed, get its result options
      previousOptions = ops->interp.get_return_options(interp, handlerResult);
    } else {
      // No handler, use body's options
      previousOptions = bodyOptions;
    }

    FeatherResult finallyCode = feather_script_eval_obj(ops, interp, finallyScript, TCL_EVAL_LOCAL);

    // If finally returns a non-ok result, add -during key
    if (finallyCode != TCL_OK) {
      add_during_to_options(ops, interp, finallyCode, previousOptions);
      return finallyCode;
    }
    // Otherwise, restore the previous result
    ops->interp.set_result(interp, finalResultObj);
  }

  // If no handler matched and body returned non-ok, propagate it
  if (!handlerMatched) {
    ops->interp.set_result(interp, bodyResult);
    return effectiveCode;
  }

  // Return handler's result
  ops->interp.set_result(interp, handlerResultObj);
  return handlerResult;
}

void feather_register_try_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Structured exception handling",
    "Evaluates body script and provides exception handling through optional "
    "handler clauses and cleanup through an optional finally clause.\n\n"
    "Handlers are matched in order. The first matching handler executes its "
    "script. If no handler matches, the body's result is propagated.\n\n"
    "The finally clause always executes, regardless of whether an exception "
    "occurred or a handler matched. If the finally script raises an exception, "
    "it replaces the previous result.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to evaluate");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?on code variableList script?");
  e = feather_usage_help(ops, interp, e,
    "Handler that matches specific completion codes. The code can be ok (0), "
    "error (1), return (2), break (3), continue (4), or an integer. "
    "The variableList can be empty, contain one variable name (for the result), "
    "or two variable names (for result and options dictionary). "
    "If script is \"-\", fall through to the next handler while using this "
    "handler's variable bindings.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?trap pattern variableList script?");
  e = feather_usage_help(ops, interp, e,
    "Handler that matches errors by -errorcode prefix. Only matches when "
    "completion code is error (1). The pattern is a list prefix that must "
    "match the beginning of the -errorcode value. An empty pattern {} matches "
    "all errors. Variable binding and fallthrough work the same as for on.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?finally script?");
  e = feather_usage_help(ops, interp, e,
    "Cleanup script that always executes after body and any handler. "
    "If finally raises an exception, it replaces the previous result. "
    "Otherwise, the result from the body or handler is preserved. "
    "Must appear at the end if present.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "try {\n"
    "    expr {1 / 0}\n"
    "} on error {result opts} {\n"
    "    puts \"caught: $result\"\n"
    "}",
    "Catch errors using on handler:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "try {\n"
    "    error \"custom error\" {} {MYAPP ERR01}\n"
    "} trap {MYAPP} {result} {\n"
    "    puts \"application error: $result\"\n"
    "}",
    "Match specific error codes using trap handler:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "try {\n"
    "    set f [open file.txt]\n"
    "    # process file\n"
    "} finally {\n"
    "    close $f\n"
    "}",
    "Ensure cleanup with finally clause:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "try {\n"
    "    risky_operation\n"
    "} trap {DB} - trap {NET} {err} {\n"
    "    # handle both DB and NET errors\n"
    "    log_error $err\n"
    "}",
    "Use fallthrough to handle multiple error types:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "try", spec);
}
