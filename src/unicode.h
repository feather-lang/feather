#ifndef FEATHER_UNICODE_H
#define FEATHER_UNICODE_H

#include <stdint.h>
#include <stddef.h>

/**
 * Encode a Unicode codepoint as UTF-8.
 * Returns number of bytes written (1-4).
 * Invalid codepoints (> 0x10FFFF) produce U+FFFD replacement character.
 */
static inline size_t feather_utf8_encode(uint32_t codepoint, char *buf) {
  if (codepoint <= 0x7F) {
    // 1-byte sequence: 0xxxxxxx
    buf[0] = (char)codepoint;
    return 1;
  } else if (codepoint <= 0x7FF) {
    // 2-byte sequence: 110xxxxx 10xxxxxx
    buf[0] = (char)(0xC0 | (codepoint >> 6));
    buf[1] = (char)(0x80 | (codepoint & 0x3F));
    return 2;
  } else if (codepoint <= 0xFFFF) {
    // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
    buf[0] = (char)(0xE0 | (codepoint >> 12));
    buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (codepoint & 0x3F));
    return 3;
  } else if (codepoint <= 0x10FFFF) {
    // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    buf[0] = (char)(0xF0 | (codepoint >> 18));
    buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (codepoint & 0x3F));
    return 4;
  } else {
    // Invalid codepoint - return replacement character (U+FFFD)
    buf[0] = (char)0xEF;
    buf[1] = (char)0xBF;
    buf[2] = (char)0xBD;
    return 3;
  }
}

/**
 * Decode a UTF-8 codepoint from a byte buffer.
 * Returns codepoint value, or -1 on error.
 * Sets *bytes_read to number of bytes consumed (1-4).
 */
static inline int64_t feather_utf8_decode(const unsigned char *buf, size_t len, size_t *bytes_read) {
  if (len == 0) return -1;

  unsigned char byte0 = buf[0];

  // 1-byte sequence: 0xxxxxxx
  if ((byte0 & 0x80) == 0) {
    *bytes_read = 1;
    return (int64_t)byte0;
  }

  // 2-byte sequence: 110xxxxx 10xxxxxx
  if ((byte0 & 0xE0) == 0xC0) {
    if (len < 2) return -1;
    unsigned char byte1 = buf[1];
    if ((byte1 & 0xC0) != 0x80) return -1;
    *bytes_read = 2;
    return (int64_t)(((byte0 & 0x1F) << 6) | (byte1 & 0x3F));
  }

  // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
  if ((byte0 & 0xF0) == 0xE0) {
    if (len < 3) return -1;
    unsigned char byte1 = buf[1];
    unsigned char byte2 = buf[2];
    if ((byte1 & 0xC0) != 0x80 || (byte2 & 0xC0) != 0x80) return -1;
    *bytes_read = 3;
    return (int64_t)(((byte0 & 0x0F) << 12) | ((byte1 & 0x3F) << 6) | (byte2 & 0x3F));
  }

  // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if ((byte0 & 0xF8) == 0xF0) {
    if (len < 4) return -1;
    unsigned char byte1 = buf[1];
    unsigned char byte2 = buf[2];
    unsigned char byte3 = buf[3];
    if ((byte1 & 0xC0) != 0x80 || (byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80) return -1;
    *bytes_read = 4;
    return (int64_t)(((byte0 & 0x07) << 18) | ((byte1 & 0x3F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F));
  }

  return -1; // Invalid UTF-8
}

#endif
