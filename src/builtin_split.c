#include "tclc.h"
#include "internal.h"

// Default split characters (whitespace)
static int is_default_split_char(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

TclResult tcl_builtin_split(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"split string ?splitChars?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  size_t strLen;
  const char *str = ops->string.get(interp, strObj, &strLen);

  const char *splitChars = NULL;
  size_t splitLen = 0;
  int emptyDelim = 0;

  if (argc == 2) {
    TclObj splitObj = ops->list.shift(interp, args);
    splitChars = ops->string.get(interp, splitObj, &splitLen);
    if (splitLen == 0) {
      emptyDelim = 1;  // Split into individual characters
    }
  }

  TclObj result = ops->list.create(interp);

  // Handle empty string
  if (strLen == 0) {
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Split into individual characters
  if (emptyDelim) {
    for (size_t i = 0; i < strLen; i++) {
      TclObj elem = ops->string.intern(interp, str + i, 1);
      result = ops->list.push(interp, result, elem);
    }
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Split by delimiter characters
  size_t start = 0;
  for (size_t i = 0; i <= strLen; i++) {
    int isDelim = 0;
    if (i < strLen) {
      if (splitChars) {
        // Check if current char is in split chars
        for (size_t j = 0; j < splitLen; j++) {
          if (str[i] == splitChars[j]) {
            isDelim = 1;
            break;
          }
        }
      } else {
        // Use default whitespace
        isDelim = is_default_split_char(str[i]);
      }
    }

    if (isDelim || i == strLen) {
      // End of segment
      TclObj elem = ops->string.intern(interp, str + start, i - start);
      result = ops->list.push(interp, result, elem);
      start = i + 1;
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
