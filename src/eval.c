#include "tclc.h"
#include "internal.h"

TclResult tcl_command_exec(const TclHostOps *ops, TclInterp interp,
                           TclObj command, TclEvalFlags flags) {
  // command is a parsed command list [name, arg1, arg2, ...]
  // First element is the command name, rest are arguments (unevaluated)

  // Check if the list is empty
  if (ops->list.length(interp, command) == 0) {
    return TCL_OK;
  }

  // Extract the command name (first element)
  TclObj cmd = ops->list.shift(interp, command);
  if (ops->list.is_nil(interp, cmd)) {
    return TCL_OK;
  }

  // The remaining list is the arguments
  TclObj args = command;

  // Resolve command name to fully qualified form for lookup
  size_t cmdLen;
  const char *cmdStr = ops->string.get(interp, cmd, &cmdLen);

  TclObj lookupName = cmd;
  TclBuiltinCmd builtin = NULL;
  TclCommandType cmdType;

  // First try the command name as-is (handles builtins like tcl::mathfunc::exp)
  cmdType = ops->proc.lookup(interp, cmd, &builtin);

  // If not found and it's a qualified name without leading ::, try with ::
  if (cmdType == TCL_CMD_NONE && tcl_is_qualified(cmdStr, cmdLen)) {
    if (cmdLen < 2 || cmdStr[0] != ':' || cmdStr[1] != ':') {
      // Relative qualified name like "foo::bar" - try "::foo::bar"
      lookupName = ops->string.intern(interp, "::", 2);
      lookupName = ops->string.concat(interp, lookupName, cmd);
      cmdType = ops->proc.lookup(interp, lookupName, &builtin);
    }
  }

  // If not found and it's an unqualified name, try with current namespace prefix
  if (cmdType == TCL_CMD_NONE && !tcl_is_qualified(cmdStr, cmdLen)) {
    TclObj currentNs = ops->ns.current(interp);
    size_t nsLen;
    const char *nsStr = ops->string.get(interp, currentNs, &nsLen);

    // Only try namespace-qualified lookup if not in global namespace
    if (nsLen > 2 || (nsLen == 2 && (nsStr[0] != ':' || nsStr[1] != ':'))) {
      // Try current namespace: "::ns::cmd"
      TclObj nsQualified = ops->string.concat(interp, currentNs,
                                              ops->string.intern(interp, "::", 2));
      nsQualified = ops->string.concat(interp, nsQualified, cmd);
      cmdType = ops->proc.lookup(interp, nsQualified, &builtin);
      if (cmdType != TCL_CMD_NONE) {
        lookupName = nsQualified;
      }
    }

    // If still not found, try global namespace: "::cmd"
    if (cmdType == TCL_CMD_NONE) {
      TclObj globalQualified = ops->string.intern(interp, "::", 2);
      globalQualified = ops->string.concat(interp, globalQualified, cmd);
      cmdType = ops->proc.lookup(interp, globalQualified, &builtin);
      if (cmdType != TCL_CMD_NONE) {
        lookupName = globalQualified;
      }
    }
  }

  switch (cmdType) {
  case TCL_CMD_BUILTIN:
    if (builtin != NULL) {
      // Call the builtin function directly
      return builtin(ops, interp, lookupName, args);
    }
    // NULL builtin means host-registered command - fall through to unknown
    break;
  case TCL_CMD_PROC:
    // For procs, use the fully qualified name for lookup
    return tcl_invoke_proc(ops, interp, lookupName, args);
  case TCL_CMD_NONE:
    // Fall through to unknown handling
    break;
  }

  // Check for user-defined 'unknown' procedure
  // Try both "unknown" and "::unknown" since procs may be stored either way
  TclObj unknownName = ops->string.intern(interp, "unknown", 7);
  TclBuiltinCmd unusedFn = NULL;
  TclCommandType unknownType = ops->proc.lookup(interp, unknownName, &unusedFn);

  // If not found as "unknown", try "::unknown"
  if (unknownType == TCL_CMD_NONE) {
    unknownName = ops->string.intern(interp, "::unknown", 9);
    unknownType = ops->proc.lookup(interp, unknownName, &unusedFn);
  }

  if (unknownType == TCL_CMD_PROC) {
    // Build args list: [originalCmd, arg1, arg2, ...]
    TclObj unknownArgs = ops->list.create(interp);
    unknownArgs = ops->list.push(interp, unknownArgs, cmd);
    size_t argc = ops->list.length(interp, args);
    for (size_t i = 0; i < argc; i++) {
      TclObj arg = ops->list.at(interp, args, i);
      unknownArgs = ops->list.push(interp, unknownArgs, arg);
    }
    return tcl_invoke_proc(ops, interp, unknownName, unknownArgs);
  }

  // Fall back to host command lookup via bind.unknown
  TclObj result;
  TclResult code = ops->bind.unknown(interp, cmd, args, &result);

  if (code == TCL_OK) {
    ops->interp.set_result(interp, result);
  }

  return code;
}

TclResult tcl_script_eval(const TclHostOps *ops, TclInterp interp,
                          const char *source, size_t len, TclEvalFlags flags) {
  TclResult result = TCL_OK;
  TclParseContext ctx;
  tcl_parse_init(&ctx, source, len);

  TclParseStatus status;
  while ((status = tcl_parse_command(ops, interp, &ctx)) == TCL_PARSE_OK) {
    TclObj parsed = ops->interp.get_result(interp);

    // Only execute non-empty commands
    if (ops->list.length(interp, parsed) > 0) {
      result = tcl_command_exec(ops, interp, parsed, flags);
      if (result != TCL_OK) {
        // Let break/continue propagate - the while loop will catch them
        // If they reach the top level, the host converts to error
        return result;
      }
    }
  }

  return (status == TCL_PARSE_DONE) ? result : TCL_ERROR;
}

TclResult tcl_script_eval_obj(const TclHostOps *ops, TclInterp interp,
                              TclObj script, TclEvalFlags flags) {
  size_t len;
  const char *source = ops->string.get(interp, script, &len);
  return tcl_script_eval(ops, interp, source, len, flags);
}
