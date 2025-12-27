#include "feather.h"
#include "internal.h"
#include "index_parse.h"

// Default whitespace characters for trim
static int string_is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// Check if char is in charset
static int in_charset(char c, const char *chars, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (chars[i] == c) return 1;
  }
  return 0;
}

// string length
static FeatherResult string_length(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string length string\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  size_t charLen = ops->rune.length(interp, strObj);

  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)charLen));
  return TCL_OK;
}

// string index
static FeatherResult string_index(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string index string charIndex\"", 55);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  FeatherObj indexObj = ops->list.shift(interp, args);

  size_t charLen = ops->rune.length(interp, strObj);

  int64_t index;
  if (feather_parse_index(ops, interp, indexObj, charLen, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  if (index < 0 || (size_t)index >= charLen) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  FeatherObj result = ops->rune.at(interp, strObj, (size_t)index);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string range
static FeatherResult string_range(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string range string first last\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  FeatherObj firstObj = ops->list.shift(interp, args);
  FeatherObj lastObj = ops->list.shift(interp, args);

  size_t charLen = ops->rune.length(interp, strObj);

  int64_t first, last;
  if (feather_parse_index(ops, interp, firstObj, charLen, &first) != TCL_OK) {
    return TCL_ERROR;
  }
  if (feather_parse_index(ops, interp, lastObj, charLen, &last) != TCL_OK) {
    return TCL_ERROR;
  }

  // ops->rune.range handles clamping and empty string cases
  FeatherObj result = ops->rune.range(interp, strObj, first, last);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string match
static FeatherResult string_match(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  int nocase = 0;

  // Check for -nocase option
  if (argc >= 1) {
    FeatherObj first = ops->list.at(interp, args, 0);
    if (feather_obj_eq_literal(ops, interp, first, "-nocase")) {
      nocase = 1;
      ops->list.shift(interp, args);
      argc--;
    }
  }

  if (argc != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string match ?-nocase? pattern string\"", 63);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj pattern = ops->list.shift(interp, args);
  FeatherObj string = ops->list.shift(interp, args);

  int matches;
  if (nocase) {
    // Use case-folded versions for comparison
    FeatherObj foldedPattern = ops->rune.fold(interp, pattern);
    FeatherObj foldedString = ops->rune.fold(interp, string);
    size_t plen, slen;
    const char *pstr = ops->string.get(interp, foldedPattern, &plen);
    const char *sstr = ops->string.get(interp, foldedString, &slen);
    matches = feather_glob_match(pstr, plen, sstr, slen);
  } else {
    size_t plen, slen;
    const char *pstr = ops->string.get(interp, pattern, &plen);
    const char *sstr = ops->string.get(interp, string, &slen);
    matches = feather_glob_match(pstr, plen, sstr, slen);
  }

  ops->interp.set_result(interp, ops->integer.create(interp, matches ? 1 : 0));
  return TCL_OK;
}

// string toupper
static FeatherResult string_toupper(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string toupper string ?first? ?last?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  // For now, convert entire string (ignore first/last)
  FeatherObj result = ops->rune.to_upper(interp, strObj);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string tolower
static FeatherResult string_tolower(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string tolower string ?first? ?last?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  FeatherObj result = ops->rune.to_lower(interp, strObj);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string trim
static FeatherResult string_trim(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string trim string ?chars?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  size_t len;
  const char *str = ops->string.get(interp, strObj, &len);

  const char *chars = NULL;
  size_t charsLen = 0;
  if (argc == 2) {
    FeatherObj charsObj = ops->list.shift(interp, args);
    chars = ops->string.get(interp, charsObj, &charsLen);
  }

  // Find start (skip leading trim chars)
  size_t start = 0;
  while (start < len) {
    int shouldTrim = chars ? in_charset(str[start], chars, charsLen) : string_is_whitespace(str[start]);
    if (!shouldTrim) break;
    start++;
  }

  // Find end (skip trailing trim chars)
  size_t end = len;
  while (end > start) {
    int shouldTrim = chars ? in_charset(str[end - 1], chars, charsLen) : string_is_whitespace(str[end - 1]);
    if (!shouldTrim) break;
    end--;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, str + start, end - start));
  return TCL_OK;
}

// string trimleft
static FeatherResult string_trimleft(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string trimleft string ?chars?\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  size_t len;
  const char *str = ops->string.get(interp, strObj, &len);

  const char *chars = NULL;
  size_t charsLen = 0;
  if (argc == 2) {
    FeatherObj charsObj = ops->list.shift(interp, args);
    chars = ops->string.get(interp, charsObj, &charsLen);
  }

  size_t start = 0;
  while (start < len) {
    int shouldTrim = chars ? in_charset(str[start], chars, charsLen) : string_is_whitespace(str[start]);
    if (!shouldTrim) break;
    start++;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, str + start, len - start));
  return TCL_OK;
}

// string trimright
static FeatherResult string_trimright(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string trimright string ?chars?\"", 57);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  size_t len;
  const char *str = ops->string.get(interp, strObj, &len);

  const char *chars = NULL;
  size_t charsLen = 0;
  if (argc == 2) {
    FeatherObj charsObj = ops->list.shift(interp, args);
    chars = ops->string.get(interp, charsObj, &charsLen);
  }

  size_t end = len;
  while (end > 0) {
    int shouldTrim = chars ? in_charset(str[end - 1], chars, charsLen) : string_is_whitespace(str[end - 1]);
    if (!shouldTrim) break;
    end--;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, str, end));
  return TCL_OK;
}

// string map - uses byte-level comparison with ASCII-only case folding for -nocase
// TODO: For full Unicode support, this would need to use rune operations
static FeatherResult string_map(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  int nocase = 0;
  if (argc >= 1) {
    FeatherObj first = ops->list.at(interp, args, 0);
    if (feather_obj_eq_literal(ops, interp, first, "-nocase")) {
      nocase = 1;
      ops->list.shift(interp, args);
      argc--;
    }
  }

  if (argc != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string map ?-nocase? mapping string\"", 61);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj mappingObj = ops->list.shift(interp, args);
  FeatherObj strObj = ops->list.shift(interp, args);

  // For nocase, we compare folded versions
  FeatherObj foldedStr = nocase ? ops->rune.fold(interp, strObj) : strObj;

  // Parse mapping as list of key/value pairs
  FeatherObj mapping = ops->list.from(interp, mappingObj);
  size_t mappingLen = ops->list.length(interp, mapping);
  if (mappingLen % 2 != 0) {
    FeatherObj msg = ops->string.intern(interp, "char map list unbalanced", 24);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  size_t strLen;
  const char *str = ops->string.get(interp, strObj, &strLen);
  size_t foldedLen;
  const char *folded = ops->string.get(interp, foldedStr, &foldedLen);

  // Build result - note: for nocase with case-folding that changes length,
  // we use the folded string for matching but original for non-matched chars
  FeatherObj result = ops->string.intern(interp, "", 0);
  size_t i = 0;

  while (i < strLen) {
    int matched = 0;

    // Try each mapping
    for (size_t m = 0; m < mappingLen; m += 2) {
      FeatherObj keyObj = ops->list.at(interp, mapping, m);
      FeatherObj valObj = ops->list.at(interp, mapping, m + 1);

      // For nocase, fold the key
      FeatherObj keyToMatch = nocase ? ops->rune.fold(interp, keyObj) : keyObj;

      size_t keyLen;
      const char *key = ops->string.get(interp, keyToMatch, &keyLen);
      size_t valLen;
      const char *val = ops->string.get(interp, valObj, &valLen);

      if (keyLen == 0) continue;
      if (i + keyLen > foldedLen) continue;

      // Check if key matches at position i in folded string
      int match = 1;
      for (size_t k = 0; k < keyLen; k++) {
        if (folded[i + k] != key[k]) {
          match = 0;
          break;
        }
      }

      if (match) {
        // Append replacement value
        FeatherObj valStr = ops->string.intern(interp, val, valLen);
        result = ops->string.concat(interp, result, valStr);
        i += keyLen;
        matched = 1;
        break;
      }
    }

    if (!matched) {
      // Append original character (from folded, which may differ from original)
      FeatherObj ch = ops->string.intern(interp, &folded[i], 1);
      result = ops->string.concat(interp, result, ch);
      i++;
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

FeatherResult feather_builtin_string(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string subcommand ?arg ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj subcmd = ops->list.shift(interp, args);

  if (feather_obj_eq_literal(ops, interp, subcmd, "length")) {
    return string_length(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "index")) {
    return string_index(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "range")) {
    return string_range(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "match")) {
    return string_match(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "toupper")) {
    return string_toupper(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "tolower")) {
    return string_tolower(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "trim")) {
    return string_trim(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "trimleft")) {
    return string_trimleft(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "trimright")) {
    return string_trimright(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "map")) {
    return string_map(ops, interp, args);
  } else {
    FeatherObj msg = ops->string.intern(interp, "unknown or ambiguous subcommand \"", 33);
    msg = ops->string.concat(interp, msg, subcmd);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be index, length, map, match, range, tolower, toupper, trim, trimleft, or trimright", 91);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
