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
  FeatherObj resultValue = ops->string.intern(interp, "", 0);

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
          // Build error with original object
          FeatherObj msg1 = ops->string.intern(interp, "expected integer but got \"", 26);
          FeatherObj msg3 = ops->string.intern(interp, "\"", 1);
          FeatherObj msg = ops->string.concat(interp, msg1, levelArg);
          msg = ops->string.concat(interp, msg, msg3);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        if (levelVal < 0) {
          // Build error with original object
          FeatherObj msg1 = ops->string.intern(interp,
            "bad -level value: expected non-negative integer but got \"", 57);
          FeatherObj msg3 = ops->string.intern(interp, "\"", 1);
          FeatherObj msg = ops->string.concat(interp, msg1, levelArg);
          msg = ops->string.concat(interp, msg, msg3);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        level = (int)levelVal;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-options")) {
        // -options takes a dictionary - skip for now, just consume value
        if (argc == 0) {
          FeatherObj msg = ops->string.intern(interp, "-options requires a value", 25);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        ops->list.shift(interp, argsCopy);
        argc--;
      } else {
        // Unknown option - build error with original object
        FeatherObj msg1 = ops->string.intern(interp, "bad option \"", 12);
        FeatherObj msg3 = ops->string.intern(interp,
          "\": must be -code, -level, or -options", 37);
        FeatherObj msg = ops->string.concat(interp, msg1, arg);
        msg = ops->string.concat(interp, msg, msg3);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    } else {
      // Not an option, must be the result value
      // Remaining arguments form the result
      if (argc == 1) {
        resultValue = ops->list.shift(interp, argsCopy);
      } else {
        // Multiple remaining args - join with space
        resultValue = ops->list.shift(interp, argsCopy);
        argc--;
        while (argc > 0) {
          FeatherObj space = ops->string.intern(interp, " ", 1);
          FeatherObj next = ops->list.shift(interp, argsCopy);
          resultValue = ops->string.concat(interp, resultValue, space);
          resultValue = ops->string.concat(interp, resultValue, next);
          argc--;
        }
      }
      break;
    }
  }

  // Build return options dictionary as a list: {-code X -level Y}
  FeatherObj options = ops->list.create(interp);
  options = ops->list.push(interp, options, ops->string.intern(interp, "-code", 5));
  options = ops->list.push(interp, options, ops->integer.create(interp, code));
  options = ops->list.push(interp, options, ops->string.intern(interp, "-level", 6));
  options = ops->list.push(interp, options, ops->integer.create(interp, level));

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
