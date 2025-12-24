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
