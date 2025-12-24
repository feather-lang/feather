#include "tclc.h"
#include "internal.h"

// Helper to check string equality
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t llen = 0;
  while (lit[llen]) llen++;
  if (len != llen) return 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) return 0;
  }
  return 1;
}

// Parse an index like "end", "end-N", or integer
static TclResult parse_index(const TclHostOps *ops, TclInterp interp,
                             TclObj indexObj, size_t strLen, int64_t *out) {
  size_t len;
  const char *str = ops->string.get(interp, indexObj, &len);

  if (len == 3 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd') {
    *out = (int64_t)strLen - 1;
    return TCL_OK;
  }

  if (len > 4 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd' && str[3] == '-') {
    int64_t offset = 0;
    for (size_t i = 4; i < len; i++) {
      if (str[i] < '0' || str[i] > '9') {
        TclObj msg = ops->string.intern(interp, "bad index \"", 11);
        msg = ops->string.concat(interp, msg, indexObj);
        TclObj suffix = ops->string.intern(interp, "\"", 1);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      offset = offset * 10 + (str[i] - '0');
    }
    *out = (int64_t)strLen - 1 - offset;
    return TCL_OK;
  }

  if (ops->integer.get(interp, indexObj, out) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "bad index \"", 11);
    msg = ops->string.concat(interp, msg, indexObj);
    TclObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  return TCL_OK;
}

// Default whitespace characters for trim
static int is_whitespace(char c) {
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
static TclResult string_length(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string length string\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  size_t charLen = ops->rune.length(interp, strObj);

  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)charLen));
  return TCL_OK;
}

// string index
static TclResult string_index(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string index string charIndex\"", 55);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  TclObj indexObj = ops->list.shift(interp, args);

  size_t charLen = ops->rune.length(interp, strObj);

  int64_t index;
  if (parse_index(ops, interp, indexObj, charLen, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  if (index < 0 || (size_t)index >= charLen) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  TclObj result = ops->rune.at(interp, strObj, (size_t)index);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string range
static TclResult string_range(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string range string first last\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  TclObj firstObj = ops->list.shift(interp, args);
  TclObj lastObj = ops->list.shift(interp, args);

  size_t charLen = ops->rune.length(interp, strObj);

  int64_t first, last;
  if (parse_index(ops, interp, firstObj, charLen, &first) != TCL_OK) {
    return TCL_ERROR;
  }
  if (parse_index(ops, interp, lastObj, charLen, &last) != TCL_OK) {
    return TCL_ERROR;
  }

  // ops->rune.range handles clamping and empty string cases
  TclObj result = ops->rune.range(interp, strObj, first, last);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string match
static TclResult string_match(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  int nocase = 0;

  // Check for -nocase option
  if (argc >= 1) {
    TclObj first = ops->list.at(interp, args, 0);
    size_t len;
    const char *str = ops->string.get(interp, first, &len);
    if (str_eq(str, len, "-nocase")) {
      nocase = 1;
      ops->list.shift(interp, args);
      argc--;
    }
  }

  if (argc != 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string match ?-nocase? pattern string\"", 63);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj pattern = ops->list.shift(interp, args);
  TclObj string = ops->list.shift(interp, args);

  int matches;
  if (nocase) {
    // Use case-folded versions for comparison
    TclObj foldedPattern = ops->rune.fold(interp, pattern);
    TclObj foldedString = ops->rune.fold(interp, string);
    size_t plen, slen;
    const char *pstr = ops->string.get(interp, foldedPattern, &plen);
    const char *sstr = ops->string.get(interp, foldedString, &slen);
    matches = tcl_glob_match(pstr, plen, sstr, slen);
  } else {
    size_t plen, slen;
    const char *pstr = ops->string.get(interp, pattern, &plen);
    const char *sstr = ops->string.get(interp, string, &slen);
    matches = tcl_glob_match(pstr, plen, sstr, slen);
  }

  ops->interp.set_result(interp, ops->integer.create(interp, matches ? 1 : 0));
  return TCL_OK;
}

// string toupper
static TclResult string_toupper(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string toupper string ?first? ?last?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  // For now, convert entire string (ignore first/last)
  TclObj result = ops->rune.to_upper(interp, strObj);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string tolower
static TclResult string_tolower(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string tolower string ?first? ?last?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  TclObj result = ops->rune.to_lower(interp, strObj);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string trim
static TclResult string_trim(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string trim string ?chars?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  size_t len;
  const char *str = ops->string.get(interp, strObj, &len);

  const char *chars = NULL;
  size_t charsLen = 0;
  if (argc == 2) {
    TclObj charsObj = ops->list.shift(interp, args);
    chars = ops->string.get(interp, charsObj, &charsLen);
  }

  // Find start (skip leading trim chars)
  size_t start = 0;
  while (start < len) {
    int shouldTrim = chars ? in_charset(str[start], chars, charsLen) : is_whitespace(str[start]);
    if (!shouldTrim) break;
    start++;
  }

  // Find end (skip trailing trim chars)
  size_t end = len;
  while (end > start) {
    int shouldTrim = chars ? in_charset(str[end - 1], chars, charsLen) : is_whitespace(str[end - 1]);
    if (!shouldTrim) break;
    end--;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, str + start, end - start));
  return TCL_OK;
}

// string trimleft
static TclResult string_trimleft(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string trimleft string ?chars?\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  size_t len;
  const char *str = ops->string.get(interp, strObj, &len);

  const char *chars = NULL;
  size_t charsLen = 0;
  if (argc == 2) {
    TclObj charsObj = ops->list.shift(interp, args);
    chars = ops->string.get(interp, charsObj, &charsLen);
  }

  size_t start = 0;
  while (start < len) {
    int shouldTrim = chars ? in_charset(str[start], chars, charsLen) : is_whitespace(str[start]);
    if (!shouldTrim) break;
    start++;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, str + start, len - start));
  return TCL_OK;
}

// string trimright
static TclResult string_trimright(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string trimright string ?chars?\"", 57);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj strObj = ops->list.shift(interp, args);
  size_t len;
  const char *str = ops->string.get(interp, strObj, &len);

  const char *chars = NULL;
  size_t charsLen = 0;
  if (argc == 2) {
    TclObj charsObj = ops->list.shift(interp, args);
    chars = ops->string.get(interp, charsObj, &charsLen);
  }

  size_t end = len;
  while (end > 0) {
    int shouldTrim = chars ? in_charset(str[end - 1], chars, charsLen) : is_whitespace(str[end - 1]);
    if (!shouldTrim) break;
    end--;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, str, end));
  return TCL_OK;
}

// string map - uses byte-level comparison with ASCII-only case folding for -nocase
// TODO: For full Unicode support, this would need to use rune operations
static TclResult string_map(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  int nocase = 0;
  if (argc >= 1) {
    TclObj first = ops->list.at(interp, args, 0);
    size_t len;
    const char *str = ops->string.get(interp, first, &len);
    if (str_eq(str, len, "-nocase")) {
      nocase = 1;
      ops->list.shift(interp, args);
      argc--;
    }
  }

  if (argc != 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string map ?-nocase? mapping string\"", 61);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj mappingObj = ops->list.shift(interp, args);
  TclObj strObj = ops->list.shift(interp, args);

  // For nocase, we compare folded versions
  TclObj foldedStr = nocase ? ops->rune.fold(interp, strObj) : strObj;

  // Parse mapping as list of key/value pairs
  TclObj mapping = ops->list.from(interp, mappingObj);
  size_t mappingLen = ops->list.length(interp, mapping);
  if (mappingLen % 2 != 0) {
    TclObj msg = ops->string.intern(interp, "char map list unbalanced", 24);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  size_t strLen;
  const char *str = ops->string.get(interp, strObj, &strLen);
  size_t foldedLen;
  const char *folded = ops->string.get(interp, foldedStr, &foldedLen);

  // Build result - note: for nocase with case-folding that changes length,
  // we use the folded string for matching but original for non-matched chars
  TclObj result = ops->string.intern(interp, "", 0);
  size_t i = 0;

  while (i < strLen) {
    int matched = 0;

    // Try each mapping
    for (size_t m = 0; m < mappingLen; m += 2) {
      TclObj keyObj = ops->list.at(interp, mapping, m);
      TclObj valObj = ops->list.at(interp, mapping, m + 1);

      // For nocase, fold the key
      TclObj keyToMatch = nocase ? ops->rune.fold(interp, keyObj) : keyObj;

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
        TclObj valStr = ops->string.intern(interp, val, valLen);
        result = ops->string.concat(interp, result, valStr);
        i += keyLen;
        matched = 1;
        break;
      }
    }

    if (!matched) {
      // Append original character (from folded, which may differ from original)
      TclObj ch = ops->string.intern(interp, &folded[i], 1);
      result = ops->string.concat(interp, result, ch);
      i++;
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

TclResult tcl_builtin_string(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string subcommand ?arg ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj subcmd = ops->list.shift(interp, args);
  size_t len;
  const char *subcmdStr = ops->string.get(interp, subcmd, &len);

  if (str_eq(subcmdStr, len, "length")) {
    return string_length(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "index")) {
    return string_index(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "range")) {
    return string_range(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "match")) {
    return string_match(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "toupper")) {
    return string_toupper(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "tolower")) {
    return string_tolower(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "trim")) {
    return string_trim(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "trimleft")) {
    return string_trimleft(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "trimright")) {
    return string_trimright(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "map")) {
    return string_map(ops, interp, args);
  } else {
    TclObj msg = ops->string.intern(interp, "unknown or ambiguous subcommand \"", 33);
    msg = ops->string.concat(interp, msg, subcmd);
    TclObj suffix = ops->string.intern(interp,
      "\": must be index, length, map, match, range, tolower, toupper, trim, trimleft, or trimright", 91);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
