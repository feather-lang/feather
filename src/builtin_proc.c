#include "tclc.h"
#include "internal.h"

/**
 * Helper to get the display name for a command.
 * Strips leading "::" for global namespace commands (e.g., "::set" -> "set")
 * but preserves the full path for nested namespaces (e.g., "::foo::bar" stays as-is).
 */
static TclObj get_display_name(const TclHostOps *ops, TclInterp interp, TclObj name) {
  size_t len;
  const char *str = ops->string.get(interp, name, &len);

  // Check if it starts with ::
  if (len > 2 && str[0] == ':' && str[1] == ':') {
    // Check if there's another :: after the initial one (nested namespace)
    for (size_t i = 2; i + 1 < len; i++) {
      if (str[i] == ':' && str[i + 1] == ':') {
        // Nested namespace - return as-is
        return name;
      }
    }
    // Global namespace only - strip the leading ::
    return ops->string.intern(interp, str + 2, len - 2);
  }
  return name;
}

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

  // Get the proc name string
  size_t nameLen;
  const char *nameStr = ops->string.get(interp, name, &nameLen);

  // Determine the fully qualified proc name
  TclObj qualifiedName;
  if (tcl_is_qualified(nameStr, nameLen)) {
    // Name is already qualified (starts with :: or contains ::)
    // Use it as-is, but ensure the namespace exists
    // For "::foo::bar::baz", create namespaces ::foo and ::foo::bar

    // Find the last :: to split namespace from proc name
    size_t lastSep = 0;
    for (size_t i = 0; i + 1 < nameLen; i++) {
      if (nameStr[i] == ':' && nameStr[i + 1] == ':') {
        lastSep = i;
      }
    }

    // Create namespace path if needed (everything before last ::)
    if (lastSep > 0) {
      TclObj nsPath = ops->string.intern(interp, nameStr, lastSep);
      ops->ns.create(interp, nsPath);
    }

    qualifiedName = name;
  } else {
    // Unqualified name - prepend current namespace
    TclObj currentNs = ops->ns.current(interp);
    size_t nsLen;
    const char *nsStr = ops->string.get(interp, currentNs, &nsLen);

    // Always store with full namespace path
    // Global namespace (::) -> "::name"
    // Other namespace -> "::ns::name"
    if (nsLen == 2 && nsStr[0] == ':' && nsStr[1] == ':') {
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
  ops->proc.define(interp, qualifiedName, params, body);

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
    // Use display name to strip :: for global namespace commands
    TclObj displayName = get_display_name(ops, interp, name);

    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"", 25);
    msg = ops->string.concat(interp, msg, displayName);

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

  // Set the namespace for this frame based on the proc's qualified name
  // For "::counter::incr", the namespace is "::counter"
  // For "incr", the namespace is "::" (global)
  size_t nameLen;
  const char *nameStr = ops->string.get(interp, name, &nameLen);

  if (tcl_is_qualified(nameStr, nameLen)) {
    // Find the last :: to extract the namespace part
    size_t lastSep = 0;
    int foundSep = 0;
    for (size_t i = 0; i + 1 < nameLen; i++) {
      if (nameStr[i] == ':' && nameStr[i + 1] == ':') {
        lastSep = i;
        foundSep = 1;
      }
    }
    if (foundSep && lastSep > 0) {
      // Namespace is everything before the last ::
      TclObj ns = ops->string.intern(interp, nameStr, lastSep);
      ops->frame.set_namespace(interp, ns);
    } else if (foundSep && lastSep == 0) {
      // Starts with :: but has no more separators, e.g., "::incr"
      // Namespace is "::" (global)
      TclObj globalNs = ops->string.intern(interp, "::", 2);
      ops->frame.set_namespace(interp, globalNs);
    }
  }
  // For unqualified names, leave namespace as default (global)

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

  // Evaluate the body as a script
  TclResult result = tcl_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

  // Pop the call frame
  ops->frame.pop(interp);

  // Handle TCL_RETURN specially
  if (result == TCL_RETURN) {
    // Get the return options
    TclObj opts = ops->interp.get_return_options(interp, result);

    // Parse -code and -level from the options list
    // Options list format: {-code X -level Y}
    int code = TCL_OK;
    int level = 1;

    size_t optsLen = ops->list.length(interp, opts);
    TclObj optsCopy = ops->list.from(interp, opts);

    for (size_t i = 0; i + 1 < optsLen; i += 2) {
      TclObj key = ops->list.shift(interp, optsCopy);
      TclObj val = ops->list.shift(interp, optsCopy);

      size_t keyLen;
      const char *keyStr = ops->string.get(interp, key, &keyLen);

      if (keyLen == 5 && keyStr[0] == '-' && keyStr[1] == 'c' &&
          keyStr[2] == 'o' && keyStr[3] == 'd' && keyStr[4] == 'e') {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          code = (int)intVal;
        }
      } else if (keyLen == 6 && keyStr[0] == '-' && keyStr[1] == 'l' &&
                 keyStr[2] == 'e' && keyStr[3] == 'v' && keyStr[4] == 'e' &&
                 keyStr[5] == 'l') {
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
      return (TclResult)code;
    } else {
      // Level > 0, update options and keep returning TCL_RETURN
      TclObj newOpts = ops->list.create(interp);
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
