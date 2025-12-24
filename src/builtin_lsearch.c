#include "tclc.h"
#include "internal.h"

// Match mode
typedef enum {
  MATCH_EXACT,
  MATCH_GLOB,
  MATCH_REGEXP
} MatchMode;

// Helper for case-insensitive compare
static int char_tolower(int c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  return c;
}

static int compare_nocase(const TclHostOps *ops, TclInterp interp,
                          TclObj a, TclObj b) {
  size_t lenA, lenB;
  const char *strA = ops->string.get(interp, a, &lenA);
  const char *strB = ops->string.get(interp, b, &lenB);

  if (lenA != lenB) return 0;
  for (size_t i = 0; i < lenA; i++) {
    int ca = char_tolower((unsigned char)strA[i]);
    int cb = char_tolower((unsigned char)strB[i]);
    if (ca != cb) return 0;
  }
  return 1;
}

// Glob match with nocase support
static int glob_match_nocase(const char *pattern, size_t plen,
                             const char *string, size_t slen) {
  // Simple case-insensitive glob matching
  // For a complete implementation, we'd need to lowercase both
  // For now, just use the existing tcl_glob_match for case-sensitive
  // This is a simplified version
  size_t p = 0, s = 0;
  size_t starP = (size_t)-1, starS = 0;

  while (s < slen) {
    if (p < plen && (pattern[p] == '?' ||
                     char_tolower((unsigned char)pattern[p]) ==
                     char_tolower((unsigned char)string[s]))) {
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

// Check if element matches pattern
static int element_matches(const TclHostOps *ops, TclInterp interp,
                           TclObj element, TclObj pattern,
                           MatchMode mode, int nocase, int negate) {
  int matches = 0;

  switch (mode) {
    case MATCH_EXACT:
      if (nocase) {
        matches = compare_nocase(ops, interp, element, pattern);
      } else {
        matches = (ops->string.compare(interp, element, pattern) == 0);
      }
      break;

    case MATCH_GLOB: {
      size_t elen, plen;
      const char *estr = ops->string.get(interp, element, &elen);
      const char *pstr = ops->string.get(interp, pattern, &plen);
      if (nocase) {
        matches = glob_match_nocase(pstr, plen, estr, elen);
      } else {
        matches = tcl_glob_match(pstr, plen, estr, elen);
      }
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

TclResult tcl_builtin_lsearch(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    TclObj msg = ops->string.intern(interp,
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

  // Process options
  while (ops->list.length(interp, args) > 2) {
    TclObj arg = ops->list.shift(interp, args);
    size_t len;
    const char *str = ops->string.get(interp, arg, &len);

    if (str_eq(str, len, "-exact")) {
      mode = MATCH_EXACT;
    } else if (str_eq(str, len, "-glob")) {
      mode = MATCH_GLOB;
    } else if (str_eq(str, len, "-regexp")) {
      mode = MATCH_REGEXP;
    } else if (str_eq(str, len, "-nocase")) {
      nocase = 1;
    } else if (str_eq(str, len, "-all")) {
      all = 1;
    } else if (str_eq(str, len, "-inline")) {
      inlineResult = 1;
    } else if (str_eq(str, len, "-not")) {
      negate = 1;
    } else {
      TclObj msg = ops->string.intern(interp, "bad option \"", 12);
      msg = ops->string.concat(interp, msg, arg);
      TclObj suffix = ops->string.intern(interp, "\"", 1);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  TclObj listObj = ops->list.shift(interp, args);
  TclObj pattern = ops->list.shift(interp, args);

  // Convert to list
  TclObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  if (all) {
    // Return all matching indices/elements
    TclObj result = ops->list.create(interp);
    for (size_t i = 0; i < listLen; i++) {
      TclObj elem = ops->list.at(interp, list, i);
      if (element_matches(ops, interp, elem, pattern, mode, nocase, negate)) {
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
    for (size_t i = 0; i < listLen; i++) {
      TclObj elem = ops->list.at(interp, list, i);
      if (element_matches(ops, interp, elem, pattern, mode, nocase, negate)) {
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
