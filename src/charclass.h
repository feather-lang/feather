#ifndef FEATHER_CHARCLASS_H
#define FEATHER_CHARCLASS_H

#include <stddef.h>

/*
 * Inline character classification functions for byte-at-a-time parsing.
 * All functions take int ch to handle -1 (EOF/out-of-bounds) gracefully.
 * The -1 case returns false/0 for all predicates.
 */

static inline int feather_is_whitespace(int ch) {
  return ch == ' ' || ch == '\t';
}

static inline int feather_is_newline(int ch) {
  return ch == '\n' || ch == '\r';
}

static inline int feather_is_command_terminator(int ch) {
  return ch == '\n' || ch == '\r' || ch == '\0' || ch == ';' || ch < 0;
}

static inline int feather_is_word_terminator(int ch) {
  return feather_is_whitespace(ch) || feather_is_command_terminator(ch);
}

static inline int feather_inline_is_digit(int ch) {
  return ch >= '0' && ch <= '9';
}

static inline int feather_is_hex_digit(int ch) {
  return feather_inline_is_digit(ch) ||
         (ch >= 'a' && ch <= 'f') ||
         (ch >= 'A' && ch <= 'F');
}

static inline int feather_is_octal(int ch) {
  return ch >= '0' && ch <= '7';
}

static inline int feather_is_varname_char(int ch) {
  return (ch >= 'a' && ch <= 'z') ||
         (ch >= 'A' && ch <= 'Z') ||
         (ch >= '0' && ch <= '9') ||
         ch == '_';
}

static inline int feather_is_alpha(int ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static inline int feather_hex_value(int ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
  if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
  return -1;
}

static inline int feather_inline_tolower(int ch) {
  if (ch >= 'A' && ch <= 'Z') return ch + 32;
  return ch;
}

/*
 * Non-inline functions (defined in charclass.c).
 * These are kept for backward compatibility with existing code.
 */
int feather_is_octal_digit(char c);
int feather_is_digit(char c);
int feather_char_tolower(int c);
int feather_is_args_param(const char *s, size_t len);

#endif
