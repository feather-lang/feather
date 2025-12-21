#include "tclc.h"
#include "internal.h"

TclResult tcl_eval_obj(const TclHostOps *ops, TclInterp interp, TclObj script,
                       TclEvalFlags flags) {
  // script is a list of tokens from the parser
  // First token is the command name, rest are arguments

  // Check if the list is empty
  if (ops->list.length(interp, script) == 0) {
    return TCL_OK;
  }

  // Extract the command name (first element)
  TclObj cmd = ops->list.shift(interp, script);
  if (ops->list.is_nil(interp, cmd)) {
    return TCL_OK;
  }

  // The remaining list is the arguments
  TclObj args = script;

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

TclResult tcl_eval_string(const TclHostOps *ops, TclInterp interp,
                          const char *script, size_t len, TclEvalFlags flags) {
  TclResult result = TCL_OK;
  TclParseContext ctx;
  tcl_parse_init(&ctx, script, len);

  TclParseStatus status;
  while ((status = tcl_parse_command(ops, interp, &ctx)) == TCL_PARSE_OK) {
    TclObj parsed = ops->interp.get_result(interp);

    // Only evaluate non-empty commands
    if (ops->list.length(interp, parsed) > 0) {
      result = tcl_eval_obj(ops, interp, parsed, flags);
      if (result != TCL_OK) {
        // Let break/continue propagate - the while loop will catch them
        // If they reach the top level, the host converts to error
        return result;
      }
    }
  }

  return (status == TCL_PARSE_DONE) ? result : TCL_ERROR;
}
