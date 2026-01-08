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

  // feather_var_exists handles qualified names
  int exists = feather_var_exists(ops, interp, varName);

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
    feather_error_expected(ops, interp, "integer", levelObj);
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
 * Helper to parse a pattern into namespace and simple pattern parts.
 *
 * For "::foo::bar*", returns ns="::foo", pattern="bar*"
 * For "bar*" (unqualified), returns ns=current namespace, pattern="bar*"
 * For "::bar*" (global), returns ns="::", pattern="bar*"
 */
static void parse_pattern_namespace(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj fullPattern,
                                    FeatherObj *nsOut, FeatherObj *patternOut) {
  // Check if pattern is qualified
  if (feather_obj_is_qualified(ops, interp, fullPattern)) {
    // Split into namespace and simple pattern
    feather_obj_split_command(ops, interp, fullPattern, nsOut, patternOut);
    if (ops->list.is_nil(interp, *nsOut)) {
      *nsOut = ops->string.intern(interp, "::", 2);
    }
  } else {
    // Unqualified pattern - use current namespace
    *nsOut = ops->ns.current(interp);
    *patternOut = fullPattern;
  }
}

/**
 * info commands ?pattern?
 *
 * Returns names of all commands visible in the current namespace.
 * If pattern is given, returns only those names that match.
 * Only the last component of pattern is a pattern - other components identify a namespace.
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

  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj currentNs = ops->ns.current(interp);
  int inGlobalNs = feather_obj_is_global_ns(ops, interp, currentNs);

  if (argc == 0) {
    // No pattern - return all visible commands
    // Visible = current namespace + global namespace (merged, no duplicates)
    FeatherObj result = ops->list.create(interp);

    // First add commands from current namespace
    FeatherObj currentNames = ops->ns.list_commands(interp, currentNs);
    size_t currentCount = ops->list.length(interp, currentNames);
    for (size_t i = 0; i < currentCount; i++) {
      result = ops->list.push(interp, result, ops->list.at(interp, currentNames, i));
    }

    // If not in global namespace, also add global commands (avoiding duplicates)
    if (!inGlobalNs) {
      FeatherObj globalNames = ops->ns.list_commands(interp, globalNs);
      size_t globalCount = ops->list.length(interp, globalNames);
      for (size_t i = 0; i < globalCount; i++) {
        FeatherObj name = ops->list.at(interp, globalNames, i);
        // Check if already in result (from current namespace)
        int found = 0;
        for (size_t j = 0; j < currentCount; j++) {
          if (ops->string.equal(interp, name, ops->list.at(interp, currentNames, j))) {
            found = 1;
            break;
          }
        }
        if (!found) {
          result = ops->list.push(interp, result, name);
        }
      }
    }

    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Pattern specified - parse namespace and pattern parts
  FeatherObj fullPattern = ops->list.at(interp, args, 0);
  FeatherObj searchNs, pattern;
  parse_pattern_namespace(ops, interp, fullPattern, &searchNs, &pattern);

  int patternIsQualified = feather_obj_is_qualified(ops, interp, fullPattern);

  // Get commands from the target namespace
  FeatherObj allNames = ops->ns.list_commands(interp, searchNs);
  size_t count = ops->list.length(interp, allNames);

  FeatherObj result = ops->list.create(interp);
  FeatherObj colons = ops->string.intern(interp, "::", 2);

  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);
    if (feather_obj_glob_match(ops, interp, pattern, name)) {
      // When pattern was qualified, return fully qualified names
      if (patternIsQualified) {
        FeatherObj qualifiedName;
        if (feather_obj_is_global_ns(ops, interp, searchNs)) {
          qualifiedName = ops->string.concat(interp, colons, name);
        } else {
          qualifiedName = ops->string.concat(interp, searchNs, colons);
          qualifiedName = ops->string.concat(interp, qualifiedName, name);
        }
        result = ops->list.push(interp, result, qualifiedName);
      } else {
        result = ops->list.push(interp, result, name);
      }
    }
  }

  // If searching current namespace (unqualified pattern) and not in global,
  // also search global namespace for matches
  if (!patternIsQualified && !inGlobalNs) {
    FeatherObj globalNames = ops->ns.list_commands(interp, globalNs);
    size_t globalCount = ops->list.length(interp, globalNames);
    for (size_t i = 0; i < globalCount; i++) {
      FeatherObj name = ops->list.at(interp, globalNames, i);
      if (feather_obj_glob_match(ops, interp, pattern, name)) {
        // Check if already in result (from current namespace)
        int found = 0;
        size_t resultLen = ops->list.length(interp, result);
        for (size_t j = 0; j < resultLen; j++) {
          if (ops->string.equal(interp, name, ops->list.at(interp, result, j))) {
            found = 1;
            break;
          }
        }
        if (!found) {
          result = ops->list.push(interp, result, name);
        }
      }
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * Helper to add procs from a namespace to result list, filtering by pattern.
 * If qualifyOutput is true, returns fully qualified names.
 * Avoids adding duplicates (checks if name already in result).
 */
static void add_procs_from_ns(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj ns, FeatherObj pattern, FeatherObj result,
                              int qualifyOutput) {
  FeatherObj allNames = ops->ns.list_commands(interp, ns);
  size_t count = ops->list.length(interp, allNames);
  FeatherObj colons = ops->string.intern(interp, "::", 2);
  int nsIsGlobal = feather_obj_is_global_ns(ops, interp, ns);

  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);

    // Check if it's a user-defined procedure (not a builtin)
    FeatherBuiltinCmd unusedFn = NULL;
    FeatherCommandType cmdType = ops->ns.get_command(interp, ns, name, &unusedFn, NULL, NULL);
    if (cmdType != TCL_CMD_PROC) {
      continue;
    }

    // Apply pattern filter if specified
    if (!ops->list.is_nil(interp, pattern)) {
      if (!feather_obj_glob_match(ops, interp, pattern, name)) {
        continue;
      }
    }

    // Build output name (qualified or simple)
    FeatherObj outputName = name;
    if (qualifyOutput) {
      if (nsIsGlobal) {
        outputName = ops->string.concat(interp, colons, name);
      } else {
        outputName = ops->string.concat(interp, ns, colons);
        outputName = ops->string.concat(interp, outputName, name);
      }
    }

    // Check if already in result (avoid duplicates)
    size_t resultLen = ops->list.length(interp, result);
    int found = 0;
    for (size_t j = 0; j < resultLen; j++) {
      if (ops->string.equal(interp, outputName, ops->list.at(interp, result, j))) {
        found = 1;
        break;
      }
    }
    if (!found) {
      ops->list.push(interp, result, outputName);
    }
  }
}

/**
 * info procs ?pattern?
 *
 * Returns names of all visible procedures (user-defined procs, not builtins).
 * If pattern is given, returns only those names that match.
 * Only the final component in pattern is a pattern - other components identify a namespace.
 * When pattern is qualified, results are fully qualified.
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

  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj currentNs = ops->ns.current(interp);
  int inGlobalNs = feather_obj_is_global_ns(ops, interp, currentNs);

  FeatherObj result = ops->list.create(interp);

  if (argc == 0) {
    // No pattern - return all visible procs (simple names)
    // Add procs from current namespace first
    add_procs_from_ns(ops, interp, currentNs, 0, result, 0);

    // If not in global namespace, also add global procs
    if (!inGlobalNs) {
      add_procs_from_ns(ops, interp, globalNs, 0, result, 0);
    }

    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Pattern specified - parse namespace and pattern parts
  FeatherObj fullPattern = ops->list.at(interp, args, 0);
  FeatherObj searchNs, pattern;
  parse_pattern_namespace(ops, interp, fullPattern, &searchNs, &pattern);

  int patternIsQualified = feather_obj_is_qualified(ops, interp, fullPattern);

  // Add procs from the target namespace
  add_procs_from_ns(ops, interp, searchNs, pattern, result, patternIsQualified);

  // If searching current namespace (unqualified pattern) and not in global,
  // also search global namespace
  if (!patternIsQualified && !inGlobalNs) {
    add_procs_from_ns(ops, interp, globalNs, pattern, result, 0);
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
 * With no argument, returns the current frame depth.
 * With a number, returns a dictionary with information about that frame:
 *   type: proc, eval, or source
 *   cmd: the command being executed (as a list)
 *   proc: the procedure name (only if type is proc)
 *   level: the stack level
 *   file: the script file path (only if type is source)
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
    feather_error_expected(ops, interp, "integer", levelObj);
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

  // Determine frame type: check if command is a user-defined proc
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, cmd, NULL, NULL, NULL);
  int isProc = (cmdType == TCL_CMD_PROC);

  // Check if we're in a source context
  FeatherObj scriptPath = ops->interp.get_script(interp);
  int hasScriptPath = (ops->string.byte_length(interp, scriptPath) > 0);

  // Determine type string
  const char *typeStr;
  size_t typeLen;
  if (isProc) {
    typeStr = "proc";
    typeLen = 4;
  } else if (hasScriptPath) {
    typeStr = "source";
    typeLen = 6;
  } else {
    typeStr = "eval";
    typeLen = 4;
  }

  // Build result dictionary as a list: {key value key value ...}
  FeatherObj result = ops->list.create(interp);

  // type
  result = ops->list.push(interp, result, ops->string.intern(interp, "type", 4));
  result = ops->list.push(interp, result, ops->string.intern(interp, typeStr, typeLen));

  // cmd {cmdname arg1 arg2 ...}
  result = ops->list.push(interp, result, ops->string.intern(interp, "cmd", 3));
  FeatherObj cmdList = ops->list.create(interp);
  cmdList = ops->list.push(interp, cmdList, displayCmd);
  size_t argCount = ops->list.length(interp, frameArgs);
  for (size_t i = 0; i < argCount; i++) {
    cmdList = ops->list.push(interp, cmdList, ops->list.at(interp, frameArgs, i));
  }
  result = ops->list.push(interp, result, cmdList);

  // proc name (only if type is proc)
  if (isProc) {
    result = ops->list.push(interp, result, ops->string.intern(interp, "proc", 4));
    result = ops->list.push(interp, result, displayCmd);
  }

  // level number
  result = ops->list.push(interp, result, ops->string.intern(interp, "level", 5));
  result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)targetLevel));

  // file (only if type is source)
  if (hasScriptPath) {
    result = ops->list.push(interp, result, ops->string.intern(interp, "file", 4));
    result = ops->list.push(interp, result, scriptPath);
  }

  // namespace
  result = ops->list.push(interp, result, ops->string.intern(interp, "namespace", 9));
  result = ops->list.push(interp, result, frameNs);

  // line (if available)
  size_t lineNum = ops->frame.get_line(interp, targetLevel);
  if (lineNum > 0) {
    result = ops->list.push(interp, result, ops->string.intern(interp, "line", 4));
    result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)lineNum));
  }

  // lambda (only for apply frames)
  FeatherObj lambda = ops->frame.get_lambda(interp, targetLevel);
  if (!ops->list.is_nil(interp, lambda)) {
    result = ops->list.push(interp, result, ops->string.intern(interp, "lambda", 6));
    result = ops->list.push(interp, result, lambda);
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
      FeatherResult res;
      if (paramLen >= 2) {
        // Has default value
        FeatherObj defaultVal = ops->list.at(interp, paramList, 1);
        res = feather_set_var(ops, interp, varName, defaultVal);
        if (res != TCL_OK) return res;
        ops->interp.set_result(interp, ops->integer.create(interp, 1));
      } else {
        // No default
        res = feather_set_var(ops, interp, varName, ops->string.intern(interp, "", 0));
        if (res != TCL_OK) return res;
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
 * Excludes variables linked via global, upvar, or variable commands.
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

  // Get all variable names in current frame (includes linked)
  FeatherObj allNames = ops->var.names(interp, 0);
  size_t count = ops->list.length(interp, allNames);

  // Optional pattern
  FeatherObj pattern = 0;
  if (argc == 1) {
    pattern = ops->list.at(interp, args, 0);
  }

  // Filter out linked variables and apply pattern
  FeatherObj result = ops->list.create(interp);
  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);

    // Skip linked variables (upvar, global, variable)
    if (ops->var.is_link(interp, name)) {
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
 * Returns a list of all visible variable names.
 * If pattern contains namespace qualifiers, searches that namespace and
 * returns fully qualified names.
 * Otherwise returns current frame variables (locals + linked via upvar/global/variable).
 */
static FeatherResult info_vars(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"info vars ?pattern?\"", 45));
    return TCL_ERROR;
  }

  if (argc == 0) {
    // No pattern - return all visible variables in current frame
    FeatherObj allNames = ops->var.names(interp, 0);
    ops->interp.set_result(interp, allNames);
    return TCL_OK;
  }

  // Pattern specified
  FeatherObj fullPattern = ops->list.at(interp, args, 0);

  // Check if pattern is namespace-qualified
  if (feather_obj_is_qualified(ops, interp, fullPattern)) {
    // Split into namespace and pattern parts
    FeatherObj searchNs, pattern;
    parse_pattern_namespace(ops, interp, fullPattern, &searchNs, &pattern);

    // Get variables from the target namespace
    FeatherObj allNames = ops->var.names(interp, searchNs);
    size_t count = ops->list.length(interp, allNames);

    FeatherObj result = ops->list.create(interp);
    FeatherObj colons = ops->string.intern(interp, "::", 2);
    int nsIsGlobal = feather_obj_is_global_ns(ops, interp, searchNs);
    for (size_t i = 0; i < count; i++) {
      FeatherObj name = ops->list.at(interp, allNames, i);
      if (feather_obj_glob_match(ops, interp, pattern, name)) {
        // Return fully qualified names when pattern was qualified
        FeatherObj qualifiedName;
        if (nsIsGlobal) {
          // Global namespace: "::x"
          qualifiedName = ops->string.concat(interp, colons, name);
        } else {
          // Other namespace: "::foo::x"
          qualifiedName = ops->string.concat(interp, searchNs, colons);
          qualifiedName = ops->string.concat(interp, qualifiedName, name);
        }
        result = ops->list.push(interp, result, qualifiedName);
      }
    }

    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Unqualified pattern - search current frame variables
  FeatherObj allNames = ops->var.names(interp, 0);
  size_t count = ops->list.length(interp, allNames);

  FeatherObj result = ops->list.create(interp);
  for (size_t i = 0; i < count; i++) {
    FeatherObj name = ops->list.at(interp, allNames, i);
    if (feather_obj_glob_match(ops, interp, fullPattern, name)) {
      result = ops->list.push(interp, result, name);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
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

void feather_register_info_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj e, subspec;

  e = feather_usage_about(ops, interp,
    "Information about the state of the interpreter",
    "Provides runtime introspection capabilities including information about "
    "variables, procedures, commands, call stack, namespaces, and values.\n\n"
    "Pattern arguments use glob-style matching as supported by the string match "
    "command. For commands and variables, if the pattern contains :: it is treated "
    "as a qualified name where only the final component is used as a pattern.\n\n"
    "Note: The type and methods subcommands are Feather-specific extensions not "
    "found in standard TCL.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info args procname
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<procname>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "args", subspec);
  e = feather_usage_help(ops, interp, e, "Get argument names of a procedure");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a list containing the names of the arguments to procedure procname, "
    "in order.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info body procname
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<procname>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "body", subspec);
  e = feather_usage_help(ops, interp, e, "Get body of a procedure");
  e = feather_usage_long_help(ops, interp, e,
    "Returns the body of procedure procname. Procname must be the name of a TCL "
    "command procedure.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info commands ?pattern?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?pattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "commands", subspec);
  e = feather_usage_help(ops, interp, e, "List available commands");
  e = feather_usage_long_help(ops, interp, e,
    "Returns the names of all commands visible in the current namespace. If "
    "pattern is given, returns only those names that match according to string "
    "match. Only the last component of pattern is a pattern. Other components "
    "identify a namespace. See NAMESPACE RESOLUTION in the namespace "
    "documentation.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info default procname arg varname
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<procname>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<parameter>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<varname>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "default", subspec);
  e = feather_usage_help(ops, interp, e, "Get default value of a procedure argument");
  e = feather_usage_long_help(ops, interp, e,
    "If the parameter parameter for the procedure named procname has a default "
    "value, stores that value in varname and returns 1. Otherwise, returns 0.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info exists varName
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<varName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "exists", subspec);
  e = feather_usage_help(ops, interp, e, "Check if a variable exists");
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if a variable named varName is visible and has been defined, and "
    "0 otherwise. Handles qualified variable names containing ::.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info frame ?depth?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?depth?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "frame", subspec);
  e = feather_usage_help(ops, interp, e, "Get information about a call frame");
  e = feather_usage_long_help(ops, interp, e,
    "Returns the depth of the call to info frame itself. Otherwise, returns a "
    "dictionary describing the active command at the depth, which counts all "
    "commands visible to info level, plus commands that don't create a new level, "
    "such as eval or source.\n\n"
    "If depth is greater than 0 it is the frame at that depth. Otherwise it is "
    "the number of frames up from the current frame.\n\n"
    "The dictionary may contain the following keys:\n\n"
    "type     Always present. Possible values are source, proc, or eval.\n\n"
    "line     The line number of the command inside its script.\n\n"
    "file     For type source, provides the path of the file containing the command.\n\n"
    "cmd      The command before substitutions were performed.\n\n"
    "proc     For type proc, the name of the procedure containing the command.\n\n"
    "lambda   For apply commands, the definition of the lambda.\n\n"
    "level    The stack level.\n\n"
    "namespace  The namespace in which the command is executing.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info globals ?pattern?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?pattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "globals", subspec);
  e = feather_usage_help(ops, interp, e, "List global variables");
  e = feather_usage_long_help(ops, interp, e,
    "If pattern is not given, returns a list of all the names of currently-defined "
    "global variables. Global variables are variables in the global namespace. If "
    "pattern is given, only those names matching pattern are returned. Matching is "
    "determined using the same rules as for string match.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info level ?level?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?level?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "level", subspec);
  e = feather_usage_help(ops, interp, e, "Get current or specified stack level");
  e = feather_usage_long_help(ops, interp, e,
    "If level is not given, returns the level this routine was called from. "
    "Otherwise returns the complete command active at the given level as a list. "
    "If level is greater than 0, it is the desired level. Otherwise, it is level "
    "levels up from the current level. See uplevel for more information on levels.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info locals ?pattern?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?pattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "locals", subspec);
  e = feather_usage_help(ops, interp, e, "List local variables");
  e = feather_usage_long_help(ops, interp, e,
    "If pattern is given, returns the name of each local variable matching pattern "
    "according to string match. Otherwise, returns the name of each local variable. "
    "A variable defined with the global, upvar or variable command is not local.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info methods value (Feather extension)
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<value>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "methods", subspec);
  e = feather_usage_help(ops, interp, e, "List methods of a foreign object");
  e = feather_usage_long_help(ops, interp, e,
    "Feather extension: Returns a list of method names available on a foreign "
    "object. Returns an empty list for non-foreign objects.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info procs ?pattern?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?pattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "procs", subspec);
  e = feather_usage_help(ops, interp, e, "List defined procedures");
  e = feather_usage_long_help(ops, interp, e,
    "Returns the names of all visible procedures. If pattern is given, returns "
    "only those names that match according to string match. Only the final "
    "component in pattern is actually considered a pattern. Any qualifying "
    "components simply select a namespace. See NAMESPACE RESOLUTION in the "
    "namespace documentation.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info script
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_cmd(ops, interp, "script", subspec);
  e = feather_usage_help(ops, interp, e, "Get pathname of current script");
  e = feather_usage_long_help(ops, interp, e,
    "Returns the pathname of the innermost script currently being evaluated, or "
    "the empty string if no pathname can be determined.\n\n"
    "Note: Unlike TCL, Feather does not support setting the script path with "
    "info script filename.");
  spec = feather_usage_add(ops, interp, spec, e);

  // info type value (Feather extension)
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<value>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "type", subspec);
  e = feather_usage_help(ops, interp, e, "Get type of a value");
  e = feather_usage_long_help(ops, interp, e,
    "Feather extension: Returns the type of a value. For foreign objects returns "
    "the registered type name (e.g., \"Mux\", \"Connection\"). For collections "
    "returns \"list\" or \"dict\". For numbers returns \"int\" or \"double\". For "
    "everything else returns \"string\".");
  spec = feather_usage_add(ops, interp, spec, e);

  // info vars ?pattern?
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?pattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "vars", subspec);
  e = feather_usage_help(ops, interp, e, "List visible variables");
  e = feather_usage_long_help(ops, interp, e,
    "If pattern is not given, returns the names of all visible variables. If "
    "pattern is given, returns only those names that match according to string "
    "match. Only the last component of pattern is a pattern. Other components "
    "identify a namespace. See NAMESPACE RESOLUTION in the namespace documentation. "
    "When pattern is a qualified name, results are fully qualified.");
  spec = feather_usage_add(ops, interp, spec, e);

  // See Also section
  e = feather_usage_section(ops, interp, "See Also",
    "namespace, proc, global, upvar, variable, uplevel");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "info", spec);
}
