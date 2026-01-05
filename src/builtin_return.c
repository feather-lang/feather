#include "feather.h"
#include "internal.h"
// Parse a code name or integer into a FeatherResult value
// Returns TCL_OK on success and sets *code, TCL_ERROR on failure
static FeatherResult return_parse_code(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj codeObj, int *code) {
  // Check named codes using feather_obj_eq_literal
  if (feather_obj_eq_literal(ops, interp, codeObj, "ok")) {
    *code = TCL_OK;
    return TCL_OK;
  }
  if (feather_obj_eq_literal(ops, interp, codeObj, "error")) {
    *code = TCL_ERROR;
    return TCL_OK;
  }
  if (feather_obj_eq_literal(ops, interp, codeObj, "return")) {
    *code = TCL_RETURN;
    return TCL_OK;
  }
  if (feather_obj_eq_literal(ops, interp, codeObj, "break")) {
    *code = TCL_BREAK;
    return TCL_OK;
  }
  if (feather_obj_eq_literal(ops, interp, codeObj, "continue")) {
    *code = TCL_CONTINUE;
    return TCL_OK;
  }

  // Try as integer
  int64_t intVal;
  if (ops->integer.get(interp, codeObj, &intVal) == TCL_OK) {
    *code = (int)intVal;
    return TCL_OK;
  }

  // Error: invalid code - use concat with original object
  FeatherObj msg1 = ops->string.intern(interp, "bad completion code \"", 21);
  FeatherObj msg3 = ops->string.intern(interp,
    "\": must be ok, error, return, break, continue, or an integer", 60);
  FeatherObj msg = ops->string.concat(interp, msg1, codeObj);
  msg = ops->string.concat(interp, msg, msg3);
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}

FeatherResult feather_builtin_return(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  int code = TCL_OK;      // Default: -code ok
  int level = 1;          // Default: -level 1
  FeatherObj errorcode = 0;  // Default: not set (will use NONE)
  FeatherObj errorinfo = 0;  // Default: not set
  FeatherObj errorstack = 0;  // Default: not set
  FeatherObj resultValue = ops->string.intern(interp, "", 0);
  FeatherObj customOptions = ops->list.create(interp);  // Collect arbitrary options

  // Make a copy of args since we'll be shifting
  FeatherObj argsCopy = ops->list.from(interp, args);
  size_t argc = ops->list.length(interp, argsCopy);

  // Parse options
  while (argc > 0) {
    FeatherObj arg = ops->list.at(interp, argsCopy, 0);

    // Check if this is an option (starts with -)
    if (ops->string.byte_at(interp, arg, 0) == '-') {
      // Remove this argument
      ops->list.shift(interp, argsCopy);
      argc--;

      if (feather_obj_eq_literal(ops, interp, arg, "-code")) {
        // Need value
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-code requires a value", 22);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        FeatherObj codeArg = ops->list.shift(interp, argsCopy);
        argc--;
        if (return_parse_code(ops, interp, codeArg, &code) != TCL_OK) {
          return TCL_ERROR;
        }
      } else if (feather_obj_eq_literal(ops, interp, arg, "-level")) {
        // Need value
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-level requires a value", 23);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        FeatherObj levelArg = ops->list.shift(interp, argsCopy);
        argc--;
        int64_t levelVal;
        if (ops->integer.get(interp, levelArg, &levelVal) != TCL_OK) {
          feather_error_expected(ops, interp, "integer", levelArg);
          return TCL_ERROR;
        }
        if (levelVal < 0) {
          FeatherObj msg1 = ops->string.intern(interp,
            "bad -level value: expected non-negative integer but got \"", 57);
          FeatherObj msg3 = ops->string.intern(interp, "\"", 1);
          FeatherObj msg = ops->string.concat(interp, msg1, levelArg);
          msg = ops->string.concat(interp, msg, msg3);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        level = (int)levelVal;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-errorcode")) {
        // Need value
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-errorcode requires a value", 27);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        errorcode = ops->list.shift(interp, argsCopy);
        argc--;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-errorinfo")) {
        // Need value
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-errorinfo requires a value", 27);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        errorinfo = ops->list.shift(interp, argsCopy);
        argc--;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-errorstack")) {
        // Need value
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-errorstack requires a value", 28);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        errorstack = ops->list.shift(interp, argsCopy);
        argc--;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-options")) {
        // -options takes a dictionary, extract -code and -level from it
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-options requires a value", 25);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        FeatherObj optionsArg = ops->list.shift(interp, argsCopy);
        argc--;

        // Try to parse as a dictionary
        FeatherObj optDict = ops->dict.from(interp, optionsArg);
        if (ops->list.is_nil(interp, optDict)) {
          // Not a valid dict - that's an error
          FeatherObj msg1 = ops->string.intern(interp, "bad -options value \"", 20);
          FeatherObj msg3 = ops->string.intern(interp, "\": must be a dict", 17);
          FeatherObj msg = ops->string.concat(interp, msg1, optionsArg);
          msg = ops->string.concat(interp, msg, msg3);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }

        // Extract -code if present
        FeatherObj codeKey = ops->string.intern(interp, "-code", 5);
        if (ops->dict.exists(interp, optDict, codeKey)) {
          FeatherObj codeVal = ops->dict.get(interp, optDict, codeKey);
          if (return_parse_code(ops, interp, codeVal, &code) != TCL_OK) {
            return TCL_ERROR;
          }
        }

        // Extract -level if present
        FeatherObj levelKey = ops->string.intern(interp, "-level", 6);
        if (ops->dict.exists(interp, optDict, levelKey)) {
          FeatherObj levelVal = ops->dict.get(interp, optDict, levelKey);
          int64_t levelInt;
          if (ops->integer.get(interp, levelVal, &levelInt) != TCL_OK) {
            feather_error_expected(ops, interp, "integer", levelVal);
            return TCL_ERROR;
          }
          if (levelInt < 0) {
            FeatherObj msg1 = ops->string.intern(interp,
              "bad -level value: expected non-negative integer but got \"", 57);
            FeatherObj msg3 = ops->string.intern(interp, "\"", 1);
            FeatherObj msg = ops->string.concat(interp, msg1, levelVal);
            msg = ops->string.concat(interp, msg, msg3);
            ops->interp.set_result(interp, msg);
            return TCL_ERROR;
          }
          level = (int)levelInt;
        }

        // Extract -errorcode if present
        FeatherObj errorcodeKey = ops->string.intern(interp, "-errorcode", 10);
        if (ops->dict.exists(interp, optDict, errorcodeKey)) {
          errorcode = ops->dict.get(interp, optDict, errorcodeKey);
        }

        // Extract -errorinfo if present
        FeatherObj errorinfoKey = ops->string.intern(interp, "-errorinfo", 10);
        if (ops->dict.exists(interp, optDict, errorinfoKey)) {
          errorinfo = ops->dict.get(interp, optDict, errorinfoKey);
        }

        // Extract -errorstack if present
        FeatherObj errorstackKey = ops->string.intern(interp, "-errorstack", 11);
        if (ops->dict.exists(interp, optDict, errorstackKey)) {
          errorstack = ops->dict.get(interp, optDict, errorstackKey);
        }

        // Copy any other (custom) options from the dictionary
        // Iterate through the dict and copy any keys we don't recognize
        FeatherObj dictList = ops->list.from(interp, optDict);
        size_t dictLen = ops->list.length(interp, dictList);
        for (size_t i = 0; i + 1 < dictLen; i += 2) {
          FeatherObj key = ops->list.at(interp, dictList, i);
          // Skip known options
          if (feather_obj_eq_literal(ops, interp, key, "-code") ||
              feather_obj_eq_literal(ops, interp, key, "-level") ||
              feather_obj_eq_literal(ops, interp, key, "-errorcode") ||
              feather_obj_eq_literal(ops, interp, key, "-errorinfo") ||
              feather_obj_eq_literal(ops, interp, key, "-errorstack")) {
            continue;
          }
          // Copy custom option
          customOptions = ops->list.push(interp, customOptions, key);
          customOptions = ops->list.push(interp, customOptions, ops->list.at(interp, dictList, i + 1));
        }
      } else {
        // Unknown option - store it for the return options dictionary
        // Need value - if no value, this becomes the result
        if (argc == 0) {
          // No value, treat the option as the result
          resultValue = arg;
          break;
        }
        FeatherObj optValue = ops->list.shift(interp, argsCopy);
        argc--;
        // Store option key and value for later
        customOptions = ops->list.push(interp, customOptions, arg);
        customOptions = ops->list.push(interp, customOptions, optValue);
      }
    } else {
      // Not an option, must be the result value
      // TCL takes only the last argument as the result
      resultValue = ops->list.at(interp, argsCopy, argc - 1);
      break;
    }
  }

  // Build return options dictionary as a list: {custom... -code X -level Y ...}
  // Custom options come first, then standard options
  FeatherObj options = ops->list.create(interp);

  // Add custom options first (they were collected in order)
  size_t customLen = ops->list.length(interp, customOptions);
  for (size_t i = 0; i < customLen; i++) {
    options = ops->list.push(interp, options, ops->list.at(interp, customOptions, i));
  }

  // Then add standard options
  options = ops->list.push(interp, options, ops->string.intern(interp, "-code", 5));
  options = ops->list.push(interp, options, ops->integer.create(interp, code));
  options = ops->list.push(interp, options, ops->string.intern(interp, "-level", 6));
  options = ops->list.push(interp, options, ops->integer.create(interp, level));

  // Add -errorcode if set, or default to NONE for error codes
  if (errorcode != 0) {
    options = ops->list.push(interp, options, ops->string.intern(interp, "-errorcode", 10));
    options = ops->list.push(interp, options, errorcode);
  } else if (code == TCL_ERROR) {
    // Default errorcode for errors is NONE
    options = ops->list.push(interp, options, ops->string.intern(interp, "-errorcode", 10));
    options = ops->list.push(interp, options, ops->string.intern(interp, "NONE", 4));
  }

  // Add -errorinfo if set
  if (errorinfo != 0) {
    options = ops->list.push(interp, options, ops->string.intern(interp, "-errorinfo", 10));
    options = ops->list.push(interp, options, errorinfo);
  }

  // Add -errorstack if set
  if (errorstack != 0) {
    options = ops->list.push(interp, options, ops->string.intern(interp, "-errorstack", 11));
    options = ops->list.push(interp, options, errorstack);
  }

  // Store the return options
  FeatherResult storeResult = ops->interp.set_return_options(interp, options);

  // Set the result value
  ops->interp.set_result(interp, resultValue);

  // Determine what code to return
  if (level == 0) {
    // Level 0: the code takes effect immediately
    return (FeatherResult)code;
  } else {
    // Level > 0: return TCL_RETURN, proc will handle decrementing level
    return TCL_RETURN;
  }
}

void feather_register_return_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Return from a procedure",
    "Returns from the current procedure, loop, or top-level command, optionally "
    "specifying the return value, completion code, and other options.\n\n"
    "With no options, returns from the current procedure with the specified value "
    "(or empty string if no value is given). The value becomes the result of the "
    "procedure invocation.\n\n"
    "The -code option specifies the completion code: ok (default), error, return, "
    "break, continue, or any integer. This affects how the return is interpreted "
    "by the caller.\n\n"
    "The -level option controls which stack level the return applies to. Level 0 "
    "means the code takes effect immediately. Level 1 (default) means return from "
    "the enclosing procedure. Higher levels propagate up the call stack.\n\n"
    "The -errorcode option sets a machine-readable error code when returning an "
    "error. The -errorinfo option provides custom stack trace information. The "
    "-errorstack option stores call stack details.\n\n"
    "The -options option accepts a dictionary containing any of the above options. "
    "This enables error re-raising: catch {command} result opts; return -options $opts $result\n\n"
    "Arbitrary option-value pairs (like -custom value) are accepted and preserved "
    "in the return options dictionary, available through catch.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-code?");
  e = feather_usage_help(ops, interp, e,
    "Completion code: ok, error, return, break, continue, or an integer (default: ok)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?code?");
  e = feather_usage_help(ops, interp, e, "The code value");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-level?");
  e = feather_usage_help(ops, interp, e,
    "Stack level for return (0 = immediate, 1 = enclosing proc, default: 1)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?level?");
  e = feather_usage_help(ops, interp, e, "The level value (non-negative integer)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-errorcode?");
  e = feather_usage_help(ops, interp, e, "Machine-readable error code (used with -code error)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?errorcode?");
  e = feather_usage_help(ops, interp, e, "The error code value");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-errorinfo?");
  e = feather_usage_help(ops, interp, e, "Custom stack trace information (used with -code error)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?errorinfo?");
  e = feather_usage_help(ops, interp, e, "The error info value");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-errorstack?");
  e = feather_usage_help(ops, interp, e, "Call stack information (automatically generated during error propagation)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?errorstack?");
  e = feather_usage_help(ops, interp, e, "The error stack value");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-options?");
  e = feather_usage_help(ops, interp, e,
    "Dictionary containing return options (extracts -code, -level, -errorcode, -errorinfo, -errorstack)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?options?");
  e = feather_usage_help(ops, interp, e, "The options dictionary");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?-option?...");
  e = feather_usage_help(ops, interp, e,
    "Arbitrary custom options (stored in return options dictionary)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?value?...");
  e = feather_usage_help(ops, interp, e,
    "Return value (default: empty string). If multiple values given, last one is used");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return",
    "Return empty string from procedure",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return 42",
    "Return value 42 from procedure",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return -code error \"Something failed\"",
    "Return an error with message",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return -code error -errorcode {POSIX ENOENT} \"File not found\"",
    "Return error with machine-readable error code",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "catch {command} result opts\nreturn -options $opts $result",
    "Re-raise an error preserving all options",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return -code break",
    "Break out of enclosing loop from within a procedure",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return -level 0 -code error \"Immediate error\"",
    "Trigger error immediately (level 0) without returning from procedure",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "return", spec);
}
