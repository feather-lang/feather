#include "tclc.h"

TclParseStatus tcl_parse(const TclHostOps *ops, TclInterp interp,
                         const char *script, size_t len) {
  // Skip leading whitespace
  while (len > 0 &&
         (*script == ' ' || *script == '\t' || *script == '\n' ||
          *script == '\r')) {
    script++;
    len--;
  }

  if (len == 0) {
    // Empty script after stripping whitespace
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_PARSE_OK;
  }

  // For now, just intern the remaining script as a single command
  // TODO: proper tokenization
  TclObj cmd = ops->string.intern(interp, script, len);
  ops->interp.set_result(interp, cmd);

  return TCL_PARSE_OK;
}
