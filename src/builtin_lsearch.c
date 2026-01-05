#include "feather.h"
#include "internal.h"
#include "charclass.h"
#include "index_parse.h"

// Match mode
typedef enum {
  MATCH_EXACT,
  MATCH_GLOB,
  MATCH_REGEXP
} MatchMode;

// Compare mode for sorted searches
typedef enum {
  COMPARE_ASCII,
  COMPARE_INTEGER,
  COMPARE_REAL,
  COMPARE_DICTIONARY
} CompareMode;

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

// Case-insensitive comparison that returns -1, 0, or 1 (for sorted comparison)
static int lsearch_compare_nocase_cmp(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj a, FeatherObj b) {
  size_t lenA = ops->string.byte_length(interp, a);
  size_t lenB = ops->string.byte_length(interp, b);

  size_t minLen = lenA < lenB ? lenA : lenB;
  for (size_t i = 0; i < minLen; i++) {
    int ca = feather_char_tolower((unsigned char)ops->string.byte_at(interp, a, i));
    int cb = feather_char_tolower((unsigned char)ops->string.byte_at(interp, b, i));
    if (ca != cb) return ca - cb;
  }
  if (lenA < lenB) return -1;
  if (lenA > lenB) return 1;
  return 0;
}

// Dictionary comparison for sorted searches
static int lsearch_compare_dictionary(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj a, FeatherObj b) {
  size_t lenA = ops->string.byte_length(interp, a);
  size_t lenB = ops->string.byte_length(interp, b);
  size_t iA = 0, iB = 0;
  int caseDiff = 0;

  while (iA < lenA && iB < lenB) {
    unsigned char cA = (unsigned char)ops->string.byte_at(interp, a, iA);
    unsigned char cB = (unsigned char)ops->string.byte_at(interp, b, iB);

    if (feather_is_digit(cA) && feather_is_digit(cB)) {
      size_t zerosA = 0, zerosB = 0;
      while (iA < lenA && ops->string.byte_at(interp, a, iA) == '0') {
        zerosA++;
        iA++;
      }
      while (iB < lenB && ops->string.byte_at(interp, b, iB) == '0') {
        zerosB++;
        iB++;
      }

      int64_t numA = 0, numB = 0;
      while (iA < lenA && feather_is_digit((unsigned char)ops->string.byte_at(interp, a, iA))) {
        numA = numA * 10 + (ops->string.byte_at(interp, a, iA) - '0');
        iA++;
      }
      while (iB < lenB && feather_is_digit((unsigned char)ops->string.byte_at(interp, b, iB))) {
        numB = numB * 10 + (ops->string.byte_at(interp, b, iB) - '0');
        iB++;
      }

      if (numA != numB) {
        return (numA < numB) ? -1 : 1;
      }
      if (zerosA != zerosB) {
        return (zerosA < zerosB) ? -1 : 1;
      }
    } else {
      int lowerA = feather_char_tolower(cA);
      int lowerB = feather_char_tolower(cB);

      if (lowerA != lowerB) {
        return lowerA - lowerB;
      }

      if (caseDiff == 0 && cA != cB) {
        caseDiff = (int)cA - (int)cB;
      }

      iA++;
      iB++;
    }
  }

  if (iA < lenA) return 1;
  if (iB < lenB) return -1;
  return caseDiff;
}

// Compare two elements for sorted search - returns -1, 0, or 1
static int sorted_compare(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj a, FeatherObj b,
                          CompareMode mode, int nocase, int decreasing) {
  int result = 0;

  switch (mode) {
    case COMPARE_ASCII:
      if (nocase) {
        result = lsearch_compare_nocase_cmp(ops, interp, a, b);
      } else {
        result = ops->string.compare(interp, a, b);
      }
      break;

    case COMPARE_INTEGER: {
      int64_t va, vb;
      ops->integer.get(interp, a, &va);
      ops->integer.get(interp, b, &vb);
      if (va < vb) result = -1;
      else if (va > vb) result = 1;
      else result = 0;
      break;
    }

    case COMPARE_REAL: {
      double va, vb;
      ops->dbl.get(interp, a, &va);
      ops->dbl.get(interp, b, &vb);
      if (va < vb) result = -1;
      else if (va > vb) result = 1;
      else result = 0;
      break;
    }

    case COMPARE_DICTIONARY:
      result = lsearch_compare_dictionary(ops, interp, a, b);
      break;
  }

  if (decreasing) result = -result;
  return result;
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
      if (ops->string.regex_match(interp, pattern, element, nocase, &result, NULL, NULL) == TCL_OK) {
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
  CompareMode compareMode = COMPARE_ASCII;  // Default for sorted
  int nocase = 0;
  int all = 0;
  int inlineResult = 0;
  int negate = 0;
  int sorted = 0;
  int bisect = 0;
  int decreasing = 0;
  int subindices = 0;
  int64_t startIndex = 0;
  FeatherObj startIndexObj = 0;  // Raw start index for deferred parsing
  int hasIndex = 0;
  FeatherObj searchIndexObjs[16];  // Raw index objects for deferred parsing
  size_t numSearchIndices = 0;
  int64_t strideLength = 1;  // Default is 1 (no stride)

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
      startIndexObj = ops->list.shift(interp, args);
    } else if (feather_obj_eq_literal(ops, interp, arg, "-index")) {
      // -index requires an argument
      if (ops->list.length(interp, args) < 3) {
        FeatherObj msg = ops->string.intern(interp, "\"-index\" option must be followed by list index", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj indexArg = ops->list.shift(interp, args);
      // Try as list of indices first
      FeatherObj indexList = ops->list.from(interp, indexArg);
      size_t indexListLen = ops->list.length(interp, indexList);
      if (indexListLen > 1) {
        // It's a list of indices
        if (indexListLen > 16) {
          FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
          msg = ops->string.concat(interp, msg, indexArg);
          FeatherObj suffix = ops->string.intern(interp, "\": must be integer?[+-]integer? or end?[+-]integer?", 51);
          msg = ops->string.concat(interp, msg, suffix);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        for (size_t j = 0; j < indexListLen; j++) {
          searchIndexObjs[j] = ops->list.at(interp, indexList, j);
        }
        numSearchIndices = indexListLen;
      } else {
        // Single index (store as-is for end-N support)
        searchIndexObjs[0] = indexArg;
        numSearchIndices = 1;
      }
      hasIndex = 1;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-stride")) {
      // -stride requires an argument
      if (ops->list.length(interp, args) < 3) {
        FeatherObj msg = ops->string.intern(interp,
          "\"-stride\" option must be followed by stride length", 50);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj strideArg = ops->list.shift(interp, args);
      if (ops->integer.get(interp, strideArg, &strideLength) != TCL_OK) {
        FeatherObj msg = ops->string.intern(interp, "bad stride length \"", 19);
        msg = ops->string.concat(interp, msg, strideArg);
        FeatherObj suffix = ops->string.intern(interp, "\"", 1);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      if (strideLength < 1) {
        FeatherObj msg = ops->string.intern(interp,
          "stride length must be at least 1", 32);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    } else if (feather_obj_eq_literal(ops, interp, arg, "-sorted")) {
      sorted = 1;
      mode = MATCH_EXACT;  // -sorted implies -exact
    } else if (feather_obj_eq_literal(ops, interp, arg, "-bisect")) {
      bisect = 1;
      sorted = 1;  // -bisect implies -sorted
      mode = MATCH_EXACT;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-dictionary")) {
      compareMode = COMPARE_DICTIONARY;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-ascii")) {
      compareMode = COMPARE_ASCII;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-integer")) {
      compareMode = COMPARE_INTEGER;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-real")) {
      compareMode = COMPARE_REAL;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-increasing")) {
      decreasing = 0;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-decreasing")) {
      decreasing = 1;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-subindices")) {
      subindices = 1;
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
  size_t stride = (size_t)strideLength;

  // Validate stride constraint
  if (stride > 1 && listLen % stride != 0) {
    FeatherObj msg = ops->string.intern(interp,
      "list size must be a multiple of the stride length", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Validate -subindices requires -index
  if (subindices && !hasIndex) {
    FeatherObj msg = ops->string.intern(interp,
      "-subindices cannot be used without -index option", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Parse -start index now that we have the list length
  if (startIndexObj) {
    if (feather_parse_index(ops, interp, startIndexObj, listLen, &startIndex) != TCL_OK) {
      return TCL_ERROR;
    }
    // Clamp negative to 0
    if (startIndex < 0) startIndex = 0;
  }

  // Clamp startIndex to list length (returns -1 or empty for out of range)
  size_t start = (size_t)startIndex;
  if (start > listLen) start = listLen;

  // Resolved indices array for subindices output
  int64_t resolvedIndices[16];

  // Helper macro to extract element to compare (supports nested indices with end-N)
  // Resolves searchIndexObjs at match time against actual element lengths
  #define GET_MATCH_ELEM(i, elem, matchElem) do { \
    if (stride > 1 && hasIndex) { \
      /* First index selects within stride group */ \
      FeatherObj _firstIdxObj = searchIndexObjs[0]; \
      int64_t _firstIdx; \
      if (feather_parse_index(ops, interp, _firstIdxObj, stride, &_firstIdx) != TCL_OK) { \
        matchElem = 0; \
      } else { \
        resolvedIndices[0] = _firstIdx; \
        if (_firstIdx < 0 || (size_t)_firstIdx >= stride) { matchElem = 0; } \
        else { \
          matchElem = ops->list.at(interp, list, (i) + (size_t)_firstIdx); \
          /* Traverse remaining indices for nested access */ \
          for (size_t _k = 1; _k < numSearchIndices && matchElem; _k++) { \
            FeatherObj _sub = ops->list.from(interp, matchElem); \
            size_t _subLen = ops->list.length(interp, _sub); \
            int64_t _idx; \
            if (feather_parse_index(ops, interp, searchIndexObjs[_k], _subLen, &_idx) != TCL_OK) { \
              matchElem = 0; break; \
            } \
            resolvedIndices[_k] = _idx; \
            if (_idx >= 0 && (size_t)_idx < _subLen) { \
              matchElem = ops->list.at(interp, _sub, (size_t)_idx); \
            } else { matchElem = 0; } \
          } \
        } \
      } \
    } else if (stride > 1) { \
      matchElem = elem; \
    } else if (hasIndex) { \
      matchElem = elem; \
      /* Traverse all indices for nested access */ \
      for (size_t _k = 0; _k < numSearchIndices && matchElem; _k++) { \
        FeatherObj _sub = ops->list.from(interp, matchElem); \
        size_t _subLen = ops->list.length(interp, _sub); \
        int64_t _idx; \
        if (feather_parse_index(ops, interp, searchIndexObjs[_k], _subLen, &_idx) != TCL_OK) { \
          matchElem = 0; break; \
        } \
        resolvedIndices[_k] = _idx; \
        if (_idx >= 0 && (size_t)_idx < _subLen) { \
          matchElem = ops->list.at(interp, _sub, (size_t)_idx); \
        } else { matchElem = 0; } \
      } \
    } else { \
      matchElem = elem; \
    } \
  } while(0)

  // Use binary search for sorted lists
  if (sorted && !negate) {
    // Binary search to find an exact match or insertion point
    size_t numGroups = listLen / stride;
    size_t lo = 0, hi = numGroups;
    size_t foundIdx = (size_t)-1;

    while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2;
      size_t realIdx = mid * stride;
      FeatherObj elem = ops->list.at(interp, list, realIdx);
      FeatherObj matchElem;
      GET_MATCH_ELEM(realIdx, elem, matchElem);

      int cmp = sorted_compare(ops, interp, matchElem, pattern, compareMode, nocase, decreasing);

      if (cmp < 0) {
        lo = mid + 1;
      } else if (cmp > 0) {
        hi = mid;
      } else {
        // Found a match
        foundIdx = mid;
        break;
      }
    }

    if (bisect) {
      // Return insertion point: last element <= pattern (increasing) or >= pattern (decreasing)
      // For bisect, we want the largest index where element <= pattern
      if (numGroups == 0) {
        ops->interp.set_result(interp, ops->integer.create(interp, -1));
        return TCL_OK;
      }

      // Reset and do bisect search
      lo = 0;
      hi = numGroups;
      size_t bisectIdx = (size_t)-1;

      while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        size_t realIdx = mid * stride;
        FeatherObj elem = ops->list.at(interp, list, realIdx);
        FeatherObj matchElem;
        GET_MATCH_ELEM(realIdx, elem, matchElem);

        int cmp = sorted_compare(ops, interp, matchElem, pattern, compareMode, nocase, decreasing);

        if (cmp <= 0) {
          bisectIdx = mid;
          lo = mid + 1;
        } else {
          hi = mid;
        }
      }

      if (bisectIdx == (size_t)-1) {
        ops->interp.set_result(interp, ops->integer.create(interp, -1));
      } else {
        ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)(bisectIdx * stride)));
      }
      return TCL_OK;
    }

    if (foundIdx == (size_t)-1) {
      // Not found
      if (all) {
        ops->interp.set_result(interp, ops->list.create(interp));
      } else if (inlineResult) {
        ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      } else if (subindices) {
        FeatherObj pair = ops->list.create(interp);
        pair = ops->list.push(interp, pair, ops->integer.create(interp, -1));
        for (size_t si = 0; si < numSearchIndices; si++) {
          pair = ops->list.push(interp, pair, ops->integer.create(interp, resolvedIndices[si]));
        }
        ops->interp.set_result(interp, pair);
      } else {
        ops->interp.set_result(interp, ops->integer.create(interp, -1));
      }
      return TCL_OK;
    }

    // Found at foundIdx - handle -all to find all duplicates
    if (all) {
      // Find range of matching elements
      size_t first = foundIdx, last = foundIdx;
      // Search backward for first match
      while (first > 0) {
        size_t prevIdx = (first - 1) * stride;
        FeatherObj elem = ops->list.at(interp, list, prevIdx);
        FeatherObj matchElem;
        GET_MATCH_ELEM(prevIdx, elem, matchElem);
        if (sorted_compare(ops, interp, matchElem, pattern, compareMode, nocase, decreasing) != 0) break;
        first--;
      }
      // Search forward for last match
      while (last < numGroups - 1) {
        size_t nextIdx = (last + 1) * stride;
        FeatherObj elem = ops->list.at(interp, list, nextIdx);
        FeatherObj matchElem;
        GET_MATCH_ELEM(nextIdx, elem, matchElem);
        if (sorted_compare(ops, interp, matchElem, pattern, compareMode, nocase, decreasing) != 0) break;
        last++;
      }

      FeatherObj result = ops->list.create(interp);
      for (size_t i = first; i <= last; i++) {
        size_t realIdx = i * stride;
        if (inlineResult) {
          if (stride > 1) {
            for (size_t j = 0; j < stride; j++) {
              result = ops->list.push(interp, result, ops->list.at(interp, list, realIdx + j));
            }
          } else if (subindices) {
            // With -subindices -inline, return the matched element value
            FeatherObj subElem = ops->list.at(interp, list, realIdx);
            FeatherObj subMatchElem;
            GET_MATCH_ELEM(realIdx, subElem, subMatchElem);
            result = ops->list.push(interp, result, subMatchElem);
          } else {
            result = ops->list.push(interp, result, ops->list.at(interp, list, realIdx));
          }
        } else if (subindices) {
          FeatherObj pair = ops->list.create(interp);
          pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)realIdx));
          for (size_t si = 0; si < numSearchIndices; si++) {
            pair = ops->list.push(interp, pair, ops->integer.create(interp, resolvedIndices[si]));
          }
          result = ops->list.push(interp, result, pair);
        } else {
          result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)realIdx));
        }
      }
      ops->interp.set_result(interp, result);
    } else {
      // Return first match
      size_t realIdx = foundIdx * stride;
      if (inlineResult) {
        if (stride > 1) {
          FeatherObj group = ops->list.create(interp);
          for (size_t j = 0; j < stride; j++) {
            group = ops->list.push(interp, group, ops->list.at(interp, list, realIdx + j));
          }
          ops->interp.set_result(interp, group);
        } else if (subindices) {
          // With -subindices -inline, return the matched element value
          FeatherObj singleElem = ops->list.at(interp, list, realIdx);
          FeatherObj singleMatchElem;
          GET_MATCH_ELEM(realIdx, singleElem, singleMatchElem);
          ops->interp.set_result(interp, singleMatchElem);
        } else {
          ops->interp.set_result(interp, ops->list.at(interp, list, realIdx));
        }
      } else if (subindices) {
        FeatherObj pair = ops->list.create(interp);
        pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)realIdx));
        for (size_t si = 0; si < numSearchIndices; si++) {
          pair = ops->list.push(interp, pair, ops->integer.create(interp, resolvedIndices[si]));
        }
        ops->interp.set_result(interp, pair);
      } else {
        ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)realIdx));
      }
    }
    return TCL_OK;
  }

  // Linear search (used for unsorted lists or when -not is specified with -sorted)
  if (all) {
    // Return all matching indices/elements
    FeatherObj result = ops->list.create(interp);
    for (size_t i = start; i < listLen; i += stride) {
      FeatherObj elem = ops->list.at(interp, list, i);
      FeatherObj matchElem;
      GET_MATCH_ELEM(i, elem, matchElem);
      if (!matchElem) continue;  // Index out of range

      int matches;
      if (sorted) {
        // -sorted with -not uses linear search but sorted comparison
        matches = (sorted_compare(ops, interp, matchElem, pattern, compareMode, nocase, decreasing) == 0);
        if (negate) matches = !matches;
      } else {
        matches = element_matches(ops, interp, matchElem, pattern, mode, nocase, negate);
      }

      if (matches) {
        if (inlineResult) {
          if (stride > 1) {
            // Return all elements in the stride group
            for (size_t j = 0; j < stride; j++) {
              result = ops->list.push(interp, result, ops->list.at(interp, list, i + j));
            }
          } else if (subindices) {
            // With -subindices -inline, return the matched element value, not the container
            result = ops->list.push(interp, result, matchElem);
          } else {
            result = ops->list.push(interp, result, elem);
          }
        } else if (subindices) {
          FeatherObj pair = ops->list.create(interp);
          pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)i));
          for (size_t si = 0; si < numSearchIndices; si++) {
            pair = ops->list.push(interp, pair, ops->integer.create(interp, resolvedIndices[si]));
          }
          result = ops->list.push(interp, result, pair);
        } else {
          result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)i));
        }
      }
    }
    ops->interp.set_result(interp, result);
  } else {
    // Return first match
    for (size_t i = start; i < listLen; i += stride) {
      FeatherObj elem = ops->list.at(interp, list, i);
      FeatherObj matchElem;
      GET_MATCH_ELEM(i, elem, matchElem);
      if (!matchElem) continue;  // Index out of range

      int matches;
      if (sorted) {
        // -sorted with -not uses linear search but sorted comparison
        matches = (sorted_compare(ops, interp, matchElem, pattern, compareMode, nocase, decreasing) == 0);
        if (negate) matches = !matches;
      } else {
        matches = element_matches(ops, interp, matchElem, pattern, mode, nocase, negate);
      }

      if (matches) {
        if (inlineResult) {
          if (stride > 1) {
            // Return all elements in the stride group
            FeatherObj group = ops->list.create(interp);
            for (size_t j = 0; j < stride; j++) {
              group = ops->list.push(interp, group, ops->list.at(interp, list, i + j));
            }
            ops->interp.set_result(interp, group);
          } else if (subindices) {
            // With -subindices -inline, return the matched element value, not the container
            ops->interp.set_result(interp, matchElem);
          } else {
            ops->interp.set_result(interp, elem);
          }
        } else if (subindices) {
          FeatherObj pair = ops->list.create(interp);
          pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)i));
          for (size_t si = 0; si < numSearchIndices; si++) {
            pair = ops->list.push(interp, pair, ops->integer.create(interp, resolvedIndices[si]));
          }
          ops->interp.set_result(interp, pair);
        } else {
          ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)i));
        }
        return TCL_OK;
      }
    }
    // Not found
    if (inlineResult) {
      ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    } else if (subindices) {
      FeatherObj pair = ops->list.create(interp);
      pair = ops->list.push(interp, pair, ops->integer.create(interp, -1));
      for (size_t si = 0; si < numSearchIndices; si++) {
        pair = ops->list.push(interp, pair, ops->integer.create(interp, resolvedIndices[si]));
      }
      ops->interp.set_result(interp, pair);
    } else {
      ops->interp.set_result(interp, ops->integer.create(interp, -1));
    }
  }

  return TCL_OK;
}

void feather_register_lsearch_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "See if a list contains a particular element",
    "Searches the elements of list to see if one of them matches pattern. "
    "If so, the command returns the index of the first matching element "
    "(unless the options -all or -inline are specified). If not, the command "
    "returns -1 or (if options -all or -inline are specified) the empty string.\n\n"
    "The option arguments indicate how the elements of the list are to be "
    "matched against pattern. Options are organized into categories:\n\n"
    "Matching style options (-exact, -glob, -regexp, -sorted) control how "
    "pattern matching is performed. The default is -glob. If more than one "
    "matching style is specified, the last one takes precedence.\n\n"
    "General modifier options (-all, -inline, -not, -start) may be used with "
    "any matching style to modify the search behavior.\n\n"
    "Contents description options (-ascii, -dictionary, -integer, -nocase, -real) "
    "describe how to interpret list elements. They are meaningful with -exact and "
    "-sorted. The default is -ascii.\n\n"
    "Sorted list options (-decreasing, -increasing, -bisect) are only meaningful "
    "with -sorted to specify sort order. The default is -increasing.\n\n"
    "Nested list options (-stride, -index, -subindices) are used to search lists "
    "of lists and may be combined with any other options.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Matching Style Options
  e = feather_usage_flag(ops, interp, "-exact", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Pattern is a literal string that is compared for exact equality against "
    "each list element");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-glob", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Pattern is a glob-style pattern which is matched against each list element "
    "using the same rules as the string match command");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-regexp", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Pattern is treated as a regular expression and matched against each list element");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-sorted", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are in sorted order. If this option is specified, lsearch "
    "will use a more efficient binary search algorithm (O(log n) instead of O(n)). "
    "If no other options are specified, list is assumed to be sorted in increasing "
    "order and to contain ASCII strings. This option is mutually exclusive with "
    "-glob and -regexp, and is treated exactly like -exact when either -all or -not "
    "are specified");
  spec = feather_usage_add(ops, interp, spec, e);

  // General Modifier Options
  e = feather_usage_flag(ops, interp, "-all", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Changes the result to be the list of all matching indices (or all matching "
    "values if -inline is specified as well). If indices are returned, the indices "
    "will be in ascending numeric order. If values are returned, the order of the "
    "values will be the order of those values within the input list");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-inline", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The matching value is returned instead of its index (or an empty string if "
    "no value matches). If -all is also specified, then the result of the command "
    "is the list of all values that matched");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-not", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "This negates the sense of the match, returning the index of the first "
    "non-matching value in the list");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-start", NULL, "<index>");
  e = feather_usage_help(ops, interp, e,
    "The list is searched starting at position index. The interpretation of "
    "the index value is the same as for the command string index, supporting "
    "simple index arithmetic and indices relative to the end of the list");
  spec = feather_usage_add(ops, interp, spec, e);

  // Contents Description Options
  e = feather_usage_flag(ops, interp, "-ascii", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are to be examined as Unicode strings (the name is for "
    "backward-compatibility reasons)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-dictionary", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are to be compared using dictionary-style comparisons "
    "(see lsort for a fuller description). Note that this only makes a meaningful "
    "difference from the -ascii option when the -sorted option is given, because "
    "values are only dictionary-equal when exactly equal");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-integer", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are to be compared as integers");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-nocase", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Causes comparisons to be handled in a case-insensitive manner. Has no effect "
    "if combined with the -dictionary, -integer, or -real options");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-real", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are to be compared as floating-point values");
  spec = feather_usage_add(ops, interp, spec, e);

  // Sorted List Options
  e = feather_usage_flag(ops, interp, "-bisect", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "Inexact search when the list elements are in sorted order. For an increasing "
    "list the last index where the element is less than or equal to the pattern is "
    "returned. For a decreasing list the last index where the element is greater "
    "than or equal to the pattern is returned. If the pattern is before the first "
    "element or the list is empty, -1 is returned. This option implies -sorted and "
    "cannot be used with either -all or -not");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-decreasing", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are sorted in decreasing order. This option is only "
    "meaningful when used with -sorted");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-increasing", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "The list elements are sorted in increasing order. This option is only "
    "meaningful when used with -sorted");
  spec = feather_usage_add(ops, interp, spec, e);

  // Nested List Options
  e = feather_usage_flag(ops, interp, "-index", NULL, "<indexList>");
  e = feather_usage_help(ops, interp, e,
    "This option is designed for use when searching within nested lists. The indexList "
    "argument gives a path of indices (much as might be used with the lindex or lset "
    "commands) within each element to allow the location of the term being matched against");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-stride", NULL, "<strideLength>");
  e = feather_usage_help(ops, interp, e,
    "If this option is specified, the list is treated as consisting of groups of "
    "strideLength elements and the groups are searched by either their first element "
    "or, if the -index option is used, by the element within each group given by the "
    "first index passed to -index (which is then ignored by -index). The resulting "
    "index always points to the first element in a group. The list length must be an "
    "integer multiple of strideLength, which in turn must be at least 1. A strideLength "
    "of 1 is the default and indicates no grouping");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-subindices", NULL, NULL);
  e = feather_usage_help(ops, interp, e,
    "If this option is given, the index result from this command (or every index result "
    "when -all is also specified) will be a complete path (suitable for use with lindex "
    "or lset) within the overall list to the term found. This option has no effect unless "
    "the -index is also specified, and is just a convenience short-cut");
  spec = feather_usage_add(ops, interp, spec, e);

  // Positional arguments
  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to search");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<pattern>");
  e = feather_usage_help(ops, interp, e, "The pattern or value to search for");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "lsearch {a b c d e} c",
    "Basic searching - returns 2",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -all {a b c a b c} c",
    "Find all matches - returns {2 5}",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -inline {a20 b35 c47} b*",
    "Using lsearch to filter lists - returns b35",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -all -inline -not {a20 b35 c47} b*",
    "Negated filter - returns {a20 c47}",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -all -inline -not -exact {a b c a d e a f g a} a",
    "Set-like removal operation - returns {b c d e f g}",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -start 3 {a b c a b c} c",
    "Searching may start part-way through the list - returns 5",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -index 1 -all -inline {{abc abc} {abc bcd} {abc cde}} *bc*",
    "Search inside elements - returns {{abc abc} {abc bcd}}",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lsearch -stride 2 -index 1 -all -inline {abc abc abc bcd abc cde} *bc*",
    "Same thing for a flattened list - returns {abc abc abc bcd}",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // See Also section
  e = feather_usage_section(ops, interp, "See Also",
    "list, lappend, lindex, linsert, llength, lrange, lreplace, lreverse, lset, lsort, string");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lsearch", spec);
}
