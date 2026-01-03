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
          feather_error_expected(ops, interp, "non-negative integer", levelArg);
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
            feather_error_expected(ops, interp, "non-negative integer", levelVal);
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
