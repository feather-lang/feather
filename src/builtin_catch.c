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

void feather_register_catch_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Evaluate script and trap exceptional returns",
    "The catch command may be used to prevent errors from aborting command interpretation. "
    "It calls the interpreter recursively to execute script, and always returns without "
    "raising an error, regardless of any errors that might occur while executing script.\n\n"
    "If script raises an error, catch will return a non-zero integer value corresponding "
    "to the exceptional return code returned by evaluation of script. The normal return "
    "code from script evaluation is zero (0), or TCL_OK. The exceptional return codes are: "
    "1 (TCL_ERROR), 2 (TCL_RETURN), 3 (TCL_BREAK), and 4 (TCL_CONTINUE). Errors during "
    "evaluation of a script are indicated by a return code of TCL_ERROR. The other "
    "exceptional return codes are returned by the return, break, and continue commands.\n\n"
    "If the resultVarName argument is given, then the variable it names is set to the "
    "result of the script evaluation. When the return code from the script is 1 (TCL_ERROR), "
    "the value stored in resultVarName is an error message. When the return code from the "
    "script is 0 (TCL_OK), the value stored in resultVarName is the value returned from script.\n\n"
    "If the optionsVarName argument is given, then the variable it names is set to a "
    "dictionary of return options returned by evaluation of script. Two entries are always "
    "defined in the dictionary: -code and -level. When the return code from evaluation of "
    "script is not TCL_RETURN, the value of the -level entry will be 0, and the value of "
    "the -code entry will be the same as the return code.\n\n"
    "When the return code from evaluation of script is TCL_ERROR, four additional entries "
    "are defined in the dictionary of return options stored in optionsVarName: -errorinfo, "
    "-errorcode, -errorline, and -errorstack. The value of the -errorinfo entry is a "
    "formatted stack trace containing more information about the context in which the "
    "error happened. The value of the -errorcode entry is additional information about "
    "the error stored as a list. The -errorcode value is meant to be further processed "
    "by programs, and may not be particularly readable by people. The value of the "
    "-errorline entry is an integer indicating which line of script was being evaluated "
    "when the error occurred. The value of the -errorstack entry is an even-sized list "
    "made of token-parameter pairs accumulated while unwinding the stack.\n\n"
    "The values of the -errorinfo and -errorcode entries of the most recent error are "
    "also available as values of the global variables ::errorInfo and ::errorCode respectively.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<script>");
  e = feather_usage_help(ops, interp, e, "The script to evaluate");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?resultVarName?");
  e = feather_usage_help(ops, interp, e, "Variable name to store the result or error message");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?optionsVarName?");
  e = feather_usage_help(ops, interp, e, "Variable name to store the return options dictionary");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "if {[catch {open $someFile w} fid]} {\n"
    "    puts stderr \"Could not open $someFile for writing\\n$fid\"\n"
    "    exit 1\n"
    "}",
    "Branch based on success of a script",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "catch {expr {1 / 0}} msg opts\n"
    "puts \"Code: [dict get $opts -code]\"\n"
    "puts \"Error: $msg\"",
    "Capture error details including return options",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "break, continue, dict, error, return");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "catch", spec);
}
