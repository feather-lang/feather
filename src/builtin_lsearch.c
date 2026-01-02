#include "feather.h"
#include "internal.h"
#include "charclass.h"

// Match mode
typedef enum {
  MATCH_EXACT,
  MATCH_GLOB,
  MATCH_REGEXP
} MatchMode;

static int lsearch_compare_nocase(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj a, FeatherObj b) {
  size_t lenA = ops->string.byte_length(interp, a);
  size_t lenB = ops->string.byte_length(interp, b);

  if (lenA != lenB) return 0;
  for (size_t i = 0; i < lenA; i++) {
    int ca = feather_char_tolower((unsigned char)ops->string.byte_at(interp, a, i));
    int cb = feather_char_tolower((unsigned char)ops->string.byte_at(interp, b, i));
    if (ca != cb) return 0;
  }
  return 1;
}

// Glob match with nocase support
static int glob_match_nocase(const char *pattern, size_t plen,
                             const char *string, size_t slen) {
  // Simple case-insensitive glob matching
  // For a complete implementation, we'd need to lowercase both
  // For now, just use the existing feather_glob_match for case-sensitive
  // This is a simplified version
  size_t p = 0, s = 0;
  size_t starP = (size_t)-1, starS = 0;

  while (s < slen) {
    if (p < plen && (pattern[p] == '?' ||
                     feather_char_tolower((unsigned char)pattern[p]) ==
                     feather_char_tolower((unsigned char)string[s]))) {
      p++;
      s++;
    } else if (p < plen && pattern[p] == '*') {
      starP = p;
      starS = s;
      p++;
    } else if (starP != (size_t)-1) {
      p = starP + 1;
      starS++;
      s = starS;
    } else {
      return 0;
    }
  }

  while (p < plen && pattern[p] == '*') p++;
  return p == plen;
}

// Check if element matches pattern
static int element_matches(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj element, FeatherObj pattern,
                           MatchMode mode, int nocase, int negate) {
  int matches = 0;

  switch (mode) {
    case MATCH_EXACT:
      if (nocase) {
        matches = lsearch_compare_nocase(ops, interp, element, pattern);
      } else {
        matches = (ops->string.compare(interp, element, pattern) == 0);
      }
      break;

    case MATCH_GLOB: {
      // Use host's glob match which supports nocase via the third parameter
      matches = ops->string.match(interp, pattern, element, nocase);
      break;
    }

    case MATCH_REGEXP: {
      int result;
      if (ops->string.regex_match(interp, pattern, element, &result) == TCL_OK) {
        matches = result;
      }
      break;
    }
  }

  return negate ? !matches : matches;
}

FeatherResult feather_builtin_lsearch(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lsearch ?options? list pattern\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Parse options
  MatchMode mode = MATCH_GLOB;  // Default is glob
  int nocase = 0;
  int all = 0;
  int inlineResult = 0;
  int negate = 0;
  int64_t startIndex = 0;
  int hasIndex = 0;
  int64_t searchIndex = 0;

  // Process options
  while (ops->list.length(interp, args) > 2) {
    FeatherObj arg = ops->list.shift(interp, args);

    if (feather_obj_eq_literal(ops, interp, arg, "-exact")) {
      mode = MATCH_EXACT;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-glob")) {
      mode = MATCH_GLOB;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-regexp")) {
      mode = MATCH_REGEXP;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-nocase")) {
      nocase = 1;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-all")) {
      all = 1;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-inline")) {
      inlineResult = 1;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-not")) {
      negate = 1;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-start")) {
      // -start requires an argument
      if (ops->list.length(interp, args) < 3) {
        FeatherObj msg = ops->string.intern(interp, "missing starting index", 22);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj startArg = ops->list.shift(interp, args);
      if (ops->integer.get(interp, startArg, &startIndex) != TCL_OK) {
        FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
        msg = ops->string.concat(interp, msg, startArg);
        FeatherObj suffix = ops->string.intern(interp, "\": must be integer?[+-]integer? or end?[+-]integer?", 51);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      // Clamp negative to 0
      if (startIndex < 0) startIndex = 0;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-index")) {
      // -index requires an argument
      if (ops->list.length(interp, args) < 3) {
        FeatherObj msg = ops->string.intern(interp, "\"-index\" option must be followed by list index", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj indexArg = ops->list.shift(interp, args);
      if (ops->integer.get(interp, indexArg, &searchIndex) != TCL_OK) {
        FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
        msg = ops->string.concat(interp, msg, indexArg);
        FeatherObj suffix = ops->string.intern(interp, "\": must be integer?[+-]integer? or end?[+-]integer?", 51);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      hasIndex = 1;
    } else {
      FeatherObj msg = ops->string.intern(interp, "bad option \"", 12);
      msg = ops->string.concat(interp, msg, arg);
      FeatherObj suffix = ops->string.intern(interp, "\"", 1);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj pattern = ops->list.shift(interp, args);

  // Convert to list
  FeatherObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Clamp startIndex to list length (returns -1 or empty for out of range)
  size_t start = (size_t)startIndex;
  if (start > listLen) start = listLen;

  if (all) {
    // Return all matching indices/elements
    FeatherObj result = ops->list.create(interp);
    for (size_t i = start; i < listLen; i++) {
      FeatherObj elem = ops->list.at(interp, list, i);
      // For -index, extract the element at the specified index within the sublist
      FeatherObj matchElem = elem;
      if (hasIndex) {
        FeatherObj sublist = ops->list.from(interp, elem);
        size_t sublistLen = ops->list.length(interp, sublist);
        if (searchIndex >= 0 && (size_t)searchIndex < sublistLen) {
          matchElem = ops->list.at(interp, sublist, (size_t)searchIndex);
        } else {
          // Index out of range - skip this element (doesn't match)
          continue;
        }
      }
      if (element_matches(ops, interp, matchElem, pattern, mode, nocase, negate)) {
        if (inlineResult) {
          result = ops->list.push(interp, result, elem);
        } else {
          result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)i));
        }
      }
    }
    ops->interp.set_result(interp, result);
  } else {
    // Return first match
    for (size_t i = start; i < listLen; i++) {
      FeatherObj elem = ops->list.at(interp, list, i);
      // For -index, extract the element at the specified index within the sublist
      FeatherObj matchElem = elem;
      if (hasIndex) {
        FeatherObj sublist = ops->list.from(interp, elem);
        size_t sublistLen = ops->list.length(interp, sublist);
        if (searchIndex >= 0 && (size_t)searchIndex < sublistLen) {
          matchElem = ops->list.at(interp, sublist, (size_t)searchIndex);
        } else {
          // Index out of range - skip this element (doesn't match)
          continue;
        }
      }
      if (element_matches(ops, interp, matchElem, pattern, mode, nocase, negate)) {
        if (inlineResult) {
          ops->interp.set_result(interp, elem);
        } else {
          ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)i));
        }
        return TCL_OK;
      }
    }
    // Not found
    if (inlineResult) {
      ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    } else {
      ops->interp.set_result(interp, ops->integer.create(interp, -1));
    }
  }

  return TCL_OK;
}
