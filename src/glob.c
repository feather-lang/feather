#include "tclc.h"

/**
 * Helper: check if character c is in the character class [start, end).
 * Handles ranges like a-z and negation with ^ or !.
 */
static int match_char_class(const char *class_start, size_t class_len, char c) {
  if (class_len == 0) {
    return 0;
  }

  int negated = 0;
  size_t pos = 0;

  // Check for negation
  if (class_start[0] == '^' || class_start[0] == '!') {
    negated = 1;
    pos = 1;
  }

  int matched = 0;
  while (pos < class_len) {
    // Check for range (e.g., a-z)
    if (pos + 2 < class_len && class_start[pos + 1] == '-') {
      char range_start = class_start[pos];
      char range_end = class_start[pos + 2];
      if (c >= range_start && c <= range_end) {
        matched = 1;
      }
      pos += 3;
    } else {
      if (class_start[pos] == c) {
        matched = 1;
      }
      pos++;
    }
  }

  return negated ? !matched : matched;
}

int tcl_glob_match(const char *pattern, size_t pattern_len,
                   const char *string, size_t string_len) {
  size_t p = 0; // pattern position
  size_t s = 0; // string position

  // For backtracking on * matches
  size_t star_p = (size_t)-1; // pattern position after last *
  size_t star_s = (size_t)-1; // string position at last * match

  while (s < string_len) {
    if (p < pattern_len) {
      char pc = pattern[p];

      // Handle escape
      if (pc == '\\' && p + 1 < pattern_len) {
        p++;
        pc = pattern[p];
        if (string[s] == pc) {
          p++;
          s++;
          continue;
        }
        // Escaped char doesn't match - try backtracking
        goto backtrack;
      }

      // Handle *
      if (pc == '*') {
        // Skip consecutive *s
        while (p < pattern_len && pattern[p] == '*') {
          p++;
        }
        // If * is at end, it matches everything
        if (p >= pattern_len) {
          return 1;
        }
        // Mark position for backtracking
        star_p = p;
        star_s = s;
        continue;
      }

      // Handle ?
      if (pc == '?') {
        p++;
        s++;
        continue;
      }

      // Handle [...]
      if (pc == '[') {
        // Find closing ]
        size_t class_start = p + 1;
        size_t class_end = class_start;
        while (class_end < pattern_len && pattern[class_end] != ']') {
          class_end++;
        }
        if (class_end >= pattern_len) {
          // No closing ], treat [ as literal
          if (string[s] == '[') {
            p++;
            s++;
            continue;
          }
          goto backtrack;
        }
        // Match character against class
        if (match_char_class(pattern + class_start, class_end - class_start,
                             string[s])) {
          p = class_end + 1;
          s++;
          continue;
        }
        goto backtrack;
      }

      // Literal character match
      if (pc == string[s]) {
        p++;
        s++;
        continue;
      }
    }

  backtrack:
    // No match at current position - try backtracking
    if (star_p != (size_t)-1) {
      // Advance the string position where * matched
      star_s++;
      s = star_s;
      p = star_p;
      continue;
    }

    // No * to backtrack to - match failed
    return 0;
  }

  // String exhausted - check if remaining pattern is all *
  while (p < pattern_len && pattern[p] == '*') {
    p++;
  }

  return p >= pattern_len;
}
