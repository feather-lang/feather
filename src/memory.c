#include "feather.h"

/**
 * feather_strlen counts bytes in a null-terminated C string, excluding the null.
 *
 * This is equivalent to strlen but avoids stdlib dependency.
 */
size_t feather_strlen(const char *s) {
  size_t len = 0;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

/**
 * feather_str_eq compares a length-delimited string against a null-terminated literal.
 *
 * Returns 1 if equal, 0 otherwise.
 */
int feather_str_eq(const char *s, size_t len, const char *lit) {
  size_t llen = feather_strlen(lit);
  if (len != llen) return 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) return 0;
  }
  return 1;
}
