#include "feather.h"
#include "internal.h"
#include "namespace_util.h"
#include "charclass.h"

FeatherResult feather_builtin_proc(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  // proc requires exactly 3 arguments: name args body
  if (argc != 3) {
    FeatherObj msg = ops->string.intern(
        interp, "wrong # args: should be \"proc name args body\"", 45);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Extract name, params, body
  FeatherObj name = ops->list.shift(interp, args);
  FeatherObj params = ops->list.shift(interp, args);
  FeatherObj body = ops->list.shift(interp, args);

  // Determine the fully qualified proc name
  FeatherObj qualifiedName;
  if (feather_obj_is_qualified(ops, interp, name)) {
    // Name is already qualified (starts with :: or contains ::)
    // Use it as-is, but ensure the namespace exists
    // For "::foo::bar::baz", create namespaces ::foo and ::foo::bar

    // Find the last :: to split namespace from proc name
    long lastSep = feather_obj_find_last_colons(ops, interp, name);

    // Create namespace path if needed (everything before last ::)
    if (lastSep > 0) {
      FeatherObj nsPath = ops->string.slice(interp, name, 0, (size_t)lastSep);
      ops->ns.create(interp, nsPath);
    }

    qualifiedName = name;
  } else {
    // Unqualified name - prepend current namespace
    FeatherObj currentNs = ops->ns.current(interp);

    // Always store with full namespace path
    // Global namespace (::) -> "::name"
    // Other namespace -> "::ns::name"
    if (feather_obj_is_global_ns(ops, interp, currentNs)) {
      // Global namespace: prepend "::"
      qualifiedName = ops->string.intern(interp, "::", 2);
      qualifiedName = ops->string.concat(interp, qualifiedName, name);
    } else {
      // Other namespace: "::ns::name"
      qualifiedName = ops->string.concat(interp, currentNs,
                                         ops->string.intern(interp, "::", 2));
      qualifiedName = ops->string.concat(interp, qualifiedName, name);
    }
  }

  // Register the procedure with its fully qualified name
  feather_register_command(ops, interp, qualifiedName, TCL_CMD_PROC, NULL, params, body);

  // proc returns empty string
  FeatherObj empty = ops->string.intern(interp, "", 0);
  ops->interp.set_result(interp, empty);
  return TCL_OK;
}


FeatherResult feather_invoke_proc(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj name, FeatherObj args) {
  // Get the procedure's parameter list and body
  FeatherObj params = 0;
  FeatherObj body = 0;
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, name, NULL, &params, &body);
  if (cmdType != TCL_CMD_PROC || params == 0 || body == 0) {
    return TCL_ERROR;
  }

  // Count arguments and parameters
  size_t argc = ops->list.length(interp, args);
  size_t paramc = ops->list.length(interp, params);

  // Check if this is a variadic proc (last param is "args")
  int is_variadic = 0;
  if (paramc > 0) {
    // Get the last parameter to check if it's "args"
    FeatherObj lastParam = ops->list.at(interp, params, paramc - 1);
    if (lastParam != 0) {
      is_variadic = feather_obj_is_args_param(ops, interp, lastParam);
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
    // Use display name to strip :: for global namespace commands
    FeatherObj displayName = feather_get_display_name(ops, interp, name);

    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"", 25);
    msg = ops->string.concat(interp, msg, displayName);

    // Add parameters to error message
    FeatherObj paramsCopy = ops->list.from(interp, params);
    for (size_t i = 0; i < paramc; i++) {
      FeatherObj space = ops->string.intern(interp, " ", 1);
      msg = ops->string.concat(interp, msg, space);
      FeatherObj param = ops->list.shift(interp, paramsCopy);

      // For variadic, show "?arg ...?" instead of "args"
      if (is_variadic && i == paramc - 1) {
        FeatherObj argsHint = ops->string.intern(interp, "?arg ...?", 9);
        msg = ops->string.concat(interp, msg, argsHint);
      } else {
        // Append the param object directly (no need for string.get)
        msg = ops->string.concat(interp, msg, param);
      }
    }

    FeatherObj end = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, end);

    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Push a new call frame
  // First, get the line number from the parent frame (set by eval before calling us)
  size_t parentLevel = ops->frame.level(interp);
  size_t parentLine = ops->frame.get_line(interp, parentLevel);

  if (ops->frame.push(interp, name, args) != TCL_OK) {
    return TCL_ERROR;
  }

  // Copy the line number from the parent frame to the new frame
  ops->frame.set_line(interp, parentLine);

  // Set the namespace for this frame based on the proc's qualified name
  // For "::counter::incr", the namespace is "::counter"
  // For "incr", the namespace is "::" (global)
  if (feather_obj_is_qualified(ops, interp, name)) {
    // Find the last :: to extract the namespace part
    long lastSep = feather_obj_find_last_colons(ops, interp, name);
    if (lastSep > 0) {
      // Namespace is everything before the last ::
      FeatherObj ns = ops->string.slice(interp, name, 0, (size_t)lastSep);
      ops->frame.set_namespace(interp, ns);
    } else if (lastSep == 0) {
      // Starts with :: but has no more separators, e.g., "::incr"
      // Namespace is "::" (global)
      FeatherObj globalNs = ops->string.intern(interp, "::", 2);
      ops->frame.set_namespace(interp, globalNs);
    }
  }
  // For unqualified names, leave namespace as default (global)

  // Create copies of params and args for binding (since shift mutates)
  FeatherObj paramsList = ops->list.from(interp, params);
  FeatherObj argsList = ops->list.from(interp, args);

  // Bind arguments to parameters
  if (is_variadic) {
    // Bind required parameters first
    for (size_t i = 0; i < required_params; i++) {
      FeatherObj param = ops->list.shift(interp, paramsList);
      FeatherObj arg = ops->list.shift(interp, argsList);
      ops->var.set(interp, param, arg);
    }

    // Get the "args" parameter name
    FeatherObj argsParam = ops->list.shift(interp, paramsList);

    // Collect remaining arguments into a list
    FeatherObj collectedArgs = ops->list.create(interp);
    size_t remaining = argc - required_params;
    for (size_t i = 0; i < remaining; i++) {
      FeatherObj arg = ops->list.shift(interp, argsList);
      collectedArgs = ops->list.push(interp, collectedArgs, arg);
    }

    // Bind the list to "args"
    ops->var.set(interp, argsParam, collectedArgs);
  } else {
    // Non-variadic: bind all params normally
    for (size_t i = 0; i < paramc; i++) {
      FeatherObj param = ops->list.shift(interp, paramsList);
      FeatherObj arg = ops->list.shift(interp, argsList);
      ops->var.set(interp, param, arg);
    }
  }

  // Evaluate the body as a script
  FeatherResult result = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

  // Pop the call frame
  ops->frame.pop(interp);

  // Handle TCL_RETURN specially
  if (result == TCL_RETURN) {
    // Get the return options
    FeatherObj opts = ops->interp.get_return_options(interp, result);

    // Parse -code and -level from the options list
    // Options list format: {-code X -level Y}
    int code = TCL_OK;
    int level = 1;

    size_t optsLen = ops->list.length(interp, opts);
    FeatherObj optsCopy = ops->list.from(interp, opts);

    for (size_t i = 0; i + 1 < optsLen; i += 2) {
      FeatherObj key = ops->list.shift(interp, optsCopy);
      FeatherObj val = ops->list.shift(interp, optsCopy);

      if (feather_obj_eq_literal(ops, interp, key, "-code")) {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          code = (int)intVal;
        }
      } else if (feather_obj_eq_literal(ops, interp, key, "-level")) {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          level = (int)intVal;
        }
      }
    }

    // Decrement level
    level--;

    if (level <= 0) {
      // Level reached 0, apply the -code
      return (FeatherResult)code;
    } else {
      // Level > 0, update options and keep returning TCL_RETURN
      FeatherObj newOpts = ops->list.create(interp);
      newOpts = ops->list.push(interp, newOpts,
                               ops->string.intern(interp, "-code", 5));
      newOpts = ops->list.push(interp, newOpts,
                               ops->integer.create(interp, code));
      newOpts = ops->list.push(interp, newOpts,
                               ops->string.intern(interp, "-level", 6));
      newOpts = ops->list.push(interp, newOpts,
                               ops->integer.create(interp, level));
      ops->interp.set_return_options(interp, newOpts);
      return TCL_RETURN;
    }
  }

  return result;
}
