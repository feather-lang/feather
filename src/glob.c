#include "feather.h"
#include "internal.h"

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

int feather_glob_match(const char *pattern, size_t pattern_len,
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

/**
 * Helper: check if character c is in the character class using byte-at-a-time access.
 * class_start and class_end are positions in the pattern object (after '[', before ']').
 * Handles ranges like a-z and negation with ^ or !.
 */
static int match_char_class_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj pattern, size_t class_start, size_t class_end,
                                int c) {
  size_t class_len = class_end - class_start;
  if (class_len == 0) {
    return 0;
  }

  int negated = 0;
  size_t pos = class_start;

  // Check for negation
  int first = ops->string.byte_at(interp, pattern, pos);
  if (first == '^' || first == '!') {
    negated = 1;
    pos++;
  }

  int matched = 0;
  while (pos < class_end) {
    int curr = ops->string.byte_at(interp, pattern, pos);
    // Check for range (e.g., a-z)
    if (pos + 2 < class_end) {
      int dash = ops->string.byte_at(interp, pattern, pos + 1);
      if (dash == '-') {
        int range_end = ops->string.byte_at(interp, pattern, pos + 2);
        if (c >= curr && c <= range_end) {
          matched = 1;
        }
        pos += 3;
        continue;
      }
    }
    if (curr == c) {
      matched = 1;
    }
    pos++;
  }

  return negated ? !matched : matched;
}

int feather_obj_glob_match(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj pattern, FeatherObj string) {
  size_t pattern_len = ops->string.byte_length(interp, pattern);
  size_t string_len = ops->string.byte_length(interp, string);

  size_t p = 0; // pattern position
  size_t s = 0; // string position

  // For backtracking on * matches
  size_t star_p = (size_t)-1; // pattern position after last *
  size_t star_s = (size_t)-1; // string position at last * match

  while (s < string_len) {
    if (p < pattern_len) {
      int pc = ops->string.byte_at(interp, pattern, p);

      // Handle escape
      if (pc == '\\' && p + 1 < pattern_len) {
        p++;
        pc = ops->string.byte_at(interp, pattern, p);
        int sc = ops->string.byte_at(interp, string, s);
        if (sc == pc) {
          p++;
          s++;
          continue;
        }
        // Escaped char doesn't match - try backtracking
        goto backtrack_obj;
      }

      // Handle *
      if (pc == '*') {
        // Skip consecutive *s
        while (p < pattern_len && ops->string.byte_at(interp, pattern, p) == '*') {
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
        while (class_end < pattern_len && ops->string.byte_at(interp, pattern, class_end) != ']') {
          class_end++;
        }
        if (class_end >= pattern_len) {
          // No closing ], treat [ as literal
          int sc = ops->string.byte_at(interp, string, s);
          if (sc == '[') {
            p++;
            s++;
            continue;
          }
          goto backtrack_obj;
        }
        // Match character against class
        int sc = ops->string.byte_at(interp, string, s);
        if (match_char_class_obj(ops, interp, pattern, class_start, class_end, sc)) {
          p = class_end + 1;
          s++;
          continue;
        }
        goto backtrack_obj;
      }

      // Literal character match
      int sc = ops->string.byte_at(interp, string, s);
      if (pc == sc) {
        p++;
        s++;
        continue;
      }
    }

  backtrack_obj:
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
  while (p < pattern_len && ops->string.byte_at(interp, pattern, p) == '*') {
    p++;
  }

  return p >= pattern_len;
}
