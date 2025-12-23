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
  TclResult exists = ops->var.exists(interp, varName);

  ops->interp.set_result(interp,
                         ops->integer.create(interp, exists == TCL_OK ? 1 : 0));
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
  TclObj cmd, frameArgs;
  if (ops->frame.info(interp, targetLevel, &cmd, &frameArgs) != TCL_OK) {
    goto bad_level;
  }

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

  // Check if it's a user-defined procedure
  if (!ops->proc.exists(interp, procName)) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj body;
  if (ops->proc.body(interp, procName, &body) != TCL_OK) {
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

  // Check if it's a user-defined procedure
  if (!ops->proc.exists(interp, procName)) {
    TclObj msg = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, procName);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\" isn't a procedure", 19));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj params;
  if (ops->proc.params(interp, procName, &params) != TCL_OK) {
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

  // Unknown subcommand
  TclObj msg = ops->string.intern(
      interp,
      "unknown or ambiguous subcommand \"", 33);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(
      interp, msg,
      ops->string.intern(interp, "\": must be args, body, commands, exists, level, or procs", 56));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
