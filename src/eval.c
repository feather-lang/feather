#include "feather.h"
#include "host.h"
#include "internal.h"

FeatherResult feather_command_exec(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj command, FeatherEvalFlags flags) {
  ops = feather_get_ops(ops);
  // command is a parsed command list [name, arg1, arg2, ...]
  // First element is the command name, rest are arguments (unevaluated)

  // Check if the list is empty
  if (ops->list.length(interp, command) == 0) {
    return TCL_OK;
  }

  // Save the original command list for execution traces
  // Use list.from to create a copy (it creates a new list from an existing one)
  FeatherObj originalCmd = ops->list.from(interp, command);

  // Extract the command name (first element)
  FeatherObj cmd = ops->list.shift(interp, command);
  if (ops->list.is_nil(interp, cmd)) {
    return TCL_OK;
  }

  // The remaining list is the arguments
  FeatherObj args = command;

  // Resolve command name using namespace-scoped lookup
  // TCL command resolution order:
  // 1. For qualified names (contain ::):
  //    - Split into namespace and simple name, look up in that namespace
  // 2. For unqualified names:
  //    a. Try current namespace first
  //    b. Fall back to global namespace
  //
  FeatherBuiltinCmd builtin = NULL;
  FeatherCommandType cmdType = TCL_CMD_NONE;
  FeatherObj lookupNs = 0;
  FeatherObj simpleName = cmd;

  FeatherObj currentNs = ops->ns.current(interp);
  int inGlobalNs = feather_obj_is_global_ns(ops, interp, currentNs);
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);

  if (feather_obj_is_qualified(ops, interp, cmd)) {
    // Qualified name - split and look up in the target namespace
    feather_obj_split_command(ops, interp, cmd, &lookupNs, &simpleName);
    if (ops->list.is_nil(interp, lookupNs)) {
      lookupNs = globalNs;
    }
    cmdType = ops->ns.get_command(interp, lookupNs, simpleName, &builtin, NULL, NULL);
  } else {
    // Unqualified name - try current namespace first, then global
    if (!inGlobalNs) {
      cmdType = ops->ns.get_command(interp, currentNs, cmd, &builtin, NULL, NULL);
      if (cmdType != TCL_CMD_NONE) {
        lookupNs = currentNs;
        simpleName = cmd;
      }
    }

    // If not found in current namespace, try global namespace
    if (cmdType == TCL_CMD_NONE) {
      cmdType = ops->ns.get_command(interp, globalNs, cmd, &builtin, NULL, NULL);
      if (cmdType != TCL_CMD_NONE) {
        lookupNs = globalNs;
        simpleName = cmd;
      }
    }
  }

  // Build fully qualified name for proc invocation and error messages
  FeatherObj lookupName;
  if (ops->list.is_nil(interp, lookupNs) || lookupNs == 0) {
    lookupName = cmd;
  } else {
    if (feather_obj_is_global_ns(ops, interp, lookupNs)) {
      // Global namespace: "::simpleName"
      lookupName = ops->string.concat(interp, globalNs, simpleName);
    } else {
      // Other namespace: "::ns::simpleName"
      lookupName = ops->string.concat(interp, lookupNs,
                                      ops->string.intern(interp, "::", 2));
      lookupName = ops->string.concat(interp, lookupName, simpleName);
    }
  }

  // Fire "enter" execution traces before command executes
  FeatherResult enterResult = feather_fire_exec_traces(ops, interp, lookupName, originalCmd, "enter", 0, 0);
  if (enterResult != TCL_OK) {
    return enterResult;  // Enter trace error propagates directly
  }

  FeatherResult code;
  FeatherResult leaveResult;
  switch (cmdType) {
  case TCL_CMD_BUILTIN:
    if (builtin != NULL) {
      // Call the builtin function directly
      code = builtin(ops, interp, lookupName, args);
      // Fire "leave" execution traces after command completes
      leaveResult = feather_fire_exec_traces(ops, interp, lookupName, originalCmd, "leave", code, ops->interp.get_result(interp));
      return (leaveResult != TCL_OK) ? leaveResult : code;
    }
    // NULL builtin means host-registered command - fall through to unknown
    break;
  case TCL_CMD_PROC:
    // For procs, use the fully qualified name for lookup
    code = feather_invoke_proc(ops, interp, lookupName, args);
    // Fire "leave" execution traces after command completes
    leaveResult = feather_fire_exec_traces(ops, interp, lookupName, originalCmd, "leave", code, ops->interp.get_result(interp));
    return (leaveResult != TCL_OK) ? leaveResult : code;
  case TCL_CMD_NONE:
    // Fall through to unknown handling
    break;
  }

  // Check for user-defined 'unknown' procedure in global namespace
  FeatherObj unknownSimple = ops->string.intern(interp, "unknown", 7);
  FeatherBuiltinCmd unusedFn = NULL;
  FeatherCommandType unknownType = ops->ns.get_command(interp, globalNs, unknownSimple, &unusedFn, NULL, NULL);

  if (unknownType == TCL_CMD_PROC) {
    // Build args list: [originalCmd, arg1, arg2, ...]
    FeatherObj unknownArgs = ops->list.create(interp);
    unknownArgs = ops->list.push(interp, unknownArgs, cmd);
    size_t argc = ops->list.length(interp, args);
    for (size_t i = 0; i < argc; i++) {
      FeatherObj arg = ops->list.at(interp, args, i);
      unknownArgs = ops->list.push(interp, unknownArgs, arg);
    }
    FeatherObj unknownName = ops->string.intern(interp, "::unknown", 9);
    code = feather_invoke_proc(ops, interp, unknownName, unknownArgs);
    // Fire "leave" execution traces after command completes
    leaveResult = feather_fire_exec_traces(ops, interp, lookupName, originalCmd, "leave", code, ops->interp.get_result(interp));
    return (leaveResult != TCL_OK) ? leaveResult : code;
  }

  // Fall back to host command lookup via bind.unknown
  FeatherObj result;
  code = ops->bind.unknown(interp, cmd, args, &result);

  if (code == TCL_OK) {
    ops->interp.set_result(interp, result);
  }

  // Fire "leave" execution traces after command completes
  leaveResult = feather_fire_exec_traces(ops, interp, lookupName, originalCmd, "leave", code, ops->interp.get_result(interp));

  return (leaveResult != TCL_OK) ? leaveResult : code;
}

FeatherResult feather_script_eval(const FeatherHostOps *ops, FeatherInterp interp,
                          const char *source, size_t len, FeatherEvalFlags flags) {
  ops = feather_get_ops(ops);
  FeatherResult result = TCL_OK;
  FeatherParseContext ctx;
  feather_parse_init(&ctx, source, len);

  FeatherParseStatus status;
  while ((status = feather_parse_command(ops, interp, &ctx)) == TCL_PARSE_OK) {
    FeatherObj parsed = ops->interp.get_result(interp);

    // Only execute non-empty commands
    if (ops->list.length(interp, parsed) > 0) {
      result = feather_command_exec(ops, interp, parsed, flags);
      if (result != TCL_OK) {
        // Let break/continue propagate - the while loop will catch them
        // If they reach the top level, the host converts to error
        return result;
      }
    }
  }

  return (status == TCL_PARSE_DONE) ? result : TCL_ERROR;
}

FeatherResult feather_script_eval_obj(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj script, FeatherEvalFlags flags) {
  ops = feather_get_ops(ops);
  size_t len = ops->string.byte_length(interp, script);
  FeatherResult result = TCL_OK;
  FeatherParseContextObj ctx;
  feather_parse_init_obj(&ctx, script, len);

  FeatherParseStatus status;
  while ((status = feather_parse_command_obj(ops, interp, &ctx)) == TCL_PARSE_OK) {
    FeatherObj parsed = ops->interp.get_result(interp);

    // Only execute non-empty commands
    if (ops->list.length(interp, parsed) > 0) {
      // Note: The line is set by feather_parse_command_obj before parsing,
      // so command substitutions during parsing don't overwrite it.
      // The parser also handles the check to prevent nested evals from
      // overwriting the outer command's line.

      result = feather_command_exec(ops, interp, parsed, flags);
      if (result != TCL_OK) {
        return result;
      }
    }
  }

  return (status == TCL_PARSE_DONE) ? result : TCL_ERROR;
}
