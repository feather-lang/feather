#include "feather.h"
#include "internal.h"

// Default split characters (whitespace)
static int is_default_split_char(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Check if byte c is in splitObj using byte_at()
static int is_split_char(const FeatherHostOps *ops, FeatherInterp interp,
                         int c, FeatherObj splitObj, size_t splitLen) {
  for (size_t j = 0; j < splitLen; j++) {
    if (ops->string.byte_at(interp, splitObj, j) == c) {
      return 1;
    }
  }
  return 0;
}

FeatherResult feather_builtin_split(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"split string ?splitChars?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  size_t strLen = ops->string.byte_length(interp, strObj);

  FeatherObj splitObj = 0;
  size_t splitLen = 0;
  int emptyDelim = 0;
  int hasCustomSplit = 0;

  if (argc == 2) {
    splitObj = ops->list.shift(interp, args);
    splitLen = ops->string.byte_length(interp, splitObj);
    if (splitLen == 0) {
      emptyDelim = 1;  // Split into individual characters
    }
    hasCustomSplit = 1;
  }

  FeatherObj result = ops->list.create(interp);

  // Handle empty string
  if (strLen == 0) {
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Split into individual characters
  if (emptyDelim) {
    for (size_t i = 0; i < strLen; i++) {
      FeatherObj elem = ops->string.slice(interp, strObj, i, i + 1);
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
      int c = ops->string.byte_at(interp, strObj, i);
      if (hasCustomSplit) {
        // Check if current char is in split chars
        isDelim = is_split_char(ops, interp, c, splitObj, splitLen);
      } else {
        // Use default whitespace
        isDelim = is_default_split_char(c);
      }
    }

    if (isDelim || i == strLen) {
      // End of segment
      FeatherObj elem = ops->string.slice(interp, strObj, start, i);
      result = ops->list.push(interp, result, elem);
      start = i + 1;
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
