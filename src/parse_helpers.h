#ifndef FEATHER_PARSE_HELPERS_H
#define FEATHER_PARSE_HELPERS_H

#include "feather.h"
#include "charclass.h"

/**
 * Convenience macros and functions for byte-at-a-time parsing.
 * 
 * These helpers abstract away the ops->string.byte_at() calls and provide
 * common parsing patterns like skipping whitespace.
 * 
 * Usage pattern:
 *   size_t len = ops->string.byte_length(interp, str);
 *   size_t pos = 0;
 *   pos = feather_skip_whitespace(ops, interp, str, pos);
 *   int ch = FEATHER_BYTE_AT(ops, interp, str, pos);
 */

/* Get byte at position, -1 if past end */
#define FEATHER_BYTE_AT(ops, interp, obj, pos) \
    ((ops)->string.byte_at((interp), (obj), (pos)))

/* Check if at end of string */
#define FEATHER_AT_END(ops, interp, obj, pos) \
    ((ops)->string.byte_at((interp), (obj), (pos)) < 0)

/* Skip whitespace, return new position */
static inline size_t feather_skip_whitespace(const FeatherHostOps *ops,
                                              FeatherInterp interp,
                                              FeatherObj str, size_t pos) {
  int ch;
  while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 &&
         feather_is_whitespace(ch)) {
    pos++;
  }
  return pos;
}

/* Skip while character class matches, return new position */
static inline size_t feather_skip_while(const FeatherHostOps *ops,
                                         FeatherInterp interp,
                                         FeatherObj str, size_t pos,
                                         int (*predicate)(int)) {
  int ch;
  while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 && predicate(ch)) {
    pos++;
  }
  return pos;
}

/* Skip until character class matches (or end), return new position */
static inline size_t feather_skip_until(const FeatherHostOps *ops,
                                         FeatherInterp interp,
                                         FeatherObj str, size_t pos,
                                         int (*predicate)(int)) {
  int ch;
  while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 && !predicate(ch)) {
    pos++;
  }
  return pos;
}

/* Skip to command terminator (newline, semicolon, null, or end), return position */
static inline size_t feather_skip_to_terminator(const FeatherHostOps *ops,
                                                 FeatherInterp interp,
                                                 FeatherObj str, size_t pos) {
  int ch;
  while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 &&
         !feather_is_command_terminator(ch)) {
    pos++;
  }
  return pos;
}

/* Skip to word terminator (whitespace or command terminator), return position */
static inline size_t feather_skip_to_word_end(const FeatherHostOps *ops,
                                               FeatherInterp interp,
                                               FeatherObj str, size_t pos) {
  int ch;
  while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 &&
         !feather_is_word_terminator(ch)) {
    pos++;
  }
  return pos;
}

/* Extract a varname starting at pos, return end position.
 * A varname consists of alphanumeric characters and underscores. */
static inline size_t feather_scan_varname(const FeatherHostOps *ops,
                                           FeatherInterp interp,
                                           FeatherObj str, size_t pos) {
  return feather_skip_while(ops, interp, str, pos, feather_is_varname_char);
}

#endif
