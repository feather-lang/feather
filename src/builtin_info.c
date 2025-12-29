#include "internal.h"
#include "feather.h"
#include "namespace_util.h"

/**
 * info exists varName
 */
static FeatherResult info_exists(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info exists varName\"", 45));
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.at(interp, args, 0);

  // Resolve the variable name (handles qualified names)
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, varName, &ns, &localName);

  int exists;
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local lookup
    exists = (ops->var.exists(interp, localName) == TCL_OK);
  } else {
    // Qualified - namespace lookup
    exists = ops->ns.var_exists(interp, ns, localName);
  }

  ops->interp.set_result(interp, ops->integer.create(interp, exists ? 1 : 0));
  return TCL_OK;
}

/**
 * info level ?number?
 */
static FeatherResult info_level(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    // Return current level
    size_t level = ops->frame.level(interp);
    ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)level));
    return TCL_OK;
  }

  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info level ?number?\"", 45));
    return TCL_ERROR;
  }

  // Get level number
  FeatherObj levelObj = ops->list.at(interp, args, 0);
  int64_t levelNum;
  if (ops->integer.get(interp, levelObj, &levelNum) != TCL_OK) {
    // Build error message
    FeatherObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
    msg = ops->string.concat(interp, msg, levelObj);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get current level for relative indexing
  size_t currentLevel = ops->frame.level(interp);

  // Handle level numbers:
  // - 0 means current level (special case in TCL)
  // - positive N means absolute level N
  // - negative N means relative: -1 is caller, -2 is caller's caller, etc.
  size_t targetLevel;
  if (levelNum == 0) {
    // 0 means current level
    targetLevel = currentLevel;
  } else if (levelNum < 0) {
    // Relative to current level: -1 means caller, -2 means caller's caller, etc.
    if ((size_t)(-levelNum) > currentLevel) {
      goto bad_level;
    }
    targetLevel = currentLevel + levelNum;
  } else {
    targetLevel = (size_t)levelNum;
  }

  // Validate level is in range
  size_t stackSize = ops->frame.size(interp);
  if (targetLevel >= stackSize) {
    goto bad_level;
  }

  // Get frame info
  FeatherObj cmd, frameArgs, frameNs;
  if (ops->frame.info(interp, targetLevel, &cmd, &frameArgs, &frameNs) != TCL_OK) {
    goto bad_level;
  }
  (void)frameNs; // Currently unused - info level doesn't include namespace

  // Build result list: {cmd arg1 arg2 ...}
  // Use display name for the command (strips :: for global namespace)
  FeatherObj result = ops->list.create(interp);
  FeatherObj displayCmd = feather_get_display_name(ops, interp, cmd);
  result = ops->list.push(interp, result, displayCmd);

  // Append all arguments
  size_t argCount = ops->list.length(interp, frameArgs);
  for (size_t i = 0; i < argCount; i++) {
    FeatherObj arg = ops->list.at(interp, frameArgs, i);
    result = ops->list.push(interp, result, arg);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;

bad_level:
  {
    FeatherObj msg = ops->string.intern(interp, "bad level \"", 11);
    msg = ops->string.concat(interp, msg, levelObj);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}

/**
 * info commands ?pattern?
 */
static FeatherResult info_commands(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info commands ?pattern?\"", 49));
    return TCL_ERROR;
  }

  // Get all command names from global namespace
  // Names are already simple (no :: prefix)
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj allNames = ops->ns.list_commands(interp, globalNs);
  size_t count = ops->list.length(interp, allNames);

  if (argc == 0) {
    // No pattern - return all commands (names are already simple)
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Filter by pattern
  FeatherObj pattern = ops->list.at(interp, args, 0);

  FeatherObj result = ops->list.create(interp);
  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);

    if (feather_obj_glob_match(ops, interp, pattern, name)) {
      result = ops->list.push(interp, result, name);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * info procs ?pattern?
 */
static FeatherResult info_procs(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info procs ?pattern?\"", 46));
    return TCL_ERROR;
  }

  // Get all command names from global namespace
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj allNames = ops->ns.list_commands(interp, globalNs);

  FeatherObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);

  // Optional pattern
  FeatherObj pattern = 0;
  if (argc == 1) {
    pattern = ops->list.at(interp, args, 0);
  }

  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);

    // Check if it's a user-defined procedure (not a builtin)
    // Use ns.get_command to check the type
    FeatherBuiltinCmd unusedFn = NULL;
    FeatherCommandType cmdType = ops->ns.get_command(interp, globalNs, name, &unusedFn, NULL, NULL);
    if (cmdType != TCL_CMD_PROC) {
      continue;
    }

    // Apply pattern filter if specified
    if (pattern != 0) {
      if (!feather_obj_glob_match(ops, interp, pattern, name)) {
        continue;
      }
    }

    result = ops->list.push(interp, result, name);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * Helper to resolve a proc name to its fully qualified form.
 * Tries the name as-is, then with :: prefix.
 * Returns the resolved name if found, or the original name if not found.
 */
static FeatherObj resolve_proc_name(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj procName) {
  // First try the name as-is
  if (feather_proc_exists(ops, interp, procName)) {
    return procName;
  }

  // Try with :: prefix
  FeatherObj qualified = ops->string.intern(interp, "::", 2);
  qualified = ops->string.concat(interp, qualified, procName);
  if (feather_proc_exists(ops, interp, qualified)) {
    return qualified;
  }

  // Return original name (will fail in caller)
  return procName;
}

/**
 * info body procname
 */
static FeatherResult info_body(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info body procname\"", 44));
    return TCL_ERROR;
  }

  FeatherObj procName = ops->list.at(interp, args, 0);
  FeatherObj resolvedName = resolve_proc_name(ops, interp, procName);

  // Check if it's a user-defined procedure and get its body
  FeatherObj body = 0;
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, resolvedName, NULL, NULL, &body);
  if (cmdType != TCL_CMD_PROC || body == 0) {
    FeatherObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, body);
  return TCL_OK;
}

/**
 * info args procname
 */
static FeatherResult info_args(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info args procname\"", 44));
    return TCL_ERROR;
  }

  FeatherObj procName = ops->list.at(interp, args, 0);
  FeatherObj resolvedName = resolve_proc_name(ops, interp, procName);

  // Check if it's a user-defined procedure and get its params
  FeatherObj params = 0;
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, resolvedName, NULL, &params, NULL);
  if (cmdType != TCL_CMD_PROC || params == 0) {
    FeatherObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, params);
  return TCL_OK;
}

/**
 * info frame ?number?
 *
 * With no argument, returns the current stack level (same as info level).
 * With a number, returns a dictionary with information about that frame:
 *   type: call (for proc call frames)
 *   cmd: the command being executed
 *   proc: the procedure name
 *   level: the stack level
 *   namespace: the namespace context
 */
static FeatherResult info_frame(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    // Return current frame depth
    size_t level = ops->frame.level(interp);
    ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)level));
    return TCL_OK;
  }

  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info frame ?number?\"", 45));
    return TCL_ERROR;
  }

  // Get level number
  FeatherObj levelObj = ops->list.at(interp, args, 0);
  int64_t levelNum;
  if (ops->integer.get(interp, levelObj, &levelNum) != TCL_OK) {
    FeatherObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
    msg = ops->string.concat(interp, msg, levelObj);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  size_t currentLevel = ops->frame.level(interp);
  size_t targetLevel;

  // Negative means relative offset from current
  if (levelNum < 0) {
    if ((size_t)(-levelNum) > currentLevel) {
      goto bad_level;
    }
    targetLevel = currentLevel + levelNum;
  } else {
    targetLevel = (size_t)levelNum;
  }

  size_t stackSize = ops->frame.size(interp);
  if (targetLevel >= stackSize) {
    goto bad_level;
  }

  // Get frame info
  FeatherObj cmd, frameArgs, frameNs;
  if (ops->frame.info(interp, targetLevel, &cmd, &frameArgs, &frameNs) != TCL_OK) {
    goto bad_level;
  }

  // Use display name for the command (strips :: for global namespace)
  FeatherObj displayCmd = feather_get_display_name(ops, interp, cmd);

  // Build result dictionary as a list: {key value key value ...}
  FeatherObj result = ops->list.create(interp);

  // type call
  result = ops->list.push(interp, result, ops->string.intern(interp, "type", 4));
  result = ops->list.push(interp, result, ops->string.intern(interp, "call", 4));

  // cmd {cmdname arg1 arg2 ...}
  result = ops->list.push(interp, result, ops->string.intern(interp, "cmd", 3));
  FeatherObj cmdList = ops->list.create(interp);
  cmdList = ops->list.push(interp, cmdList, displayCmd);
  size_t argCount = ops->list.length(interp, frameArgs);
  for (size_t i = 0; i < argCount; i++) {
    cmdList = ops->list.push(interp, cmdList, ops->list.at(interp, frameArgs, i));
  }
  result = ops->list.push(interp, result, cmdList);

  // proc name
  result = ops->list.push(interp, result, ops->string.intern(interp, "proc", 4));
  result = ops->list.push(interp, result, displayCmd);

  // level number
  result = ops->list.push(interp, result, ops->string.intern(interp, "level", 5));
  result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)targetLevel));

  // namespace
  result = ops->list.push(interp, result, ops->string.intern(interp, "namespace", 9));
  result = ops->list.push(interp, result, frameNs);

  ops->interp.set_result(interp, result);
  return TCL_OK;

bad_level:
  {
    FeatherObj msg = ops->string.intern(interp, "bad level \"", 11);
    msg = ops->string.concat(interp, msg, levelObj);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}

/**
 * info default procname arg varname
 *
 * Returns 1 if the parameter has a default value, storing it in varname.
 * Returns 0 if the parameter has no default.
 * Errors if the proc doesn't exist or arg isn't a parameter.
 */
static FeatherResult info_default(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 3) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info default procname arg varname\"", 59));
    return TCL_ERROR;
  }

  FeatherObj procName = ops->list.at(interp, args, 0);
  FeatherObj argName = ops->list.at(interp, args, 1);
  FeatherObj varName = ops->list.at(interp, args, 2);

  FeatherObj resolvedName = resolve_proc_name(ops, interp, procName);

  // Check if it's a user-defined procedure and get its params
  FeatherObj params = 0;
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, resolvedName, NULL, &params, NULL);
  if (cmdType != TCL_CMD_PROC || params == 0) {
    FeatherObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Search for the parameter
  size_t paramCount = ops->list.length(interp, params);
  for (size_t i = 0; i < paramCount; i++) {
    FeatherObj param = ops->list.at(interp, params, i);
    // Param can be a name or {name default}
    FeatherObj paramList = ops->list.from(interp, param);
    size_t paramLen = ops->list.length(interp, paramList);

    FeatherObj paramName;
    if (paramLen >= 1) {
      paramName = ops->list.at(interp, paramList, 0);
    } else {
      paramName = param;
    }

    if (ops->string.equal(interp, argName, paramName)) {
      // Found the parameter
      if (paramLen >= 2) {
        // Has default value
        FeatherObj defaultVal = ops->list.at(interp, paramList, 1);
        ops->var.set(interp, varName, defaultVal);
        ops->interp.set_result(interp, ops->integer.create(interp, 1));
      } else {
        // No default
        ops->var.set(interp, varName, ops->string.intern(interp, "", 0));
        ops->interp.set_result(interp, ops->integer.create(interp, 0));
      }
      return TCL_OK;
    }
  }

  // Parameter not found
  FeatherObj msg = ops->string.intern(interp, "procedure \"", 11);
  msg = ops->string.concat(interp, msg, procName);
  msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\" doesn't have an argument \"", 28));
  msg = ops->string.concat(interp, msg, argName);
  msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}

/**
 * info locals ?pattern?
 *
 * Returns a list of local variable names in the current frame.
 */
static FeatherResult info_locals(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info locals ?pattern?\"", 47));
    return TCL_ERROR;
  }

  // Get all local variable names (pass nil for current frame)
  FeatherObj allNames = ops->var.names(interp, 0);

  if (argc == 0) {
    // No pattern - return all locals
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Filter by pattern
  FeatherObj pattern = ops->list.at(interp, args, 0);

  FeatherObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);
  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);

    if (feather_obj_glob_match(ops, interp, pattern, name)) {
      result = ops->list.push(interp, result, name);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * info globals ?pattern?
 *
 * Returns a list of global variable names.
 */
static FeatherResult info_globals(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info globals ?pattern?\"", 48));
    return TCL_ERROR;
  }

  // Get all global variable names (pass "::" for global namespace)
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj allNames = ops->var.names(interp, globalNs);

  if (argc == 0) {
    // No pattern - return all globals
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Filter by pattern
  FeatherObj pattern = ops->list.at(interp, args, 0);

  FeatherObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);
  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);

    if (feather_obj_glob_match(ops, interp, pattern, name)) {
      result = ops->list.push(interp, result, name);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * info vars ?pattern?
 *
 * Returns a list of visible variable names (locals + globals via upvar/global).
 * This is the same as info locals for now since we don't have a separate
 * mechanism to track which globals have been imported.
 */
static FeatherResult info_vars(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  // For now, info vars behaves like info locals
  // In a full implementation, it would also include globals that have
  // been made visible via 'global' or 'upvar'
  return info_locals(ops, interp, args);
}

/**
 * info script
 *
 * Returns the name of the script file currently being evaluated.
 * Returns empty string if not sourcing a file.
 */
static FeatherResult info_script(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info script\"", 37));
    return TCL_ERROR;
  }

  FeatherObj scriptPath = ops->interp.get_script(interp);
  ops->interp.set_result(interp, scriptPath);
  return TCL_OK;
}

/**
 * info type value
 *
 * Returns the type name of a value:
 * - For foreign objects: the registered type name (e.g., "Mux", "Connection")
 * - For lists: "list"
 * - For dicts: "dict"
 * - For integers: "int"
 * - For doubles: "double"
 * - For strings: "string"
 */
static FeatherResult info_type(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info type value\"", 41));
    return TCL_ERROR;
  }

  FeatherObj value = ops->list.at(interp, args, 0);

  // Check if it's a foreign object first
  if (ops->foreign.is_foreign(interp, value)) {
    FeatherObj typeName = ops->foreign.type_name(interp, value);
    if (!ops->list.is_nil(interp, typeName)) {
      ops->interp.set_result(interp, typeName);
      return TCL_OK;
    }
  }

  // For non-foreign objects, return the basic type
  // We check in order of specificity

  // Check if it's natively a dict first (before list, since dicts can shimmer to lists)
  if (ops->dict.is_dict(interp, value)) {
    ops->interp.set_result(interp, ops->string.intern(interp, "dict", 4));
    return TCL_OK;
  }

  // Check if it's an integer
  int64_t intVal;
  if (ops->integer.get(interp, value, &intVal) == TCL_OK) {
    ops->interp.set_result(interp, ops->string.intern(interp, "int", 3));
    return TCL_OK;
  }

  // Check if it's a double
  double dblVal;
  if (ops->dbl.get(interp, value, &dblVal) == TCL_OK) {
    // Only return "double" if it looks like a float (has decimal point or e)
    int isFloat = feather_obj_contains_char(ops, interp, value, '.') ||
                  feather_obj_contains_char(ops, interp, value, 'e') ||
                  feather_obj_contains_char(ops, interp, value, 'E');
    if (isFloat) {
      ops->interp.set_result(interp, ops->string.intern(interp, "double", 6));
      return TCL_OK;
    }
  }

  // Check if it's a list (more than one element)
  FeatherObj asList = ops->list.from(interp, value);
  size_t listLen = ops->list.length(interp, asList);
  if (listLen > 1) {
    ops->interp.set_result(interp, ops->string.intern(interp, "list", 4));
    return TCL_OK;
  }

  // Default: it's a string
  ops->interp.set_result(interp, ops->string.intern(interp, "string", 6));
  return TCL_OK;
}

/**
 * info methods value
 *
 * Returns a list of method names available on a foreign object.
 * Returns empty list for non-foreign objects.
 */
static FeatherResult info_methods(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info methods value\"", 44));
    return TCL_ERROR;
  }

  FeatherObj value = ops->list.at(interp, args, 0);

  // Get methods from foreign ops (returns empty list for non-foreign)
  FeatherObj methods = ops->foreign.methods(interp, value);
  if (ops->list.is_nil(interp, methods)) {
    methods = ops->list.create(interp);
  }

  ops->interp.set_result(interp, methods);
  return TCL_OK;
}

FeatherResult feather_builtin_info(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info subcommand ?arg ...?\"", 51));
    return TCL_ERROR;
  }

  // Get subcommand
  FeatherObj subcmd = ops->list.shift(interp, args);

  if (feather_obj_eq_literal(ops, interp, subcmd, "exists")) {
    return info_exists(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "level")) {
    return info_level(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "commands")) {
    return info_commands(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "procs")) {
    return info_procs(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "body")) {
    return info_body(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "args")) {
    return info_args(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "frame")) {
    return info_frame(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "default")) {
    return info_default(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "locals")) {
    return info_locals(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "globals")) {
    return info_globals(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "vars")) {
    return info_vars(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "script")) {
    return info_script(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "type")) {
    return info_type(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "methods")) {
    return info_methods(ops, interp, args);
  }

  // Unknown subcommand
  FeatherObj msg = ops->string.intern(
      interp,
      "unknown or ambiguous subcommand \"", 33);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(
      interp, msg,
      ops->string.intern(interp, "\": must be args, body, commands, default, exists, frame, globals, level, locals, methods, procs, script, type, or vars", 118));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
