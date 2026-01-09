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
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Trap and process errors and exceptions",
    "This command executes the script body and, depending on what the outcome "
    "of that script is (normal exit, error, or some other exceptional result), "
    "runs a handler script to deal with the case. Once that has all happened, "
    "if the finally clause is present, the script it includes will be run and "
    "the result of the handler (or the body if no handler matched) is allowed "
    "to continue to propagate. Note that the finally clause is processed even "
    "if an error occurs and irrespective of which, if any, handler is used.\n\n"
    "The handler clauses are each expressed as several words, and must have "
    "one of the following forms: on or trap. Note that handler clauses are "
    "matched against in order, and that the first matching one is always "
    "selected. At most one handler clause will be selected. As a consequence, "
    "an on error will mask any subsequent trap in the try. Also note that "
    "on error is equivalent to trap {}.\n\n"
    "If an exception (i.e. any non-ok result) occurs during the evaluation of "
    "either the handler or the finally clause, the original exception's status "
    "dictionary will be added to the new exception's status dictionary under "
    "the -during key.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to evaluate for exceptions");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?handler?...");
  e = feather_usage_help(ops, interp, e, "Zero or more on or trap handler clauses");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?finally script?");
  e = feather_usage_help(ops, interp, e, "Optional cleanup script that always runs");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Handler clause: on ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<code>");
  e = feather_usage_help(ops, interp, e, "Completion code to match");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<variableList>");
  e = feather_usage_help(ops, interp, e, "Variables for result and options");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<script>");
  e = feather_usage_help(ops, interp, e, "Handler script or \"-\" for fallthrough");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "on", subspec);
  e = feather_usage_help(ops, interp, e, "Handle exceptions by completion code");
  e = feather_usage_clause(ops, interp, e);
  e = feather_usage_long_help(ops, interp, e,
    "This clause matches if the evaluation of body completed with the exception "
    "code code. The code may be expressed as an integer or one of the following "
    "literal words: ok, error, return, break, or continue. Those literals "
    "correspond to the integers 0 through 4 respectively.\n\n"
    "The variableList is always interpreted as a list of variable names. If the "
    "first word of the list is present and non-empty, it names a variable into "
    "which the result of the evaluation of body will be placed; this will "
    "contain the human-readable form of any errors. If the second word of the "
    "list is present and non-empty, it names a variable into which the options "
    "dictionary of the interpreter at the moment of completion of execution of "
    "body will be placed.\n\n"
    "The script is also always interpreted the same: as a Tcl script to evaluate "
    "if the clause is matched. If script is a literal \"-\" and the handler is "
    "not the last one, the script of the following handler is invoked instead "
    "(just like with the switch command).");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Handler clause: trap ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<pattern>");
  e = feather_usage_help(ops, interp, e, "Error code prefix pattern to match");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<variableList>");
  e = feather_usage_help(ops, interp, e, "Variables for result and options");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<script>");
  e = feather_usage_help(ops, interp, e, "Handler script or \"-\" for fallthrough");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "trap", subspec);
  e = feather_usage_help(ops, interp, e, "Handle exceptions by error code pattern");
  e = feather_usage_clause(ops, interp, e);
  e = feather_usage_long_help(ops, interp, e,
    "This clause matches if the evaluation of body resulted in an error and the "
    "prefix of the -errorcode from the interpreter's status dictionary is equal "
    "to the pattern. The number of prefix words taken from the -errorcode is "
    "equal to the list-length of pattern, and inter-word spaces are normalized "
    "in both the -errorcode and pattern before comparison.\n\n"
    "The variableList and script words work the same as for the on clause. "
    "An empty pattern {} matches all errors, making trap {} equivalent to "
    "on error.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Clause: finally ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<script>");
  e = feather_usage_help(ops, interp, e, "Cleanup script to execute");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "finally", subspec);
  e = feather_usage_help(ops, interp, e, "Execute cleanup script unconditionally");
  e = feather_usage_clause(ops, interp, e);
  e = feather_usage_long_help(ops, interp, e,
    "The finally clause, if present, is always executed after the body and any "
    "matched handler have completed, regardless of whether an error occurred. "
    "This makes it ideal for cleanup operations like closing files or releasing "
    "resources.\n\n"
    "If the finally script raises an exception, it replaces the previous result "
    "(whether from the body or a handler). Otherwise, the result from the body "
    "or handler is preserved and propagated. The finally clause must appear at "
    "the end of the try command if present.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "set f [open /some/file/name a]\n"
    "try {\n"
    "    puts $f \"some message\"\n"
    "    # ...\n"
    "} finally {\n"
    "    close $f\n"
    "}",
    "Ensure that a file is closed no matter what:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "try {\n"
    "    set f [open /some/file/name r]\n"
    "} trap {POSIX EISDIR} {} {\n"
    "    puts \"failed to open: it's a directory\"\n"
    "} trap {POSIX ENOENT} {} {\n"
    "    puts \"failed to open: it doesn't exist\"\n"
    "}",
    "Handle different reasons for a file to not be openable:",
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

  e = feather_usage_section(ops, interp, "See Also",
    "catch, error, return, throw");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "try", spec);
}
