#include "tclc.h"

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

  // Try to invoke via bind.unknown (host command lookup)
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
        return result;
      }
    }
  }

  return (status == TCL_PARSE_DONE) ? result : TCL_ERROR;
}
