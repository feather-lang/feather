#include "tclc.h"
#include "internal.h"

// Helper to check if char is whitespace
static int is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

TclResult tcl_builtin_concat(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  // Empty args returns empty string
  if (argc == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // concat joins arguments with spaces, trimming leading/trailing whitespace
  // from each argument
  TclObj result = 0;

  for (size_t i = 0; i < argc; i++) {
    TclObj arg = ops->list.shift(interp, args);
    size_t len;
    const char *str = ops->string.get(interp, arg, &len);

    // Trim leading whitespace
    size_t start = 0;
    while (start < len && is_whitespace(str[start])) start++;

    // Trim trailing whitespace
    size_t end = len;
    while (end > start && is_whitespace(str[end - 1])) end--;

    // Skip empty segments
    if (start >= end) continue;

    TclObj trimmed = ops->string.intern(interp, str + start, end - start);

    if (ops->list.is_nil(interp, result) || result == 0) {
      result = trimmed;
    } else {
      TclObj space = ops->string.intern(interp, " ", 1);
      result = ops->string.concat(interp, result, space);
      result = ops->string.concat(interp, result, trimmed);
    }
  }

  if (ops->list.is_nil(interp, result) || result == 0) {
    result = ops->string.intern(interp, "", 0);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
