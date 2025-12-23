#include "tclc.h"
#include "internal.h"

// Helper macro
#define S(lit) (lit), tcl_strlen(lit)

// Helper to check if a string equals a literal
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t lit_len = tcl_strlen(lit);
  if (len != lit_len) return 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) return 0;
  }
  return 1;
}

// Parse return code from string: ok=0, error=1, return=2, break=3, continue=4
// Returns -1 if invalid
static int parse_code(const TclHostOps *ops, TclInterp interp, TclObj codeObj) {
  // Try as integer first
  int64_t intVal;
  if (ops->integer.get(interp, codeObj, &intVal) == TCL_OK) {
    return (int)intVal;
  }

  size_t len;
  const char *str = ops->string.get(interp, codeObj, &len);

  if (str_eq(str, len, "ok")) return 0;
  if (str_eq(str, len, "error")) return 1;
  if (str_eq(str, len, "return")) return 2;
  if (str_eq(str, len, "break")) return 3;
  if (str_eq(str, len, "continue")) return 4;

  return -1; // invalid
}

// Check if errorcode list matches pattern prefix
// pattern {A B} matches errorcode {A B C} or {A B}
// pattern {} matches everything
static int match_errorcode(const TclHostOps *ops, TclInterp interp,
                           TclObj pattern, TclObj errorcode) {
  TclObj patList = ops->list.from(interp, pattern);
  TclObj errList = ops->list.from(interp, errorcode);

  size_t patLen = ops->list.length(interp, patList);
  size_t errLen = ops->list.length(interp, errList);

  // Pattern must not be longer than errorcode
  if (patLen > errLen) return 0;

  // Each element of pattern must match corresponding element of errorcode
  for (size_t i = 0; i < patLen; i++) {
    TclObj patElem = ops->list.at(interp, patList, i);
    TclObj errElem = ops->list.at(interp, errList, i);

    if (ops->string.compare(interp, patElem, errElem) != 0) {
      return 0;
    }
  }

  return 1;
}

// Get -errorcode from options dict (format: {-code X -errorcode Y ...})
static TclObj get_errorcode(const TclHostOps *ops, TclInterp interp, TclObj options) {
  if (ops->list.is_nil(interp, options)) {
    return ops->string.intern(interp, S(""));
  }

  TclObj optsCopy = ops->list.from(interp, options);
  size_t optsLen = ops->list.length(interp, optsCopy);

  for (size_t i = 0; i + 1 < optsLen; i += 2) {
    TclObj key = ops->list.at(interp, optsCopy, i);
    TclObj val = ops->list.at(interp, optsCopy, i + 1);

    size_t keyLen;
    const char *keyStr = ops->string.get(interp, key, &keyLen);

    if (str_eq(keyStr, keyLen, "-errorcode")) {
      return val;
    }
  }

  return ops->string.intern(interp, S(""));
}

// Check if a script is the fallthrough marker "-"
static int is_fallthrough(const TclHostOps *ops, TclInterp interp, TclObj script) {
  size_t len;
  const char *str = ops->string.get(interp, script, &len);
  return len == 1 && str[0] == '-';
}

TclResult tcl_builtin_try(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // try body ?handler...? ?finally script?
  if (argc < 1) {
    TclObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"try body ?handler ...? ?finally script?\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the body
  TclObj body = ops->list.at(interp, args, 0);

  // Parse handlers and finally clause
  // First, find the finally clause if present
  TclObj finallyScript = 0; // nil
  size_t handlerEnd = argc;

  // Check for finally at the end
  if (argc >= 2) {
    TclObj lastArg = ops->list.at(interp, args, argc - 1);
    size_t len;
    const char *str = ops->string.get(interp, lastArg, &len);

    // Check if second-to-last is "finally"
    if (argc >= 3) {
      TclObj secondLast = ops->list.at(interp, args, argc - 2);
      size_t slen;
      const char *sstr = ops->string.get(interp, secondLast, &slen);
      if (str_eq(sstr, slen, "finally")) {
        finallyScript = lastArg;
        handlerEnd = argc - 2;
      }
    }

    // Check if last arg itself is "finally" (missing script)
    if (str_eq(str, len, "finally") && finallyScript == 0) {
      TclObj msg = ops->string.intern(
          interp, S("wrong # args to finally clause: must be \"finally script\""));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Evaluate the body
  TclResult bodyCode = tcl_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
  TclObj bodyResult = ops->interp.get_result(interp);
  TclObj bodyOptions = ops->interp.get_return_options(interp, bodyCode);

  // Handle TCL_RETURN specially
  TclResult effectiveCode = bodyCode;
  if (bodyCode == TCL_RETURN) {
    // Parse -code and -level from options
    int returnCode = TCL_OK;
    int level = 1;

    if (!ops->list.is_nil(interp, bodyOptions)) {
      TclObj optsCopy = ops->list.from(interp, bodyOptions);
      size_t optsLen = ops->list.length(interp, optsCopy);

      for (size_t i = 0; i + 1 < optsLen; i += 2) {
        TclObj key = ops->list.at(interp, optsCopy, i);
        TclObj val = ops->list.at(interp, optsCopy, i + 1);

        size_t keyLen;
        const char *keyStr = ops->string.get(interp, key, &keyLen);

        if (str_eq(keyStr, keyLen, "-code")) {
          int64_t intVal;
          if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
            returnCode = (int)intVal;
          }
        } else if (str_eq(keyStr, keyLen, "-level")) {
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
      effectiveCode = (TclResult)returnCode;
    }
  }

  // Try to find a matching handler
  TclResult handlerResult = effectiveCode;
  TclObj handlerResultObj = bodyResult;
  int handlerMatched = 0;

  // For fallthrough: save the varList from the first matching handler
  TclObj savedVarList = 0;
  int pendingFallthrough = 0;

  size_t i = 1;
  while (i < handlerEnd) {
    TclObj handlerType = ops->list.at(interp, args, i);
    size_t typeLen;
    const char *typeStr = ops->string.get(interp, handlerType, &typeLen);

    if (str_eq(typeStr, typeLen, "on")) {
      // on code variableList script
      if (i + 3 > handlerEnd) {
        TclObj msg = ops->string.intern(
            interp, S("wrong # args to on clause: must be \"on code variableList script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      TclObj codeObj = ops->list.at(interp, args, i + 1);
      TclObj varList = ops->list.at(interp, args, i + 2);
      TclObj script = ops->list.at(interp, args, i + 3);

      // Parse the code
      int code = parse_code(ops, interp, codeObj);
      if (code < 0) {
        // Build error message
        TclObj prefix = ops->string.intern(interp, S("bad completion code \""));
        TclObj suffix = ops->string.intern(interp, S("\": must be ok, error, return, break, continue, or an integer"));
        TclObj msg = ops->string.concat(interp, prefix, codeObj);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Check varList length
      TclObj varListParsed = ops->list.from(interp, varList);
      size_t varListLen = ops->list.length(interp, varListParsed);
      if (varListLen > 2) {
        TclObj msg = ops->string.intern(
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
        TclObj useVarList = pendingFallthrough ? savedVarList : varList;
        TclObj useVarListParsed = ops->list.from(interp, useVarList);
        size_t useVarListLen = ops->list.length(interp, useVarListParsed);

        handlerMatched = 1;
        pendingFallthrough = 0;

        // Bind variables
        if (useVarListLen >= 1) {
          TclObj resultVar = ops->list.at(interp, useVarListParsed, 0);
          ops->var.set(interp, resultVar, bodyResult);
        }
        if (useVarListLen >= 2) {
          TclObj optsVar = ops->list.at(interp, useVarListParsed, 1);
          if (ops->list.is_nil(interp, bodyOptions)) {
            TclObj defaultOpts = ops->list.create(interp);
            defaultOpts = ops->list.push(interp, defaultOpts, ops->string.intern(interp, S("-code")));
            defaultOpts = ops->list.push(interp, defaultOpts, ops->integer.create(interp, (int64_t)effectiveCode));
            ops->var.set(interp, optsVar, defaultOpts);
          } else {
            ops->var.set(interp, optsVar, bodyOptions);
          }
        }

        // Execute handler script
        handlerResult = tcl_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
        handlerResultObj = ops->interp.get_result(interp);
        break; // Handler executed, stop searching
      }

      i += 4;
    } else if (str_eq(typeStr, typeLen, "trap")) {
      // trap pattern variableList script
      if (i + 3 > handlerEnd) {
        TclObj msg = ops->string.intern(
            interp, S("wrong # args to trap clause: must be \"trap pattern variableList script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      TclObj pattern = ops->list.at(interp, args, i + 1);
      TclObj varList = ops->list.at(interp, args, i + 2);
      TclObj script = ops->list.at(interp, args, i + 3);

      // Check varList length
      TclObj varListParsed = ops->list.from(interp, varList);
      size_t varListLen = ops->list.length(interp, varListParsed);
      if (varListLen > 2) {
        TclObj msg = ops->string.intern(
            interp, S("wrong # args: should be \"trap pattern ?resultVar ?optionsVar?? script\""));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // trap only matches errors (code 1)
      int thisMatches = 0;
      if ((int)effectiveCode == TCL_ERROR) {
        // Get errorcode from options
        TclObj errorcode = get_errorcode(ops, interp, bodyOptions);
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
        TclObj useVarList = pendingFallthrough ? savedVarList : varList;
        TclObj useVarListParsed = ops->list.from(interp, useVarList);
        size_t useVarListLen = ops->list.length(interp, useVarListParsed);

        handlerMatched = 1;
        pendingFallthrough = 0;

        // Bind variables
        if (useVarListLen >= 1) {
          TclObj resultVar = ops->list.at(interp, useVarListParsed, 0);
          ops->var.set(interp, resultVar, bodyResult);
        }
        if (useVarListLen >= 2) {
          TclObj optsVar = ops->list.at(interp, useVarListParsed, 1);
          if (ops->list.is_nil(interp, bodyOptions)) {
            TclObj defaultOpts = ops->list.create(interp);
            defaultOpts = ops->list.push(interp, defaultOpts, ops->string.intern(interp, S("-code")));
            defaultOpts = ops->list.push(interp, defaultOpts, ops->integer.create(interp, 1));
            ops->var.set(interp, optsVar, defaultOpts);
          } else {
            ops->var.set(interp, optsVar, bodyOptions);
          }
        }

        // Execute handler script
        handlerResult = tcl_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
        handlerResultObj = ops->interp.get_result(interp);
        break; // Handler executed, stop searching
      }

      i += 4;
    } else if (str_eq(typeStr, typeLen, "finally")) {
      // finally must be at the end - if we got here, it's malformed
      TclObj msg = ops->string.intern(
          interp, S("finally clause must be at the end"));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    } else {
      // Unknown handler type
      TclObj prefix = ops->string.intern(interp, S("bad handler type \""));
      TclObj suffix = ops->string.intern(interp, S("\": must be on, trap, or finally"));
      TclObj msg = ops->string.concat(interp, prefix, handlerType);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Execute finally clause if present
  TclResult finalResult = handlerResult;
  TclObj finalResultObj = handlerResultObj;

  if (!ops->list.is_nil(interp, finallyScript) && finallyScript != 0) {
    TclResult finallyCode = tcl_script_eval_obj(ops, interp, finallyScript, TCL_EVAL_LOCAL);

    // If finally returns an error, it overrides everything
    if (finallyCode != TCL_OK) {
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
