#include "internal.h"
#include "tclc.h"

/**
 * Helper to check if a string equals a literal.
 */
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t lit_len = tcl_strlen(lit);
  if (len != lit_len) {
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) {
      return 0;
    }
  }
  return 1;
}

/**
 * info exists varName
 */
static TclResult info_exists(const TclHostOps *ops, TclInterp interp,
                             TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info exists varName\"", 45));
    return TCL_ERROR;
  }

  TclObj varName = ops->list.at(interp, args, 0);
  size_t nameLen;
  const char *nameStr = ops->string.get(interp, varName, &nameLen);

  // Resolve the variable name (handles qualified names)
  TclObj ns, localName;
  tcl_resolve_variable(ops, interp, nameStr, nameLen, &ns, &localName);

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
static TclResult info_level(const TclHostOps *ops, TclInterp interp,
                            TclObj args) {
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
  TclObj levelObj = ops->list.at(interp, args, 0);
  int64_t levelNum;
  if (ops->integer.get(interp, levelObj, &levelNum) != TCL_OK) {
    size_t len;
    const char *s = ops->string.get(interp, levelObj, &len);
    // Build error message
    TclObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
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
  TclObj cmd, frameArgs, frameNs;
  if (ops->frame.info(interp, targetLevel, &cmd, &frameArgs, &frameNs) != TCL_OK) {
    goto bad_level;
  }
  (void)frameNs; // Currently unused - info level doesn't include namespace

  // Build result list: {cmd arg1 arg2 ...}
  TclObj result = ops->list.create(interp);
  result = ops->list.push(interp, result, cmd);

  // Append all arguments
  size_t argCount = ops->list.length(interp, frameArgs);
  for (size_t i = 0; i < argCount; i++) {
    TclObj arg = ops->list.at(interp, frameArgs, i);
    result = ops->list.push(interp, result, arg);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;

bad_level:
  {
    size_t len;
    const char *s = ops->string.get(interp, levelObj, &len);
    TclObj msg = ops->string.intern(interp, "bad level \"", 11);
    msg = ops->string.concat(interp, msg, levelObj);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}

/**
 * info commands ?pattern?
 */
static TclResult info_commands(const TclHostOps *ops, TclInterp interp,
                               TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info commands ?pattern?\"", 49));
    return TCL_ERROR;
  }

  // Get all command names
  TclObj allNames = ops->proc.names(interp, 0);

  if (argc == 0) {
    // No pattern - return all commands
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Filter by pattern
  TclObj pattern = ops->list.at(interp, args, 0);
  size_t patLen;
  const char *patStr = ops->string.get(interp, pattern, &patLen);

  TclObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);
  for (size_t i = 0; i < count; i++) {
    TclObj name = ops->list.at(interp, allNames, i);
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);

    if (tcl_glob_match(patStr, patLen, nameStr, nameLen)) {
      result = ops->list.push(interp, result, name);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * info procs ?pattern?
 */
static TclResult info_procs(const TclHostOps *ops, TclInterp interp,
                            TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info procs ?pattern?\"", 46));
    return TCL_ERROR;
  }

  // Get all command names
  TclObj allNames = ops->proc.names(interp, 0);

  TclObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);

  // Optional pattern
  const char *patStr = NULL;
  size_t patLen = 0;
  if (argc == 1) {
    TclObj pattern = ops->list.at(interp, args, 0);
    patStr = ops->string.get(interp, pattern, &patLen);
  }

  for (size_t i = 0; i < count; i++) {
    TclObj name = ops->list.at(interp, allNames, i);

    // Check if it's a user-defined procedure (not a builtin)
    if (!ops->proc.exists(interp, name)) {
      continue;
    }

    // Apply pattern filter if specified
    if (patStr != NULL) {
      size_t nameLen;
      const char *nameStr = ops->string.get(interp, name, &nameLen);
      if (!tcl_glob_match(patStr, patLen, nameStr, nameLen)) {
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
static TclObj resolve_proc_name(const TclHostOps *ops, TclInterp interp,
                                TclObj procName) {
  // First try the name as-is
  if (ops->proc.exists(interp, procName)) {
    return procName;
  }

  // Try with :: prefix
  TclObj qualified = ops->string.intern(interp, "::", 2);
  qualified = ops->string.concat(interp, qualified, procName);
  if (ops->proc.exists(interp, qualified)) {
    return qualified;
  }

  // Return original name (will fail in caller)
  return procName;
}

/**
 * info body procname
 */
static TclResult info_body(const TclHostOps *ops, TclInterp interp,
                           TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info body procname\"", 44));
    return TCL_ERROR;
  }

  TclObj procName = ops->list.at(interp, args, 0);
  TclObj resolvedName = resolve_proc_name(ops, interp, procName);

  // Check if it's a user-defined procedure
  if (!ops->proc.exists(interp, resolvedName)) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj body;
  if (ops->proc.body(interp, resolvedName, &body) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
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
static TclResult info_args(const TclHostOps *ops, TclInterp interp,
                           TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info args procname\"", 44));
    return TCL_ERROR;
  }

  TclObj procName = ops->list.at(interp, args, 0);
  TclObj resolvedName = resolve_proc_name(ops, interp, procName);

  // Check if it's a user-defined procedure
  if (!ops->proc.exists(interp, resolvedName)) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj params;
  if (ops->proc.params(interp, resolvedName, &params) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
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
static TclResult info_frame(const TclHostOps *ops, TclInterp interp,
                            TclObj args) {
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
  TclObj levelObj = ops->list.at(interp, args, 0);
  int64_t levelNum;
  if (ops->integer.get(interp, levelObj, &levelNum) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
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
  TclObj cmd, frameArgs, frameNs;
  if (ops->frame.info(interp, targetLevel, &cmd, &frameArgs, &frameNs) != TCL_OK) {
    goto bad_level;
  }

  // Build result dictionary as a list: {key value key value ...}
  TclObj result = ops->list.create(interp);

  // type call
  result = ops->list.push(interp, result, ops->string.intern(interp, "type", 4));
  result = ops->list.push(interp, result, ops->string.intern(interp, "call", 4));

  // cmd {cmdname arg1 arg2 ...}
  result = ops->list.push(interp, result, ops->string.intern(interp, "cmd", 3));
  TclObj cmdList = ops->list.create(interp);
  cmdList = ops->list.push(interp, cmdList, cmd);
  size_t argCount = ops->list.length(interp, frameArgs);
  for (size_t i = 0; i < argCount; i++) {
    cmdList = ops->list.push(interp, cmdList, ops->list.at(interp, frameArgs, i));
  }
  result = ops->list.push(interp, result, cmdList);

  // proc name
  result = ops->list.push(interp, result, ops->string.intern(interp, "proc", 4));
  result = ops->list.push(interp, result, cmd);

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
    TclObj msg = ops->string.intern(interp, "bad level \"", 11);
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
static TclResult info_default(const TclHostOps *ops, TclInterp interp,
                              TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 3) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info default procname arg varname\"", 59));
    return TCL_ERROR;
  }

  TclObj procName = ops->list.at(interp, args, 0);
  TclObj argName = ops->list.at(interp, args, 1);
  TclObj varName = ops->list.at(interp, args, 2);

  TclObj resolvedName = resolve_proc_name(ops, interp, procName);

  // Check if it's a user-defined procedure
  if (!ops->proc.exists(interp, resolvedName)) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj params;
  if (ops->proc.params(interp, resolvedName, &params) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Search for the parameter
  size_t argNameLen;
  const char *argNameStr = ops->string.get(interp, argName, &argNameLen);

  size_t paramCount = ops->list.length(interp, params);
  for (size_t i = 0; i < paramCount; i++) {
    TclObj param = ops->list.at(interp, params, i);
    // Param can be a name or {name default}
    TclObj paramList = ops->list.from(interp, param);
    size_t paramLen = ops->list.length(interp, paramList);

    TclObj paramName;
    if (paramLen >= 1) {
      paramName = ops->list.at(interp, paramList, 0);
    } else {
      paramName = param;
    }

    size_t pnameLen;
    const char *pnameStr = ops->string.get(interp, paramName, &pnameLen);

    if (argNameLen == pnameLen) {
      int match = 1;
      for (size_t j = 0; j < argNameLen; j++) {
        if (argNameStr[j] != pnameStr[j]) {
          match = 0;
          break;
        }
      }
      if (match) {
        // Found the parameter
        if (paramLen >= 2) {
          // Has default value
          TclObj defaultVal = ops->list.at(interp, paramList, 1);
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
  }

  // Parameter not found
  TclObj msg = ops->string.intern(interp, "procedure \"", 11);
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
static TclResult info_locals(const TclHostOps *ops, TclInterp interp,
                             TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info locals ?pattern?\"", 47));
    return TCL_ERROR;
  }

  // Get all local variable names (pass nil for current frame)
  TclObj allNames = ops->var.names(interp, 0);

  if (argc == 0) {
    // No pattern - return all locals
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Filter by pattern
  TclObj pattern = ops->list.at(interp, args, 0);
  size_t patLen;
  const char *patStr = ops->string.get(interp, pattern, &patLen);

  TclObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);
  for (size_t i = 0; i < count; i++) {
    TclObj name = ops->list.at(interp, allNames, i);
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);

    if (tcl_glob_match(patStr, patLen, nameStr, nameLen)) {
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
static TclResult info_globals(const TclHostOps *ops, TclInterp interp,
                              TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info globals ?pattern?\"", 48));
    return TCL_ERROR;
  }

  // Get all global variable names (pass "::" for global namespace)
  TclObj globalNs = ops->string.intern(interp, "::", 2);
  TclObj allNames = ops->var.names(interp, globalNs);

  if (argc == 0) {
    // No pattern - return all globals
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Filter by pattern
  TclObj pattern = ops->list.at(interp, args, 0);
  size_t patLen;
  const char *patStr = ops->string.get(interp, pattern, &patLen);

  TclObj result = ops->list.create(interp);
  size_t count = ops->list.length(interp, allNames);
  for (size_t i = 0; i < count; i++) {
    TclObj name = ops->list.at(interp, allNames, i);
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);

    if (tcl_glob_match(patStr, patLen, nameStr, nameLen)) {
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
static TclResult info_vars(const TclHostOps *ops, TclInterp interp,
                           TclObj args) {
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
static TclResult info_script(const TclHostOps *ops, TclInterp interp,
                             TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info script\"", 37));
    return TCL_ERROR;
  }

  TclObj scriptPath = ops->interp.get_script(interp);
  ops->interp.set_result(interp, scriptPath);
  return TCL_OK;
}

TclResult tcl_builtin_info(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info subcommand ?arg ...?\"", 51));
    return TCL_ERROR;
  }

  // Get subcommand
  TclObj subcmd = ops->list.shift(interp, args);
  size_t subcmdLen;
  const char *subcmdStr = ops->string.get(interp, subcmd, &subcmdLen);

  if (str_eq(subcmdStr, subcmdLen, "exists")) {
    return info_exists(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "level")) {
    return info_level(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "commands")) {
    return info_commands(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "procs")) {
    return info_procs(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "body")) {
    return info_body(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "args")) {
    return info_args(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "frame")) {
    return info_frame(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "default")) {
    return info_default(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "locals")) {
    return info_locals(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "globals")) {
    return info_globals(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "vars")) {
    return info_vars(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "script")) {
    return info_script(ops, interp, args);
  }

  // Unknown subcommand
  TclObj msg = ops->string.intern(
      interp,
      "unknown or ambiguous subcommand \"", 33);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(
      interp, msg,
      ops->string.intern(interp, "\": must be args, body, commands, default, exists, frame, globals, level, locals, procs, script, or vars", 103));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
