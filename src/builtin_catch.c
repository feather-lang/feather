#include "feather.h"
#include "internal.h"
#include "error_trace.h"

// Helper macro
#define S(lit) (lit), feather_strlen(lit)

FeatherResult feather_builtin_catch(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // catch script ?resultVar? ?optionsVar?
  if (argc < 1 || argc > 3) {
    FeatherObj msg = ops->string.intern(
        interp,
        S("wrong # args: should be \"catch script ?resultVar? ?optionsVar?\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the script to evaluate
  FeatherObj script = ops->list.at(interp, args, 0);

  // Evaluate the script object
  FeatherResult code = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

  // Handle TCL_RETURN specially - unwrap to get the actual code
  if (code == TCL_RETURN) {
    // Get the return options
    FeatherObj opts = ops->interp.get_return_options(interp, code);

    // Parse -code and -level from the options list
    // Options list format: {-code X -level Y}
    int returnCode = TCL_OK;
    int level = 1;

    size_t optsLen = ops->list.length(interp, opts);
    FeatherObj optsCopy = ops->list.from(interp, opts);

    for (size_t i = 0; i + 1 < optsLen; i += 2) {
      FeatherObj key = ops->list.shift(interp, optsCopy);
      FeatherObj val = ops->list.shift(interp, optsCopy);

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

    // Decrement level and determine actual code
    level--;
    if (level <= 0) {
      // Level reached 0, use the actual -code
      code = (FeatherResult)returnCode;
    }
    // If level > 0, keep code as TCL_RETURN (2)
  }

  // Finalize error state before getting options (transfers accumulated trace to opts)
  if (code == TCL_ERROR) {
    if (feather_error_is_active(ops, interp)) {
      feather_error_finalize(ops, interp);
    } else {
      // Even without active error trace, set ::errorCode and ::errorInfo from return options
      FeatherObj opts = ops->interp.get_return_options(interp, code);
      FeatherObj globalNs = ops->string.intern(interp, S("::"));
      FeatherObj errorCode = ops->string.intern(interp, S("NONE"));
      FeatherObj errorInfo = 0;

      size_t optsLen = ops->list.length(interp, opts);
      for (size_t i = 0; i + 1 < optsLen; i += 2) {
        FeatherObj key = ops->list.at(interp, opts, i);
        if (feather_obj_eq_literal(ops, interp, key, "-errorcode")) {
          errorCode = ops->list.at(interp, opts, i + 1);
        } else if (feather_obj_eq_literal(ops, interp, key, "-errorinfo")) {
          errorInfo = ops->list.at(interp, opts, i + 1);
        }
      }
      ops->ns.set_var(interp, globalNs, ops->string.intern(interp, S("errorCode")), errorCode);
      if (errorInfo != 0) {
        ops->ns.set_var(interp, globalNs, ops->string.intern(interp, S("errorInfo")), errorInfo);
      }
    }
  }

  // Get the result (either normal result or error message)
  FeatherObj result = ops->interp.get_result(interp);

  // If resultVar is provided, store the result in it
  if (argc >= 2) {
    FeatherObj varName = ops->list.at(interp, args, 1);
    if (feather_set_var(ops, interp, varName, result) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  // If optionsVar is provided, store the return options in it
  if (argc >= 3) {
    FeatherObj optionsVar = ops->list.at(interp, args, 2);
    FeatherObj options = ops->interp.get_return_options(interp, code);

    // If no return options were explicitly set, create default ones
    if (ops->list.is_nil(interp, options)) {
      options = ops->list.create(interp);
      options = ops->list.push(interp, options, ops->string.intern(interp, S("-code")));
      options = ops->list.push(interp, options, ops->integer.create(interp, (int64_t)code));
      options = ops->list.push(interp, options, ops->string.intern(interp, S("-level")));
      options = ops->list.push(interp, options, ops->integer.create(interp, 0));
    }

    if (feather_set_var(ops, interp, optionsVar, options) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  // Return the code as an integer result
  FeatherObj codeResult = ops->integer.create(interp, (int64_t)code);
  ops->interp.set_result(interp, codeResult);

  return TCL_OK;
}
