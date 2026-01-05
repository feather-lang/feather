#include "feather.h"
#include "internal.h"
#include "error_trace.h"

// Helper macro
#define S(lit) (lit), feather_strlen(lit)

FeatherResult feather_builtin_throw(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // throw type message
  if (argc != 2) {
    FeatherObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"throw type message\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the type (error code list) and message
  FeatherObj type = ops->list.at(interp, args, 0);
  FeatherObj message = ops->list.at(interp, args, 1);

  // Verify type is non-empty
  FeatherObj typeList = ops->list.from(interp, type);
  size_t typeLen = ops->list.length(interp, typeList);
  if (typeLen == 0) {
    FeatherObj msg = ops->string.intern(interp, S("type must be non-empty"));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build return options dictionary
  FeatherObj options = ops->list.create(interp);
  options = ops->list.push(interp, options,
                           ops->string.intern(interp, S("-code")));
  options = ops->list.push(interp, options, ops->integer.create(interp, 1));
  options = ops->list.push(interp, options,
                           ops->string.intern(interp, S("-errorcode")));
  options = ops->list.push(interp, options, type);

  // Store return options
  ops->interp.set_return_options(interp, options);

  // Set the error message as result
  ops->interp.set_result(interp, message);

  // Initialize error trace state if not already active
  if (!feather_error_is_active(ops, interp)) {
    feather_error_init(ops, interp, message, cmd, args);
  }

  return TCL_ERROR;
}

void feather_register_throw_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Generate a machine-readable error",
    "This command causes the current evaluation to be unwound with an error. "
    "The error created is described by the type and message arguments: type must contain "
    "a list of words describing the error in a form that is machine-readable (and which will "
    "form the error-code part of the result dictionary), and message should contain text that "
    "is intended for display to a human being.\n\n"
    "The stack will be unwound until the error is trapped by a suitable catch or try command.\n\n"
    "By convention, the words in the type argument should go from most general to most specific.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<type>");
  e = feather_usage_help(ops, interp, e,
    "A non-empty list of words classifying the error. Convention suggests ordering from "
    "general to specific (e.g., {ARITH DIVZERO}).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<message>");
  e = feather_usage_help(ops, interp, e,
    "Human-readable error message describing what went wrong.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "throw {ARITH DIVZERO} \"division by zero\"",
    "Throw an arithmetic division-by-zero error",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc divide {a b} {\n"
    "    if {$b == 0} {\n"
    "        throw {ARITH DIVZERO} \"cannot divide by zero\"\n"
    "    }\n"
    "    expr {$a / $b}\n"
    "}",
    "Use throw in a procedure to signal invalid input",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "try {\n"
    "    throw {MYAPP NOTFOUND} \"resource not found\"\n"
    "} trap {MYAPP NOTFOUND} err {\n"
    "    puts \"Caught: $err\"\n"
    "}",
    "Throw and catch a custom error type",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "catch, error, return, try");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "throw", spec);
}
