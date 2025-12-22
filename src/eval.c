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

  // Check for builtin commands first
  size_t cmdLen;
  const char *cmdStr = ops->string.get(interp, cmd, &cmdLen);
  TclBuiltinCmd builtin = tcl_lookup_builtin(cmdStr, cmdLen);
  if (builtin != NULL) {
    return builtin(ops, interp, cmd, args);
  }

  // Check for user-defined procedures
  if (ops->proc.exists(interp, cmd)) {
    return tcl_invoke_proc(ops, interp, cmd, args);
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
