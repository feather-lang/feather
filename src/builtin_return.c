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
    "Return from a procedure, or set return code of a script",
    "In its simplest usage, the return command is used without options in the body "
    "of a procedure to immediately return control to the caller of the procedure. "
    "If a result argument is provided, its value becomes the result of the procedure "
    "passed back to the caller. If result is not specified then an empty string will "
    "be returned to the caller as the result of the procedure.\n\n"
    "The return command serves a similar function within script files that are "
    "evaluated by the source command. When source evaluates the contents of a file "
    "as a script, an invocation of the return command will cause script evaluation "
    "to immediately cease, and the value result (or an empty string) will be returned "
    "as the result of the source command.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Custom section: Exceptional Return Codes
  e = feather_usage_section(ops, interp, "Exceptional Return Codes",
    "In addition to the result of a procedure, the return code of a procedure may "
    "also be set by return through use of the -code option. In the usual case where "
    "the -code option is not specified the procedure will return normally. However, "
    "the -code option may be used to generate an exceptional return from the procedure. "
    "Code may have any of the following values:\n\n"
    "ok (or 0)       Normal return: same as if the option is omitted. The return code "
    "of the procedure is 0 (TCL_OK).\n\n"
    "error (or 1)    Error return: the return code of the procedure is 1 (TCL_ERROR). "
    "The procedure command behaves in its calling context as if it were the command "
    "error result.\n\n"
    "return (or 2)   The return code of the procedure is 2 (TCL_RETURN). The procedure "
    "command behaves in its calling context as if it were the command return (with no arguments).\n\n"
    "break (or 3)    The return code of the procedure is 3 (TCL_BREAK). The procedure "
    "command behaves in its calling context as if it were the command break.\n\n"
    "continue (or 4) The return code of the procedure is 4 (TCL_CONTINUE). The procedure "
    "command behaves in its calling context as if it were the command continue.\n\n"
    "value           Value must be an integer; it will be returned as the return code "
    "for the current procedure.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Custom section: Return Options
  e = feather_usage_section(ops, interp, "Return Options",
    "In addition to a result and a return code, evaluation of a command in Tcl also "
    "produces a dictionary of return options. In general usage, all option value pairs "
    "given as arguments to return become entries in the return options dictionary, and "
    "any values at all are acceptable except as noted below. The catch command may be "
    "used to capture all of this information - the return code, the result, and the "
    "return options dictionary - that arise from evaluation of a script.\n\n"
    "As documented above, the -code entry in the return options dictionary receives "
    "special treatment by Tcl. There are other return options also recognized and "
    "treated specially by Tcl.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Flags
  e = feather_usage_flag(ops, interp, "-code", NULL, "<code>");
  e = feather_usage_help(ops, interp, e,
    "Completion code: ok, error, return, break, continue, or an integer (default: ok)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-level", NULL, "<level>");
  e = feather_usage_help(ops, interp, e,
    "The -level and -code options work together to set the return code to be returned "
    "by one of the commands currently being evaluated. The level value must be a "
    "non-negative integer representing a number of levels on the call stack. It defines "
    "the number of levels up the stack at which the return code of a command currently "
    "being evaluated should be code. If no -level option is provided, the default value "
    "of level is 1, so that return sets the return code that the current procedure "
    "returns to its caller, 1 level up the call stack");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-errorcode", NULL, "<list>");
  e = feather_usage_help(ops, interp, e,
    "The -errorcode option receives special treatment only when the value of the -code "
    "option is TCL_ERROR. Then the list value is meant to be additional information about "
    "the error, presented as a Tcl list for further processing by programs. If no "
    "-errorcode option is provided to return when the -code error option is provided, "
    "Tcl will set the value of the -errorcode entry in the return options dictionary "
    "to the default value of NONE. The -errorcode return option will also be stored in "
    "the global variable errorCode");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-errorinfo", NULL, "<info>");
  e = feather_usage_help(ops, interp, e,
    "The -errorinfo option receives special treatment only when the value of the -code "
    "option is TCL_ERROR. Then info is the initial stack trace, meant to provide to a "
    "human reader additional information about the context in which the error occurred. "
    "The stack trace will also be stored in the global variable errorInfo");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-errorstack", NULL, "<list>");
  e = feather_usage_help(ops, interp, e,
    "The -errorstack option receives special treatment only when the value of the -code "
    "option is TCL_ERROR. Then list is the initial error stack, recording actual argument "
    "values passed to each proc level. If no -errorstack option is provided to return "
    "when the -code error option is provided, Tcl will provide its own initial error "
    "stack in the entry for -errorstack");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-options", NULL, "<options>");
  e = feather_usage_help(ops, interp, e,
    "The value options must be a valid dictionary. The entries of that dictionary are "
    "treated as additional option value pairs for the return command. This enables "
    "the standard error re-raising pattern: catch {command} result opts; return -options $opts $result");
  spec = feather_usage_add(ops, interp, spec, e);

  // Arguments
  e = feather_usage_arg(ops, interp, "?result?");
  e = feather_usage_help(ops, interp, e,
    "Return value (default: empty string)");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "proc printOneLine {} {\n    puts \"line 1\"\n    return\n    puts \"line 2\"\n}",
    "Return from a procedure, interrupting the procedure body",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc returnX {} {return X}\nputs [returnX]",
    "Use return to set the value returned by the procedure (prints \"X\")",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc factorial {n} {\n    if {![string is integer $n] || ($n < 0)} {\n        return -code error \"expected non-negative integer, but got \\\"$n\\\"\"\n    }\n    if {$n < 2} { return 1 }\n    return [expr {$n * [factorial [expr {$n - 1}]]}]\n}",
    "Use return -code error to report invalid arguments",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc myBreak {} {\n    return -code break\n}",
    "A procedure replacement for break",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc doSomething {} {\n    set resource [allocate]\n    catch {\n        # Long script that might raise an error\n    } result options\n    deallocate $resource\n    return -options $options $result\n}",
    "Use catch and return -options to re-raise a caught error",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // See Also section
  e = feather_usage_section(ops, interp, "See Also",
    "break, catch, continue, error, proc, source");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "return", spec);
}
