#include "charclass.h"
#include <stddef.h>

int feather_is_octal_digit(char c) {
  return c >= '0' && c <= '7';
}

int feather_is_digit(char c) {
  return c >= '0' && c <= '9';
}

int feather_char_tolower(int c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  return c;
}

int feather_is_args_param(const char *s, size_t len) {
  return len == 4 && s[0] == 'a' && s[1] == 'r' && s[2] == 'g' && s[3] == 's';
}
