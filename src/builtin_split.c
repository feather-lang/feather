#include "feather.h"
#include "internal.h"

// Default split characters (whitespace) - byte-based for ASCII
static int is_default_split_char(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Check if rune (as string obj) is in splitObj by comparing each rune
static int is_split_rune(const FeatherHostOps *ops, FeatherInterp interp,
                         FeatherObj runeObj, FeatherObj splitObj, size_t splitLen) {
  for (size_t j = 0; j < splitLen; j++) {
    FeatherObj delimRune = ops->rune.at(interp, splitObj, j);
    if (ops->string.equal(interp, runeObj, delimRune)) {
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
  size_t strLen = ops->rune.length(interp, strObj);  // Unicode character count

  FeatherObj splitObj = 0;
  size_t splitLen = 0;
  int emptyDelim = 0;
  int hasCustomSplit = 0;

  if (argc == 2) {
    splitObj = ops->list.shift(interp, args);
    splitLen = ops->rune.length(interp, splitObj);  // Unicode character count
    if (splitLen == 0) {
      emptyDelim = 1;  // Split into individual Unicode characters
    }
    hasCustomSplit = 1;
  }

  FeatherObj result = ops->list.create(interp);

  // Handle empty string
  if (strLen == 0) {
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Split into individual Unicode characters
  if (emptyDelim) {
    for (size_t i = 0; i < strLen; i++) {
      FeatherObj elem = ops->rune.at(interp, strObj, i);
      result = ops->list.push(interp, result, elem);
    }
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Split by delimiter characters (Unicode-aware)
  size_t start = 0;
  for (size_t i = 0; i <= strLen; i++) {
    int isDelim = 0;
    if (i < strLen) {
      FeatherObj currentRune = ops->rune.at(interp, strObj, i);
      if (hasCustomSplit) {
        // Check if current rune is in split runes
        isDelim = is_split_rune(ops, interp, currentRune, splitObj, splitLen);
      } else {
        // Use default whitespace (byte-based check for ASCII whitespace)
        // Since all default split chars are ASCII, we can check the first byte
        int c = ops->string.byte_at(interp, currentRune, 0);
        isDelim = (c != -1) && is_default_split_char(c);
      }
    }

    if (isDelim || i == strLen) {
      // End of segment - extract using rune.range
      // Cast to int64_t before subtraction to avoid unsigned underflow on 32-bit
      FeatherObj elem = ops->rune.range(interp, strObj, (int64_t)start, (int64_t)i - 1);
      result = ops->list.push(interp, result, elem);
      start = i + 1;
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
