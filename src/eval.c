#include "tclc.h"

TclResult tcl_eval_obj(const TclHostOps *ops, TclInterp interp, TclObj script,
                       TclEvalFlags flags) {
  size_t len;
  const char *str = ops->string.get(interp, script, &len);

  if (len == 0) {
    return TCL_OK;
  }

  // For the stub, we just call bind.unknown with the entire script as command
  // This is sufficient for simple command invocation without arguments
  TclObj result;
  TclResult code = ops->bind.unknown(interp, script, 0, &result);

  if (code == TCL_OK) {
    ops->interp.set_result(interp, result);
  }

  return code;
}

TclResult tcl_eval_string(const TclHostOps *ops, TclInterp interp,
                          const char *script, size_t len, TclEvalFlags flags) {
  TclResult result = TCL_OK;
  const char *start = script;
  const char *end = script + len;

  while (start < end) {
    // Find the end of the current command (newline or end of input)
    const char *cmd_end = start;
    while (cmd_end < end && *cmd_end != '\n') {
      cmd_end++;
    }

    size_t cmd_len = cmd_end - start;

    // Parse and evaluate this command
    TclParseStatus status = tcl_parse(ops, interp, start, cmd_len);
    if (status != TCL_PARSE_OK) {
      return TCL_ERROR;
    }

    // The parser stores its result in the interpreter's result slot
    TclObj parsed = ops->interp.get_result(interp);

    // Only evaluate non-empty commands
    size_t parsed_len;
    ops->string.get(interp, parsed, &parsed_len);
    if (parsed_len > 0) {
      result = tcl_eval_obj(ops, interp, parsed, flags);
      if (result != TCL_OK) {
        return result;
      }
    }

    // Move past the newline
    start = cmd_end;
    if (start < end && *start == '\n') {
      start++;
    }
  }

  return result;
}
