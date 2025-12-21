#include "tclc.h"
#include "internal.h"

TclResult tcl_builtin_proc(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  // proc requires exactly 3 arguments: name args body
  if (argc != 3) {
    TclObj msg = ops->string.intern(
        interp, "wrong # args: should be \"proc name args body\"", 45);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Extract name, params, body
  TclObj name = ops->list.shift(interp, args);
  TclObj params = ops->list.shift(interp, args);
  TclObj body = ops->list.shift(interp, args);

  // Register the procedure
  ops->proc.define(interp, name, params, body);

  // proc returns empty string
  TclObj empty = ops->string.intern(interp, "", 0);
  ops->interp.set_result(interp, empty);
  return TCL_OK;
}

// Helper to check if a string equals "args"
static int is_args_param(const char *s, size_t len) {
  return len == 4 && s[0] == 'a' && s[1] == 'r' && s[2] == 'g' && s[3] == 's';
}

TclResult tcl_invoke_proc(const TclHostOps *ops, TclInterp interp,
                          TclObj name, TclObj args) {
  // Get the procedure's parameter list and body
  TclObj params;
  TclObj body;
  if (ops->proc.params(interp, name, &params) != TCL_OK) {
    return TCL_ERROR;
  }
  if (ops->proc.body(interp, name, &body) != TCL_OK) {
    return TCL_ERROR;
  }

  // Count arguments and parameters
  size_t argc = ops->list.length(interp, args);
  size_t paramc = ops->list.length(interp, params);

  // Check if this is a variadic proc (last param is "args")
  int is_variadic = 0;
  if (paramc > 0) {
    // Create a copy to iterate and check the last one
    TclObj paramsCopy = ops->list.from(interp, params);
    TclObj lastParam = 0;
    for (size_t i = 0; i < paramc; i++) {
      lastParam = ops->list.shift(interp, paramsCopy);
    }
    if (lastParam != 0) {
      size_t lastLen;
      const char *lastStr = ops->string.get(interp, lastParam, &lastLen);
      is_variadic = is_args_param(lastStr, lastLen);
    }
  }

  // Calculate required parameter count (excludes "args" if variadic)
  size_t required_params = is_variadic ? paramc - 1 : paramc;

  // Check argument count
  int args_ok = 0;
  if (is_variadic) {
    // Variadic: need at least required_params arguments
    args_ok = (argc >= required_params);
  } else {
    // Non-variadic: need exactly paramc arguments
    args_ok = (argc == paramc);
  }

  if (!args_ok) {
    // Build error message: wrong # args: should be "name param1 param2 ..."
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);

    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"", 25);
    TclObj namePart = ops->string.intern(interp, nameStr, nameLen);
    msg = ops->string.concat(interp, msg, namePart);

    // Add parameters to error message
    TclObj paramsCopy = ops->list.from(interp, params);
    for (size_t i = 0; i < paramc; i++) {
      TclObj space = ops->string.intern(interp, " ", 1);
      msg = ops->string.concat(interp, msg, space);
      TclObj param = ops->list.shift(interp, paramsCopy);
      size_t paramLen;
      const char *paramStr = ops->string.get(interp, param, &paramLen);

      // For variadic, show "?arg ...?" instead of "args"
      if (is_variadic && i == paramc - 1) {
        TclObj argsHint = ops->string.intern(interp, "?arg ...?", 9);
        msg = ops->string.concat(interp, msg, argsHint);
      } else {
        TclObj paramPart = ops->string.intern(interp, paramStr, paramLen);
        msg = ops->string.concat(interp, msg, paramPart);
      }
    }

    TclObj end = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, end);

    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Push a new call frame
  if (ops->frame.push(interp, name, args) != TCL_OK) {
    return TCL_ERROR;
  }

  // Create copies of params and args for binding (since shift mutates)
  TclObj paramsList = ops->list.from(interp, params);
  TclObj argsList = ops->list.from(interp, args);

  // Bind arguments to parameters
  if (is_variadic) {
    // Bind required parameters first
    for (size_t i = 0; i < required_params; i++) {
      TclObj param = ops->list.shift(interp, paramsList);
      TclObj arg = ops->list.shift(interp, argsList);
      ops->var.set(interp, param, arg);
    }

    // Get the "args" parameter name
    TclObj argsParam = ops->list.shift(interp, paramsList);

    // Collect remaining arguments into a list
    TclObj collectedArgs = ops->list.create(interp);
    size_t remaining = argc - required_params;
    for (size_t i = 0; i < remaining; i++) {
      TclObj arg = ops->list.shift(interp, argsList);
      collectedArgs = ops->list.push(interp, collectedArgs, arg);
    }

    // Bind the list to "args"
    ops->var.set(interp, argsParam, collectedArgs);
  } else {
    // Non-variadic: bind all params normally
    for (size_t i = 0; i < paramc; i++) {
      TclObj param = ops->list.shift(interp, paramsList);
      TclObj arg = ops->list.shift(interp, argsList);
      ops->var.set(interp, param, arg);
    }
  }

  // Get the string representation of the body and evaluate it
  size_t bodyLen;
  const char *bodyStr = ops->string.get(interp, body, &bodyLen);
  TclResult result = tcl_eval_string(ops, interp, bodyStr, bodyLen, TCL_EVAL_LOCAL);

  // Pop the call frame
  ops->frame.pop(interp);

  return result;
}
