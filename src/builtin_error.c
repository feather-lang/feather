#include "feather.h"
#include "internal.h"
#include "error_trace.h"

// Helper macro
#define S(lit) (lit), feather_strlen(lit)

FeatherResult feather_builtin_error(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // error message ?info? ?code?
  if (argc < 1 || argc > 3) {
    FeatherObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"error message ?info? ?code?\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the error message
  FeatherObj message = ops->list.at(interp, args, 0);

  // Build return options dictionary
  FeatherObj options = ops->list.create(interp);
  options = ops->list.push(interp, options,
                           ops->string.intern(interp, S("-code")));
  options = ops->list.push(interp, options, ops->integer.create(interp, 1));

  // Add -errorinfo if provided
  if (argc >= 2) {
    FeatherObj info = ops->list.at(interp, args, 1);
    options = ops->list.push(interp, options,
                             ops->string.intern(interp, S("-errorinfo")));
    options = ops->list.push(interp, options, info);
  }

  // Add -errorcode if provided
  if (argc >= 3) {
    FeatherObj code = ops->list.at(interp, args, 2);
    options = ops->list.push(interp, options,
                             ops->string.intern(interp, S("-errorcode")));
    options = ops->list.push(interp, options, code);
  }

  // Store return options
  ops->interp.set_return_options(interp, options);

  // Set the error message as result
  ops->interp.set_result(interp, message);

  // Initialize error trace state if not already active and no explicit -errorinfo
  if (argc < 2 && !feather_error_is_active(ops, interp)) {
    feather_error_init(ops, interp, message, cmd, args);
  }

  return TCL_ERROR;
}

void feather_register_error_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Generate an error",
    "Generates an error with the specified message. The command returns TCL_ERROR, "
    "causing the current command to fail and the error to propagate up the call stack.\n\n"
    "If the info argument is provided, it sets the -errorinfo return option, which "
    "initializes the stack trace with custom information. Otherwise, Feather automatically "
    "generates a stack trace as the error propagates.\n\n"
    "If the code argument is provided, it sets the -errorcode return option, which "
    "provides a machine-readable error code for programmatic error handling.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<message>");
  e = feather_usage_help(ops, interp, e,
    "The error message to display. This becomes the interpreter result.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?info?");
  e = feather_usage_help(ops, interp, e,
    "Optional stack trace information. If provided, overrides automatic stack trace "
    "generation. This is used when re-raising caught errors to preserve the original "
    "stack trace.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?code?");
  e = feather_usage_help(ops, interp, e,
    "Optional machine-readable error code. This is typically a list that categorizes "
    "the error type (e.g., \"ARITH DIVZERO\" for division by zero).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "error \"File not found\"",
    "Generate a simple error with a message",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "error \"Division by zero\" \"\" {ARITH DIVZERO}",
    "Generate an error with a machine-readable error code",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "if {[catch {some_operation} result options]} {\n"
    "    # Examine error and re-raise with preserved stack trace\n"
    "    error $result [dict get $options -errorinfo] [dict get $options -errorcode]\n"
    "}",
    "Re-raise a caught error while preserving its stack trace and error code",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "catch(1), return(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "error", spec);
}
