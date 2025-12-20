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
  TclParseStatus status = tcl_parse(ops, interp, script, len);
  if (status != TCL_PARSE_OK) {
    return TCL_ERROR;
  }

  // The parser stores its result in the interpreter's result slot
  TclObj parsed = ops->interp.get_result(interp);
  return tcl_eval_obj(ops, interp, parsed, flags);
}
