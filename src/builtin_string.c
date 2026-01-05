#include "feather.h"
#include "internal.h"
#include "index_parse.h"
#include "charclass.h"

// Check if char (as int) is in charset object using byte-at-a-time access
static int in_charset_obj(const FeatherHostOps *ops, FeatherInterp interp,
                          int ch, FeatherObj charsObj) {
  size_t len = ops->string.byte_length(interp, charsObj);
  for (size_t i = 0; i < len; i++) {
    if (ops->string.byte_at(interp, charsObj, i) == ch) return 1;
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
    matches = feather_obj_glob_match(ops, interp, foldedPattern, foldedString);
  } else {
    matches = feather_obj_glob_match(ops, interp, pattern, string);
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

// string totitle
static FeatherResult string_totitle(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string totitle string ?first? ?last?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.shift(interp, args);
  size_t len = ops->rune.length(interp, strObj);

  // Default range is entire string
  int64_t first = 0;
  int64_t last = (int64_t)len - 1;

  // Parse optional first/last arguments
  if (argc >= 2) {
    FeatherObj firstObj = ops->list.shift(interp, args);
    if (feather_parse_index(ops, interp, firstObj, len, &first) != TCL_OK) {
      return TCL_ERROR;
    }
  }
  if (argc >= 3) {
    FeatherObj lastObj = ops->list.shift(interp, args);
    if (feather_parse_index(ops, interp, lastObj, len, &last) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  // Handle empty string
  if (len == 0) {
    ops->interp.set_result(interp, strObj);
    return TCL_OK;
  }

  // Clamp indices
  if (first < 0) first = 0;
  if (last >= (int64_t)len) last = (int64_t)len - 1;

  // If range is invalid, return original string
  if (first > last) {
    ops->interp.set_result(interp, strObj);
    return TCL_OK;
  }

  // Build result: prefix + title-cased range + suffix
  // Title case = first char upper, rest lower
  FeatherObj result = ops->string.intern(interp, "", 0);

  // Add prefix (before first) unchanged
  if (first > 0) {
    FeatherObj prefix = ops->rune.range(interp, strObj, 0, first - 1);
    result = ops->string.concat(interp, result, prefix);
  }

  // Add first character of range uppercased
  FeatherObj firstChar = ops->rune.at(interp, strObj, (size_t)first);
  FeatherObj upperFirst = ops->rune.to_upper(interp, firstChar);
  result = ops->string.concat(interp, result, upperFirst);

  // Add rest of range lowercased
  if (first < last) {
    FeatherObj restOfRange = ops->rune.range(interp, strObj, first + 1, last);
    FeatherObj lowerRest = ops->rune.to_lower(interp, restOfRange);
    result = ops->string.concat(interp, result, lowerRest);
  }

  // Add suffix (after last) unchanged
  if (last < (int64_t)len - 1) {
    FeatherObj suffix = ops->rune.range(interp, strObj, last + 1, len - 1);
    result = ops->string.concat(interp, result, suffix);
  }

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
  size_t len = ops->string.byte_length(interp, strObj);

  FeatherObj charsObj = 0;
  if (argc == 2) {
    charsObj = ops->list.shift(interp, args);
  }

  // Find start (skip leading trim chars)
  size_t start = 0;
  while (start < len) {
    int ch = ops->string.byte_at(interp, strObj, start);
    int shouldTrim = charsObj ?
      in_charset_obj(ops, interp, ch, charsObj) : feather_is_whitespace_full(ch);
    if (!shouldTrim) break;
    start++;
  }

  // Find end (skip trailing trim chars)
  size_t end = len;
  while (end > start) {
    int ch = ops->string.byte_at(interp, strObj, end - 1);
    int shouldTrim = charsObj ?
      in_charset_obj(ops, interp, ch, charsObj) : feather_is_whitespace_full(ch);
    if (!shouldTrim) break;
    end--;
  }

  ops->interp.set_result(interp, ops->string.slice(interp, strObj, start, end));
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
  size_t len = ops->string.byte_length(interp, strObj);

  FeatherObj charsObj = 0;
  if (argc == 2) {
    charsObj = ops->list.shift(interp, args);
  }

  size_t start = 0;
  while (start < len) {
    int ch = ops->string.byte_at(interp, strObj, start);
    int shouldTrim = charsObj ?
      in_charset_obj(ops, interp, ch, charsObj) : feather_is_whitespace_full(ch);
    if (!shouldTrim) break;
    start++;
  }

  ops->interp.set_result(interp, ops->string.slice(interp, strObj, start, len));
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
  size_t len = ops->string.byte_length(interp, strObj);

  FeatherObj charsObj = 0;
  if (argc == 2) {
    charsObj = ops->list.shift(interp, args);
  }

  size_t end = len;
  while (end > 0) {
    int ch = ops->string.byte_at(interp, strObj, end - 1);
    int shouldTrim = charsObj ?
      in_charset_obj(ops, interp, ch, charsObj) : feather_is_whitespace_full(ch);
    if (!shouldTrim) break;
    end--;
  }

  ops->interp.set_result(interp, ops->string.slice(interp, strObj, 0, end));
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

  size_t foldedLen = ops->string.byte_length(interp, foldedStr);

  // Build result - note: for nocase with case-folding that changes length,
  // we use the folded string for matching but original for non-matched chars
  FeatherObj result = ops->string.intern(interp, "", 0);
  size_t i = 0;

  while (i < foldedLen) {
    int matched = 0;

    // Try each mapping
    for (size_t m = 0; m < mappingLen; m += 2) {
      FeatherObj keyObj = ops->list.at(interp, mapping, m);
      FeatherObj valObj = ops->list.at(interp, mapping, m + 1);

      // For nocase, fold the key
      FeatherObj keyToMatch = nocase ? ops->rune.fold(interp, keyObj) : keyObj;

      size_t keyLen = ops->string.byte_length(interp, keyToMatch);
      if (keyLen == 0) continue;

      // Check if key matches at position i in folded string
      if (feather_obj_matches_at(ops, interp, foldedStr, i, keyToMatch)) {
        // Append replacement value
        result = ops->string.concat(interp, result, valObj);
        i += keyLen;
        matched = 1;
        break;
      }
    }

    if (!matched) {
      // Append original character (from folded, which may differ from original)
      FeatherObj ch = ops->string.slice(interp, foldedStr, i, i + 1);
      result = ops->string.concat(interp, result, ch);
      i++;
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string cat ?string1? ?string2? ...
static FeatherResult string_cat(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  FeatherObj result = ops->string.intern(interp, "", 0);
  while (ops->list.length(interp, args) > 0) {
    FeatherObj str = ops->list.shift(interp, args);
    result = ops->string.concat(interp, result, str);
  }
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string compare ?-nocase? ?-length len? string1 string2
static FeatherResult string_compare(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  int nocase = 0;
  int64_t length = -1;

  // Parse options
  while (ops->list.length(interp, args) > 2) {
    FeatherObj opt = ops->list.at(interp, args, 0);
    if (feather_obj_eq_literal(ops, interp, opt, "-nocase")) {
      nocase = 1;
      ops->list.shift(interp, args);
    } else if (feather_obj_eq_literal(ops, interp, opt, "-length")) {
      ops->list.shift(interp, args);
      if (ops->list.length(interp, args) < 3) {
        FeatherObj msg = ops->string.intern(interp,
          "wrong # args: should be \"string compare ?-nocase? ?-length int? string1 string2\"", 79);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj lenObj = ops->list.shift(interp, args);
      if (ops->integer.get(interp, lenObj, &length) != TCL_OK) {
        feather_error_expected(ops, interp, "integer", lenObj);
        return TCL_ERROR;
      }
    } else {
      break;
    }
  }

  if (ops->list.length(interp, args) != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string compare ?-nocase? ?-length int? string1 string2\"", 79);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str1 = ops->list.shift(interp, args);
  FeatherObj str2 = ops->list.shift(interp, args);

  // Apply length limit if specified
  if (length >= 0) {
    str1 = ops->rune.range(interp, str1, 0, length - 1);
    str2 = ops->rune.range(interp, str2, 0, length - 1);
  }

  // Apply case folding if -nocase
  if (nocase) {
    str1 = ops->rune.fold(interp, str1);
    str2 = ops->rune.fold(interp, str2);
  }

  int cmp = ops->string.compare(interp, str1, str2);
  // Normalize to -1, 0, 1
  if (cmp < 0) cmp = -1;
  else if (cmp > 0) cmp = 1;

  ops->interp.set_result(interp, ops->integer.create(interp, cmp));
  return TCL_OK;
}

// string equal ?-nocase? ?-length len? string1 string2
static FeatherResult string_equal(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  int nocase = 0;
  int64_t length = -1;

  // Parse options
  while (ops->list.length(interp, args) > 2) {
    FeatherObj opt = ops->list.at(interp, args, 0);
    if (feather_obj_eq_literal(ops, interp, opt, "-nocase")) {
      nocase = 1;
      ops->list.shift(interp, args);
    } else if (feather_obj_eq_literal(ops, interp, opt, "-length")) {
      ops->list.shift(interp, args);
      if (ops->list.length(interp, args) < 3) {
        FeatherObj msg = ops->string.intern(interp,
          "wrong # args: should be \"string equal ?-nocase? ?-length int? string1 string2\"", 77);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj lenObj = ops->list.shift(interp, args);
      if (ops->integer.get(interp, lenObj, &length) != TCL_OK) {
        feather_error_expected(ops, interp, "integer", lenObj);
        return TCL_ERROR;
      }
    } else {
      break;
    }
  }

  if (ops->list.length(interp, args) != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string equal ?-nocase? ?-length int? string1 string2\"", 77);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str1 = ops->list.shift(interp, args);
  FeatherObj str2 = ops->list.shift(interp, args);

  // Apply length limit if specified
  if (length >= 0) {
    str1 = ops->rune.range(interp, str1, 0, length - 1);
    str2 = ops->rune.range(interp, str2, 0, length - 1);
  }

  // Apply case folding if -nocase
  if (nocase) {
    str1 = ops->rune.fold(interp, str1);
    str2 = ops->rune.fold(interp, str2);
  }

  int eq = ops->string.equal(interp, str1, str2);
  ops->interp.set_result(interp, ops->integer.create(interp, eq ? 1 : 0));
  return TCL_OK;
}

// string first needleString haystackString ?startIndex?
static FeatherResult string_first(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2 || argc > 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string first needleString haystackString ?startIndex?\"", 79);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj needle = ops->list.shift(interp, args);
  FeatherObj haystack = ops->list.shift(interp, args);

  size_t haystackLen = ops->rune.length(interp, haystack);
  size_t needleLen = ops->rune.length(interp, needle);

  int64_t startIndex = 0;
  if (argc == 3) {
    FeatherObj startObj = ops->list.shift(interp, args);
    if (feather_parse_index(ops, interp, startObj, haystackLen, &startIndex) != TCL_OK) {
      return TCL_ERROR;
    }
    if (startIndex < 0) startIndex = 0;
  }

  // Empty needle never matches
  if (needleLen == 0) {
    ops->interp.set_result(interp, ops->integer.create(interp, -1));
    return TCL_OK;
  }

  // Search for needle starting at startIndex
  for (size_t i = (size_t)startIndex; i + needleLen <= haystackLen; i++) {
    FeatherObj sub = ops->rune.range(interp, haystack, i, i + needleLen - 1);
    if (ops->string.equal(interp, sub, needle)) {
      ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)i));
      return TCL_OK;
    }
  }

  ops->interp.set_result(interp, ops->integer.create(interp, -1));
  return TCL_OK;
}

// string last needleString haystackString ?lastIndex?
static FeatherResult string_last(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2 || argc > 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string last needleString haystackString ?lastIndex?\"", 77);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj needle = ops->list.shift(interp, args);
  FeatherObj haystack = ops->list.shift(interp, args);

  size_t haystackLen = ops->rune.length(interp, haystack);
  size_t needleLen = ops->rune.length(interp, needle);

  int64_t lastIndex = (int64_t)haystackLen - 1;
  if (argc == 3) {
    FeatherObj lastObj = ops->list.shift(interp, args);
    if (feather_parse_index(ops, interp, lastObj, haystackLen, &lastIndex) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  // Empty needle never matches
  if (needleLen == 0) {
    ops->interp.set_result(interp, ops->integer.create(interp, -1));
    return TCL_OK;
  }

  // Search backwards for needle, starting position must allow full needle to fit
  int64_t maxStart = lastIndex;
  if (maxStart + (int64_t)needleLen > (int64_t)haystackLen) {
    maxStart = (int64_t)haystackLen - (int64_t)needleLen;
  }

  for (int64_t i = maxStart; i >= 0; i--) {
    FeatherObj sub = ops->rune.range(interp, haystack, i, i + needleLen - 1);
    if (ops->string.equal(interp, sub, needle)) {
      ops->interp.set_result(interp, ops->integer.create(interp, i));
      return TCL_OK;
    }
  }

  ops->interp.set_result(interp, ops->integer.create(interp, -1));
  return TCL_OK;
}

// string repeat string count
static FeatherResult string_repeat(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string repeat string count\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.shift(interp, args);
  FeatherObj countObj = ops->list.shift(interp, args);

  int64_t count;
  if (ops->integer.get(interp, countObj, &count) != TCL_OK) {
    feather_error_expected(ops, interp, "integer", countObj);
    return TCL_ERROR;
  }

  if (count < 0) {
    FeatherObj msg = ops->string.intern(interp, "bad count \"", 11);
    msg = ops->string.concat(interp, msg, countObj);
    FeatherObj suffix = ops->string.intern(interp, "\": must be integer >= 0", 23);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj result = ops->string.intern(interp, "", 0);
  for (int64_t i = 0; i < count; i++) {
    result = ops->string.concat(interp, result, str);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// string reverse string
static FeatherResult string_reverse(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string reverse string\"", 47);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.shift(interp, args);
  size_t len = ops->rune.length(interp, str);

  if (len == 0) {
    ops->interp.set_result(interp, str);
    return TCL_OK;
  }

  // Build reversed string using builder
  FeatherObj builder = ops->string.builder_new(interp, ops->string.byte_length(interp, str));
  for (size_t i = len; i > 0; i--) {
    FeatherObj ch = ops->rune.at(interp, str, i - 1);
    ops->string.builder_append_obj(interp, builder, ch);
  }

  ops->interp.set_result(interp, ops->string.builder_finish(interp, builder));
  return TCL_OK;
}

// string insert string index insertString
static FeatherResult string_insert(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string insert string index insertString\"", 65);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.shift(interp, args);
  FeatherObj indexObj = ops->list.shift(interp, args);
  FeatherObj insertStr = ops->list.shift(interp, args);

  size_t len = ops->rune.length(interp, str);

  int64_t index;
  // For insert, "end" means "after all chars" = position len, so use len+1 as reference
  if (feather_parse_index(ops, interp, indexObj, len + 1, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  // Clamp index
  if (index < 0) index = 0;
  if ((size_t)index > len) index = (int64_t)len;

  // Build result: str[0..index-1] + insertStr + str[index..end]
  FeatherObj prefix = (index > 0) ? ops->rune.range(interp, str, 0, index - 1) : ops->string.intern(interp, "", 0);
  FeatherObj suffix = ((size_t)index < len) ? ops->rune.range(interp, str, index, len - 1) : ops->string.intern(interp, "", 0);

  FeatherObj result = ops->string.concat(interp, prefix, insertStr);
  result = ops->string.concat(interp, result, suffix);

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// Character class type for string is
typedef enum {
  CLASS_ALNUM,
  CLASS_ALPHA,
  CLASS_ASCII,
  CLASS_BOOLEAN,
  CLASS_CONTROL,
  CLASS_DICT,
  CLASS_DIGIT,
  CLASS_DOUBLE,
  CLASS_FALSE,
  CLASS_GRAPH,
  CLASS_INTEGER,
  CLASS_LIST,
  CLASS_LOWER,
  CLASS_PRINT,
  CLASS_PUNCT,
  CLASS_SPACE,
  CLASS_TRUE,
  CLASS_UPPER,
  CLASS_WORDCHAR,
  CLASS_XDIGIT,
  CLASS_UNKNOWN
} StringIsClass;

// Parse class name to enum
static StringIsClass parse_class(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj classObj) {
  if (feather_obj_eq_literal(ops, interp, classObj, "alnum")) return CLASS_ALNUM;
  if (feather_obj_eq_literal(ops, interp, classObj, "alpha")) return CLASS_ALPHA;
  if (feather_obj_eq_literal(ops, interp, classObj, "ascii")) return CLASS_ASCII;
  if (feather_obj_eq_literal(ops, interp, classObj, "boolean")) return CLASS_BOOLEAN;
  if (feather_obj_eq_literal(ops, interp, classObj, "control")) return CLASS_CONTROL;
  if (feather_obj_eq_literal(ops, interp, classObj, "dict")) return CLASS_DICT;
  if (feather_obj_eq_literal(ops, interp, classObj, "digit")) return CLASS_DIGIT;
  if (feather_obj_eq_literal(ops, interp, classObj, "double")) return CLASS_DOUBLE;
  if (feather_obj_eq_literal(ops, interp, classObj, "false")) return CLASS_FALSE;
  if (feather_obj_eq_literal(ops, interp, classObj, "graph")) return CLASS_GRAPH;
  if (feather_obj_eq_literal(ops, interp, classObj, "integer")) return CLASS_INTEGER;
  if (feather_obj_eq_literal(ops, interp, classObj, "list")) return CLASS_LIST;
  if (feather_obj_eq_literal(ops, interp, classObj, "lower")) return CLASS_LOWER;
  if (feather_obj_eq_literal(ops, interp, classObj, "print")) return CLASS_PRINT;
  if (feather_obj_eq_literal(ops, interp, classObj, "punct")) return CLASS_PUNCT;
  if (feather_obj_eq_literal(ops, interp, classObj, "space")) return CLASS_SPACE;
  if (feather_obj_eq_literal(ops, interp, classObj, "true")) return CLASS_TRUE;
  if (feather_obj_eq_literal(ops, interp, classObj, "upper")) return CLASS_UPPER;
  if (feather_obj_eq_literal(ops, interp, classObj, "wordchar")) return CLASS_WORDCHAR;
  if (feather_obj_eq_literal(ops, interp, classObj, "xdigit")) return CLASS_XDIGIT;
  return CLASS_UNKNOWN;
}

// Map StringIsClass to FeatherCharClass for character testing
static FeatherCharClass class_to_char_class(StringIsClass cls) {
  switch (cls) {
    case CLASS_ALNUM: return FEATHER_CHAR_ALNUM;
    case CLASS_ALPHA: return FEATHER_CHAR_ALPHA;
    case CLASS_ASCII: return FEATHER_CHAR_ASCII;
    case CLASS_CONTROL: return FEATHER_CHAR_CONTROL;
    case CLASS_DIGIT: return FEATHER_CHAR_DIGIT;
    case CLASS_GRAPH: return FEATHER_CHAR_GRAPH;
    case CLASS_LOWER: return FEATHER_CHAR_LOWER;
    case CLASS_PRINT: return FEATHER_CHAR_PRINT;
    case CLASS_PUNCT: return FEATHER_CHAR_PUNCT;
    case CLASS_SPACE: return FEATHER_CHAR_SPACE;
    case CLASS_UPPER: return FEATHER_CHAR_UPPER;
    case CLASS_WORDCHAR: return FEATHER_CHAR_WORDCHAR;
    case CLASS_XDIGIT: return FEATHER_CHAR_XDIGIT;
    default: return FEATHER_CHAR_ALNUM; // Should not happen
  }
}

// Check if class is a character class (vs value class)
static int is_char_class(StringIsClass cls) {
  return cls == CLASS_ALNUM || cls == CLASS_ALPHA || cls == CLASS_ASCII ||
         cls == CLASS_CONTROL || cls == CLASS_DIGIT || cls == CLASS_GRAPH ||
         cls == CLASS_LOWER || cls == CLASS_PRINT || cls == CLASS_PUNCT ||
         cls == CLASS_SPACE || cls == CLASS_UPPER || cls == CLASS_WORDCHAR ||
         cls == CLASS_XDIGIT;
}

// Check if a string is a valid true value
static int is_true_value(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj str) {
  if (feather_obj_eq_literal(ops, interp, str, "true")) return 1;
  if (feather_obj_eq_literal(ops, interp, str, "yes")) return 1;
  if (feather_obj_eq_literal(ops, interp, str, "on")) return 1;
  if (feather_obj_eq_literal(ops, interp, str, "1")) return 1;
  return 0;
}

// Check if a string is a valid false value
static int is_false_value(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj str) {
  if (feather_obj_eq_literal(ops, interp, str, "false")) return 1;
  if (feather_obj_eq_literal(ops, interp, str, "no")) return 1;
  if (feather_obj_eq_literal(ops, interp, str, "off")) return 1;
  if (feather_obj_eq_literal(ops, interp, str, "0")) return 1;
  return 0;
}

// string is class ?-strict? ?-failindex varname? string
static FeatherResult string_is(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string is class ?-strict? ?-failindex var? str\"", 72);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj classObj = ops->list.shift(interp, args);
  StringIsClass cls = parse_class(ops, interp, classObj);

  if (cls == CLASS_UNKNOWN) {
    FeatherObj msg = ops->string.intern(interp, "bad class \"", 11);
    msg = ops->string.concat(interp, msg, classObj);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be alnum, alpha, ascii, boolean, control, dict, digit, double, false, graph, integer, list, lower, print, punct, space, true, upper, wordchar, or xdigit", 160);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  int strict = 0;
  FeatherObj failindexVar = 0;

  // Parse options
  while (ops->list.length(interp, args) > 1) {
    FeatherObj opt = ops->list.at(interp, args, 0);
    if (feather_obj_eq_literal(ops, interp, opt, "-strict")) {
      strict = 1;
      ops->list.shift(interp, args);
    } else if (feather_obj_eq_literal(ops, interp, opt, "-failindex")) {
      ops->list.shift(interp, args);
      if (ops->list.length(interp, args) < 2) {
        FeatherObj msg = ops->string.intern(interp,
          "wrong # args: should be \"string is class ?-strict? ?-failindex var? str\"", 72);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      failindexVar = ops->list.shift(interp, args);
    } else {
      break;
    }
  }

  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string is class ?-strict? ?-failindex var? str\"", 72);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.shift(interp, args);
  size_t len = ops->rune.length(interp, str);

  // Handle value classes first
  if (!is_char_class(cls)) {
    int result = 0;

    switch (cls) {
      case CLASS_BOOLEAN:
        result = is_true_value(ops, interp, str) || is_false_value(ops, interp, str);
        break;
      case CLASS_TRUE:
        result = is_true_value(ops, interp, str);
        break;
      case CLASS_FALSE:
        result = is_false_value(ops, interp, str);
        break;
      case CLASS_INTEGER: {
        int64_t dummy;
        result = (ops->integer.get(interp, str, &dummy) == TCL_OK);
        break;
      }
      case CLASS_DOUBLE: {
        double dummy;
        result = (ops->dbl.get(interp, str, &dummy) == TCL_OK);
        break;
      }
      case CLASS_LIST: {
        FeatherObj listObj = ops->list.from(interp, str);
        result = (listObj != 0);
        break;
      }
      case CLASS_DICT: {
        FeatherObj dictObj = ops->dict.from(interp, str);
        result = (dictObj != 0);
        break;
      }
      default:
        result = 0;
    }

    // Clear any error set during value parsing
    ops->interp.reset_result(interp, ops->string.intern(interp, "", 0));

    if (failindexVar && !result) {
      // For value classes, failindex is set to the end of string
      if (feather_set_var(ops, interp, failindexVar, ops->integer.create(interp, 0)) != TCL_OK) {
        return TCL_ERROR;
      }
    }

    ops->interp.set_result(interp, ops->integer.create(interp, result ? 1 : 0));
    return TCL_OK;
  }

  // Handle character classes
  // Empty string: true unless -strict
  if (len == 0) {
    if (failindexVar && strict) {
      if (feather_set_var(ops, interp, failindexVar, ops->integer.create(interp, 0)) != TCL_OK) {
        return TCL_ERROR;
      }
    }
    ops->interp.set_result(interp, ops->integer.create(interp, strict ? 0 : 1));
    return TCL_OK;
  }

  FeatherCharClass charClass = class_to_char_class(cls);

  // Check each character
  for (size_t i = 0; i < len; i++) {
    FeatherObj ch = ops->rune.at(interp, str, i);
    if (!ops->rune.is_class(interp, ch, charClass)) {
      if (failindexVar) {
        if (feather_set_var(ops, interp, failindexVar, ops->integer.create(interp, (int64_t)i)) != TCL_OK) {
          return TCL_ERROR;
        }
      }
      ops->interp.set_result(interp, ops->integer.create(interp, 0));
      return TCL_OK;
    }
  }

  ops->interp.set_result(interp, ops->integer.create(interp, 1));
  return TCL_OK;
}

// string replace string first last ?newString?
static FeatherResult string_replace(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 3 || argc > 4) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"string replace string first last ?newString?\"", 70);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.shift(interp, args);
  FeatherObj firstObj = ops->list.shift(interp, args);
  FeatherObj lastObj = ops->list.shift(interp, args);

  FeatherObj newStr = ops->string.intern(interp, "", 0);
  if (argc == 4) {
    newStr = ops->list.shift(interp, args);
  }

  size_t len = ops->rune.length(interp, str);

  int64_t first, last;
  if (feather_parse_index(ops, interp, firstObj, len, &first) != TCL_OK) {
    return TCL_ERROR;
  }
  if (feather_parse_index(ops, interp, lastObj, len, &last) != TCL_OK) {
    return TCL_ERROR;
  }

  // Handle out of bounds - if first > last or first >= len, return original
  if (first > last || first >= (int64_t)len) {
    ops->interp.set_result(interp, str);
    return TCL_OK;
  }

  // Clamp
  if (first < 0) first = 0;
  if (last >= (int64_t)len) last = (int64_t)len - 1;

  // Build result: str[0..first-1] + newStr + str[last+1..end]
  FeatherObj prefix = (first > 0) ? ops->rune.range(interp, str, 0, first - 1) : ops->string.intern(interp, "", 0);
  FeatherObj suffix = (last + 1 < (int64_t)len) ? ops->rune.range(interp, str, last + 1, len - 1) : ops->string.intern(interp, "", 0);

  FeatherObj result = ops->string.concat(interp, prefix, newStr);
  result = ops->string.concat(interp, result, suffix);

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_string_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Manipulate strings",
    "Performs one of several string operations, depending on subcommand. "
    "The legal subcommands are: cat, compare, equal, first, index, insert, is, "
    "last, length, map, match, range, repeat, replace, reverse, tolower, totitle, "
    "toupper, trim, trimleft, and trimright.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "String Indices",
    "When referring to indices into a string (e.g., for string index or string "
    "range) the following formats are supported:\n\n"
    "integer    The character at the specified integral index (0 = first char)\n\n"
    "end        The last character of the string\n\n"
    "end-N      The last character minus offset N\n\n"
    "end+N      The last character plus offset N (e.g., end+-1)\n\n"
    "M+N        The character at index M plus N\n\n"
    "M-N        The character at index M minus N");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: cat ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?string?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "cat", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Concatenate the given strings just like placing them directly next to each "
    "other and return the resulting compound string. If no strings are present, "
    "the result is an empty string.\n\n"
    "This primitive is occasionally handier than juxtaposition of strings when "
    "mixed quoting is wanted, or when the aim is to return the result of a "
    "concatenation without resorting to return -level 0.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: compare ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-nocase", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Compare case-insensitively");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_flag(ops, interp, "-length", NULL, "<len>");
  e = feather_usage_help(ops, interp, e, "Compare only first N characters");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string1>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string2>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "compare", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Perform a character-by-character comparison of strings string1 and string2. "
    "Returns -1, 0, or 1, depending on whether string1 is lexicographically less "
    "than, equal to, or greater than string2.\n\n"
    "If -length is specified, then only the first length characters are used in "
    "the comparison. If -length is negative, it is ignored. If -nocase is "
    "specified, then the strings are compared in a case-insensitive manner.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: equal ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-nocase", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Compare case-insensitively");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_flag(ops, interp, "-length", NULL, "<len>");
  e = feather_usage_help(ops, interp, e, "Compare only first N characters");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string1>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string2>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "equal", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Perform a character-by-character comparison of strings string1 and string2. "
    "Returns 1 if string1 and string2 are identical, or 0 when not.\n\n"
    "If -length is specified, then only the first length characters are used in "
    "the comparison. If -length is negative, it is ignored. If -nocase is "
    "specified, then the strings are compared in a case-insensitive manner.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: first ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<needleString>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<haystackString>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?startIndex?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "first", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Search haystackString for a sequence of characters that exactly match the "
    "characters in needleString. If found, return the index of the first "
    "character in the first such match within haystackString. If not found, "
    "return -1.\n\n"
    "If startIndex is specified (in any of the forms described in STRING INDICES), "
    "then the search is constrained to start with the character in haystackString "
    "specified by the index.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: index ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<charIndex>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "index", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the charIndex'th character of the string argument. A charIndex of 0 "
    "corresponds to the first character of the string. charIndex may be specified "
    "as described in the STRING INDICES section.\n\n"
    "If charIndex is less than 0 or greater than or equal to the length of the "
    "string then this command returns an empty string.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: insert ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<index>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<insertString>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "insert", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a copy of string with insertString inserted at the index'th "
    "character. The index may be specified as described in the STRING INDICES "
    "section.\n\n"
    "If index is at or before the start of string (e.g., index is 0), insertString "
    "is prepended to string. If index is at or after the end of string (e.g., "
    "index is end), insertString is appended to string.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: is ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<class>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_flag(ops, interp, "-strict", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Empty string returns 0 instead of 1");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_flag(ops, interp, "-failindex", NULL, "<varname>");
  e = feather_usage_help(ops, interp, e, "Variable to store failing index");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "is", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if string is a valid member of the specified character class, "
    "otherwise returns 0. If -strict is specified, then an empty string returns "
    "0, otherwise an empty string will return 1 on any class. If -failindex is "
    "specified, then if the function returns 0, the index in the string where "
    "the class was no longer valid will be stored in the variable named varname.\n\n"
    "Character classes: alnum, alpha, ascii, control, digit, graph, lower, print, "
    "punct, space, upper, wordchar, xdigit.\n\n"
    "Value classes: boolean, true, false, integer, double, list, dict.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: last ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<needleString>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<haystackString>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?lastIndex?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "last", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Search haystackString for a sequence of characters that exactly match the "
    "characters in needleString. If found, return the index of the first "
    "character in the last such match within haystackString. If there is no "
    "match, then return -1.\n\n"
    "If lastIndex is specified (in any of the forms described in STRING INDICES), "
    "then only the characters in haystackString at or before the specified "
    "lastIndex will be considered by the search.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: length ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "length", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a decimal string giving the number of characters in string. Note "
    "that this is not necessarily the same as the number of bytes used to store "
    "the string.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: map ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-nocase", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Match keys case-insensitively");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<mapping>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "map", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Replaces substrings in string based on the key-value pairs in mapping. "
    "mapping is a list of key value key value ... pairs. Each instance of a key "
    "in the string will be replaced with its corresponding value. If -nocase is "
    "specified, then matching is done without regard to case differences.\n\n"
    "Replacement is done in an ordered manner, so the key appearing first in the "
    "list will be checked first. string is only iterated over once, so earlier "
    "key replacements will have no effect for later key matches.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: match ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-nocase", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Match case-insensitively");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<pattern>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "match", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "See if pattern matches string; return 1 if it does, 0 if it does not. If "
    "-nocase is specified, then the pattern attempts to match against the string "
    "in a case insensitive manner.\n\n"
    "The following special sequences may appear in pattern:\n\n"
    "*          Matches any sequence of characters, including empty\n\n"
    "?          Matches any single character\n\n"
    "[chars]    Matches any character in the set, including ranges like a-z\n\n"
    "\\x         Matches the single character x literally");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: range ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<first>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<last>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "range", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a range of consecutive characters from string, starting with the "
    "character whose index is first and ending with the character whose index is "
    "last. An index of 0 refers to the first character of the string; an index of "
    "end refers to the last character. first and last may be specified as "
    "described in STRING INDICES.\n\n"
    "If first is less than zero then it is treated as if it were zero, and if "
    "last is greater than or equal to the length of the string then it is treated "
    "as if it were end. If first is greater than last then an empty string is "
    "returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: repeat ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<count>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "repeat", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a string consisting of string concatenated with itself count times. "
    "If count is 0, the empty string will be returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: replace ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<first>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<last>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?newstring?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "replace", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Removes a range of consecutive characters from string, starting with the "
    "character whose index is first and ending with the character whose index is "
    "last. An index of 0 refers to the first character of the string. first and "
    "last may be specified as described in STRING INDICES.\n\n"
    "If newstring is specified, then it is placed in the removed character range. "
    "The initial string is returned untouched if first is greater than last, or "
    "if first is equal to or greater than the length of the initial string, or "
    "last is less than 0.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: reverse ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "reverse", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a string that is the same length as string but with its characters "
    "in the reverse order.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: tolower ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?first?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?last?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "tolower", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a value equal to string except that all upper (or title) case "
    "letters have been converted to lower case.\n\n"
    "Note: In Feather, the optional first and last arguments are accepted for "
    "compatibility but are currently ignored. The entire string is always "
    "converted.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: totitle ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?first?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?last?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "totitle", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a value equal to string except that the first character in string "
    "(or the range specified by first and last) is converted to its Unicode "
    "title case variant (or upper case if there is no title case variant) and "
    "the rest of the string is converted to lower case.\n\n"
    "If first is specified, it refers to the first char index in the string to "
    "start modifying. If last is specified, it refers to the char index in the "
    "string to stop at (inclusive). first and last may be specified using the "
    "forms described in STRING INDICES.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: toupper ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?first?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?last?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "toupper", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a value equal to string except that all lower (or title) case "
    "letters have been converted to upper case.\n\n"
    "Note: In Feather, the optional first and last arguments are accepted for "
    "compatibility but are currently ignored. The entire string is always "
    "converted.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: trim ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?chars?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "trim", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a value equal to string except that any leading or trailing "
    "characters present in the string given by chars are removed. If chars is "
    "not specified then white space is removed (space, tab, newline, carriage "
    "return, vertical tab, form feed).");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: trimleft ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?chars?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "trimleft", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a value equal to string except that any leading characters present "
    "in the string given by chars are removed. If chars is not specified then "
    "white space is removed (space, tab, newline, carriage return, vertical tab, "
    "form feed).");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: trimright ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?chars?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "trimright", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a value equal to string except that any trailing characters present "
    "in the string given by chars are removed. If chars is not specified then "
    "white space is removed (space, tab, newline, carriage return, vertical tab, "
    "form feed).");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Examples ---
  e = feather_usage_example(ops, interp,
    "string first a 0a23456789abcdef 5",
    "Find 'a' starting at index 5 (returns 10):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "string map {abc 1 ab 2 a 3 1 0} 1abcaababcababc",
    "Apply string substitutions (returns 01teleprinter22211):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "string", spec);
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
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "totitle")) {
    return string_totitle(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "trim")) {
    return string_trim(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "trimleft")) {
    return string_trimleft(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "trimright")) {
    return string_trimright(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "map")) {
    return string_map(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "cat")) {
    return string_cat(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "compare")) {
    return string_compare(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "equal")) {
    return string_equal(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "first")) {
    return string_first(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "last")) {
    return string_last(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "repeat")) {
    return string_repeat(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "reverse")) {
    return string_reverse(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "insert")) {
    return string_insert(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "is")) {
    return string_is(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "replace")) {
    return string_replace(ops, interp, args);
  } else {
    FeatherObj msg = ops->string.intern(interp, "unknown or ambiguous subcommand \"", 33);
    msg = ops->string.concat(interp, msg, subcmd);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be cat, compare, equal, first, index, insert, is, last, length, map, match, range, repeat, replace, reverse, tolower, totitle, toupper, trim, trimleft, or trimright", 172);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
