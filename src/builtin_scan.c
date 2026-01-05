#include "feather.h"
#include "internal.h"
#include "charclass.h"
#include "unicode.h"

// Decode a UTF-8 codepoint from a string object starting at a given byte position
// Returns the codepoint value and sets *bytes_read to the number of bytes consumed
// Returns -1 on error
static int64_t decode_utf8_at_pos(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj str, size_t pos, size_t len, size_t *bytes_read) {
  if (pos >= len) return -1;

  int byte0 = ops->string.byte_at(interp, str, pos);
  if (byte0 < 0) return -1;

  // 1-byte sequence: 0xxxxxxx
  if ((byte0 & 0x80) == 0) {
    *bytes_read = 1;
    return (int64_t)byte0;
  }

  // 2-byte sequence: 110xxxxx 10xxxxxx
  if ((byte0 & 0xE0) == 0xC0) {
    if (pos + 1 >= len) return -1;
    int byte1 = ops->string.byte_at(interp, str, pos + 1);
    if ((byte1 & 0xC0) != 0x80) return -1;
    *bytes_read = 2;
    return (int64_t)(((byte0 & 0x1F) << 6) | (byte1 & 0x3F));
  }

  // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
  if ((byte0 & 0xF0) == 0xE0) {
    if (pos + 2 >= len) return -1;
    int byte1 = ops->string.byte_at(interp, str, pos + 1);
    int byte2 = ops->string.byte_at(interp, str, pos + 2);
    if ((byte1 & 0xC0) != 0x80 || (byte2 & 0xC0) != 0x80) return -1;
    *bytes_read = 3;
    return (int64_t)(((byte0 & 0x0F) << 12) | ((byte1 & 0x3F) << 6) | (byte2 & 0x3F));
  }

  // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if ((byte0 & 0xF8) == 0xF0) {
    if (pos + 3 >= len) return -1;
    int byte1 = ops->string.byte_at(interp, str, pos + 1);
    int byte2 = ops->string.byte_at(interp, str, pos + 2);
    int byte3 = ops->string.byte_at(interp, str, pos + 3);
    if ((byte1 & 0xC0) != 0x80 || (byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80) return -1;
    *bytes_read = 4;
    return (int64_t)(((byte0 & 0x07) << 18) | ((byte1 & 0x3F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F));
  }

  return -1; // Invalid UTF-8
}

static int is_binary_digit(int c) {
  return c == '0' || c == '1';
}

typedef struct {
  int suppress;
  int width;
  int position;
  int has_position;
  char specifier;
  char charset[256];
  int charset_negated;
  int charset_len;
  SizeModifier size_mod;
} ScanSpec;

// Parse format specifier using object-based byte access
static int parse_scan_spec_obj(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj fmtObj, size_t start, size_t fmtLen, ScanSpec *spec) {
  size_t pos = start;

  spec->suppress = 0;
  spec->width = 0;
  spec->position = -1;
  spec->has_position = 0;
  spec->specifier = 0;
  spec->charset_negated = 0;
  spec->charset_len = 0;
  spec->size_mod = SIZE_NONE;
  for (int i = 0; i < 256; i++) spec->charset[i] = 0;

  if (pos >= fmtLen) return -1;

  int ch = ops->string.byte_at(interp, fmtObj, pos);
  if (ch == '%') {
    spec->specifier = '%';
    return 1;
  }

  size_t posStart = pos;
  while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && feather_is_digit(ch)) {
    pos++;
  }
  if (pos > posStart && pos < fmtLen && ops->string.byte_at(interp, fmtObj, pos) == '$') {
    int idx = 0;
    for (size_t i = posStart; i < pos; i++) {
      idx = idx * 10 + (ops->string.byte_at(interp, fmtObj, i) - '0');
    }
    spec->has_position = 1;
    spec->position = idx;
    pos++;
  } else {
    pos = posStart;
  }

  ch = (pos < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos) : -1;
  if (ch == '*') {
    spec->suppress = 1;
    pos++;
  }

  while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && feather_is_digit(ch)) {
    spec->width = spec->width * 10 + (ch - '0');
    pos++;
  }

  // Parse size modifiers (ll, h, l, z, t, L, j, q)
  ch = (pos < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos) : -1;
  if (ch == 'l') {
    pos++;
    if (pos < fmtLen && ops->string.byte_at(interp, fmtObj, pos) == 'l') {
      spec->size_mod = SIZE_LL;
      pos++;
    } else {
      spec->size_mod = SIZE_L;
    }
  } else if (ch == 'h') {
    spec->size_mod = SIZE_H;
    pos++;
  } else if (ch == 'L') {
    spec->size_mod = SIZE_BIG_L;
    pos++;
  } else if (ch == 'j') {
    spec->size_mod = SIZE_J;
    pos++;
  } else if (ch == 'z') {
    spec->size_mod = SIZE_Z;
    pos++;
  } else if (ch == 't') {
    spec->size_mod = SIZE_T;
    pos++;
  } else if (ch == 'q') {
    spec->size_mod = SIZE_Q;
    pos++;
  }

  if (pos >= fmtLen) return -1;

  ch = ops->string.byte_at(interp, fmtObj, pos);
  if (ch == 'd' || ch == 'i' || ch == 'u' || ch == 'o' || ch == 'x' || ch == 'X' ||
      ch == 'b' || ch == 'c' || ch == 's' || ch == 'f' || ch == 'e' || ch == 'E' ||
      ch == 'g' || ch == 'G' || ch == 'n') {
    spec->specifier = (char)ch;
    pos++;
    return (int)(pos - start);
  }

  if (ch == '[') {
    pos++;
    if (pos >= fmtLen) return -1;

    ch = ops->string.byte_at(interp, fmtObj, pos);
    if (ch == '^') {
      spec->charset_negated = 1;
      pos++;
    }

    ch = (pos < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos) : -1;
    if (ch == ']') {
      spec->charset[(unsigned char)']'] = 1;
      spec->charset_len++;
      pos++;
    }

    while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && ch != ']') {
      int ch2 = (pos + 1 < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos + 1) : -1;
      int ch3 = (pos + 2 < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos + 2) : -1;
      if (ch2 == '-' && ch3 >= 0 && ch3 != ']') {
        int start_ch = ch;
        int end_ch = ch3;
        for (int i = (unsigned char)start_ch; i <= (unsigned char)end_ch; i++) {
          if (!spec->charset[i]) {
            spec->charset[i] = 1;
            spec->charset_len++;
          }
        }
        pos += 3;
      } else {
        if (!spec->charset[(unsigned char)ch]) {
          spec->charset[(unsigned char)ch] = 1;
          spec->charset_len++;
        }
        pos++;
      }
    }

    if (pos >= fmtLen) return -1;
    pos++;
    spec->specifier = '[';
    return (int)(pos - start);
  }

  return -1;
}

// Skip whitespace using object-based byte access
static size_t scan_skip_whitespace_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj strObj, size_t pos, size_t len) {
  int ch;
  while (pos < len && (ch = ops->string.byte_at(interp, strObj, pos)) >= 0 && feather_is_whitespace_full(ch)) {
    pos++;
  }
  return pos;
}

// Scan integer using object-based byte access
static int scan_integer_obj(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj strObj, size_t len, size_t *pos, int base, int width, int64_t *out) {
  size_t start = *pos;
  int negative = 0;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  int ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
  if (ch == '-' && consumed < max) {
    negative = 1;
    (*pos)++;
    consumed++;
  } else if (ch == '+' && consumed < max) {
    (*pos)++;
    consumed++;
  }

  if (base == 16 && *pos + 1 < len && consumed + 2 <= max) {
    int c0 = ops->string.byte_at(interp, strObj, *pos);
    int c1 = ops->string.byte_at(interp, strObj, *pos + 1);
    if (c0 == '0' && (c1 == 'x' || c1 == 'X')) {
      (*pos) += 2;
      consumed += 2;
    }
  }

  int64_t val = 0;
  int digits = 0;

  while (*pos < len && consumed < max) {
    ch = ops->string.byte_at(interp, strObj, *pos);
    int d = -1;

    if (base == 10 && feather_is_digit(ch)) {
      d = ch - '0';
    } else if (base == 8 && feather_is_octal_digit(ch)) {
      d = ch - '0';
    } else if (base == 16 && feather_is_hex_digit(ch)) {
      d = feather_hex_value(ch);
    } else if (base == 2 && is_binary_digit(ch)) {
      d = ch - '0';
    } else {
      break;
    }

    val = val * base + d;
    digits++;
    (*pos)++;
    consumed++;
  }

  if (digits == 0) {
    *pos = start;
    return 0;
  }

  *out = negative ? -val : val;
  return 1;
}

// Auto-detect integer base using object-based byte access
static int scan_auto_integer_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj strObj, size_t len, size_t *pos, int width, int64_t *out) {
  size_t start = *pos;
  int negative = 0;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  int ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
  if (ch == '-' && consumed < max) {
    negative = 1;
    (*pos)++;
    consumed++;
  } else if (ch == '+' && consumed < max) {
    (*pos)++;
    consumed++;
  }

  int base = 10;
  ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
  if (ch == '0' && consumed < max) {
    int c1 = (*pos + 1 < len) ? ops->string.byte_at(interp, strObj, *pos + 1) : -1;
    if ((c1 == 'x' || c1 == 'X') && consumed + 2 <= max) {
      base = 16;
      (*pos) += 2;
      consumed += 2;
    } else {
      base = 8;
    }
  }

  int64_t val = 0;
  int digits = 0;

  while (*pos < len && consumed < max) {
    ch = ops->string.byte_at(interp, strObj, *pos);
    int d = -1;

    if (base == 10 && feather_is_digit(ch)) {
      d = ch - '0';
    } else if (base == 8 && feather_is_octal_digit(ch)) {
      d = ch - '0';
    } else if (base == 16 && feather_is_hex_digit(ch)) {
      d = feather_hex_value(ch);
    } else {
      break;
    }

    val = val * base + d;
    digits++;
    (*pos)++;
    consumed++;
  }

  if (digits == 0) {
    *pos = start;
    return 0;
  }

  *out = negative ? -val : val;
  return 1;
}

// Scan float using object-based byte access
static int scan_float_obj(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj strObj, size_t len, size_t *pos, int width, double *out) {
  size_t start = *pos;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;
  int negative = 0;

  int ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
  if (ch == '-' && consumed < max) {
    negative = 1;
    (*pos)++;
    consumed++;
  } else if (ch == '+' && consumed < max) {
    (*pos)++;
    consumed++;
  }

  double val = 0.0;
  int digits = 0;

  while (*pos < len && consumed < max) {
    ch = ops->string.byte_at(interp, strObj, *pos);
    if (!feather_is_digit(ch)) break;
    val = val * 10.0 + (ch - '0');
    digits++;
    (*pos)++;
    consumed++;
  }

  ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
  if (ch == '.' && consumed < max) {
    (*pos)++;
    consumed++;
    double frac = 0.1;
    while (*pos < len && consumed < max) {
      ch = ops->string.byte_at(interp, strObj, *pos);
      if (!feather_is_digit(ch)) break;
      val += (ch - '0') * frac;
      frac *= 0.1;
      digits++;
      (*pos)++;
      consumed++;
    }
  }

  if (digits == 0) {
    *pos = start;
    return 0;
  }

  ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
  if ((ch == 'e' || ch == 'E') && consumed < max) {
    (*pos)++;
    consumed++;
    int expNeg = 0;
    ch = (*pos < len) ? ops->string.byte_at(interp, strObj, *pos) : -1;
    if (ch == '-' && consumed < max) {
      expNeg = 1;
      (*pos)++;
      consumed++;
    } else if (ch == '+' && consumed < max) {
      (*pos)++;
      consumed++;
    }
    int exp = 0;
    while (*pos < len && consumed < max) {
      ch = ops->string.byte_at(interp, strObj, *pos);
      if (!feather_is_digit(ch)) break;
      exp = exp * 10 + (ch - '0');
      (*pos)++;
      consumed++;
    }
    if (expNeg) exp = -exp;
    double multiplier = 1.0;
    if (exp > 0) {
      for (int i = 0; i < exp; i++) multiplier *= 10.0;
    } else {
      for (int i = 0; i < -exp; i++) multiplier /= 10.0;
    }
    val *= multiplier;
  }

  *out = negative ? -val : val;
  return 1;
}

// Scan non-whitespace string using object-based byte access
static int scan_string_obj(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj strObj, size_t len, size_t *pos, int width,
                           char *buf, size_t bufsize, size_t *outlen) {
  size_t start = *pos;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  while (*pos < len && consumed < max) {
    int ch = ops->string.byte_at(interp, strObj, *pos);
    if (feather_is_whitespace_full(ch)) break;
    if (*outlen < bufsize - 1) {
      buf[(*outlen)++] = (char)ch;
    }
    (*pos)++;
    consumed++;
  }

  if (*pos == start) return 0;
  buf[*outlen] = '\0';
  return 1;
}

// Scan charset using object-based byte access
static int scan_charset_obj(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj strObj, size_t len, size_t *pos, int width,
                            const char *charset, int negated,
                            char *buf, size_t bufsize, size_t *outlen) {
  size_t start = *pos;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  while (*pos < len && consumed < max) {
    int ch = ops->string.byte_at(interp, strObj, *pos);
    int in_set = charset[(unsigned char)ch];
    int match = negated ? !in_set : in_set;
    if (!match) break;
    if (*outlen < bufsize - 1) {
      buf[(*outlen)++] = (char)ch;
    }
    (*pos)++;
    consumed++;
  }

  if (*pos == start) return 0;
  buf[*outlen] = '\0';
  return 1;
}

FeatherResult feather_builtin_scan(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"scan string format ?varName ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj strObj = ops->list.at(interp, args, 0);
  FeatherObj fmtObj = ops->list.at(interp, args, 1);

  size_t strLen = ops->string.byte_length(interp, strObj);
  size_t fmtLen = ops->string.byte_length(interp, fmtObj);

  int varMode = (argc > 2);
  size_t numVars = argc - 2;

  FeatherObj results[64];
  size_t resultCount = 0;
  int positions[64];
  for (size_t i = 0; i < 64; i++) positions[i] = -1;

  size_t strPos = 0;
  size_t fmtPos = 0;
  size_t varIndex = 0;
  int conversions = 0;
  int usedPositional = -1;
  int anyConversionAttempted = 0;

  while (fmtPos < fmtLen) {
    int fc = ops->string.byte_at(interp, fmtObj, fmtPos);

    if (feather_is_whitespace_full(fc)) {
      fmtPos++;
      strPos = scan_skip_whitespace_obj(ops, interp, strObj, strPos, strLen);
      continue;
    }

    if (fc != '%') {
      int sc = (strPos < strLen) ? ops->string.byte_at(interp, strObj, strPos) : -1;
      if (sc != fc) {
        break;
      }
      strPos++;
      fmtPos++;
      continue;
    }

    fmtPos++;
    if (fmtPos >= fmtLen) break;

    ScanSpec spec;
    int consumed = parse_scan_spec_obj(ops, interp, fmtObj, fmtPos, fmtLen, &spec);
    if (consumed < 0) break;
    fmtPos += (size_t)consumed;

    if (spec.specifier == '%') {
      int sc = (strPos < strLen) ? ops->string.byte_at(interp, strObj, strPos) : -1;
      if (sc != '%') break;
      strPos++;
      continue;
    }

    if (spec.has_position) {
      if (usedPositional == 0) {
        FeatherObj msg = ops->string.intern(interp,
          "cannot mix \"%\" and \"%n$\" conversion specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      usedPositional = 1;
    } else if (!spec.suppress && spec.specifier != 'n') {
      if (usedPositional == 1) {
        FeatherObj msg = ops->string.intern(interp,
          "cannot mix \"%\" and \"%n$\" conversion specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      usedPositional = 0;
    }

    if (spec.specifier == 'n') {
      FeatherObj val = ops->integer.create(interp, (int64_t)strPos);
      if (spec.suppress) {
      } else if (spec.has_position) {
        int idx = spec.position - 1;
        if (idx >= 0 && idx < 64) {
          positions[idx] = (int)resultCount;
          results[resultCount++] = val;
        }
      } else {
        if (varMode) {
          if (varIndex < numVars) {
            FeatherObj varName = ops->list.at(interp, args, 2 + varIndex);
            if (feather_set_var(ops, interp, varName, val) != TCL_OK) {
              return TCL_ERROR;
            }
            varIndex++;
            conversions++;
          }
        } else {
          results[resultCount++] = val;
        }
      }
      continue;
    }

    if (spec.specifier != 'c' && spec.specifier != '[') {
      strPos = scan_skip_whitespace_obj(ops, interp, strObj, strPos, strLen);
    }

    anyConversionAttempted = 1;

    FeatherObj scannedVal = 0;
    int success = 0;

    switch (spec.specifier) {
      case 'd': {
        int64_t val;
        success = scan_integer_obj(ops, interp, strObj, strLen, &strPos, 10, spec.width, &val);
        if (success) {
          val = feather_apply_scan_truncation(val, spec.size_mod);
          scannedVal = ops->integer.create(interp, val);
        }
        break;
      }
      case 'u': {
        int64_t val;
        success = scan_integer_obj(ops, interp, strObj, strLen, &strPos, 10, spec.width, &val);
        if (success) {
          val = feather_apply_unsigned_conversion(val, spec.size_mod);
          scannedVal = ops->integer.create(interp, val);
        }
        break;
      }
      case 'o': {
        int64_t val;
        success = scan_integer_obj(ops, interp, strObj, strLen, &strPos, 8, spec.width, &val);
        if (success) {
          val = feather_apply_scan_truncation(val, spec.size_mod);
          scannedVal = ops->integer.create(interp, val);
        }
        break;
      }
      case 'x':
      case 'X': {
        int64_t val;
        success = scan_integer_obj(ops, interp, strObj, strLen, &strPos, 16, spec.width, &val);
        if (success) {
          val = feather_apply_scan_truncation(val, spec.size_mod);
          scannedVal = ops->integer.create(interp, val);
        }
        break;
      }
      case 'b': {
        int64_t val;
        success = scan_integer_obj(ops, interp, strObj, strLen, &strPos, 2, spec.width, &val);
        if (success) {
          val = feather_apply_scan_truncation(val, spec.size_mod);
          scannedVal = ops->integer.create(interp, val);
        }
        break;
      }
      case 'i': {
        int64_t val;
        success = scan_auto_integer_obj(ops, interp, strObj, strLen, &strPos, spec.width, &val);
        if (success) {
          val = feather_apply_scan_truncation(val, spec.size_mod);
          scannedVal = ops->integer.create(interp, val);
        }
        break;
      }
      case 'c': {
        // Read a Unicode codepoint
        if (strPos < strLen) {
          size_t bytes_read = 0;
          int64_t codepoint = decode_utf8_at_pos(ops, interp, strObj, strPos, strLen, &bytes_read);
          if (codepoint >= 0) {
            strPos += bytes_read;
            scannedVal = ops->integer.create(interp, codepoint);
            success = 1;
          }
        }
        break;
      }
      case 'f':
      case 'e':
      case 'E':
      case 'g':
      case 'G': {
        double val;
        success = scan_float_obj(ops, interp, strObj, strLen, &strPos, spec.width, &val);
        if (success) scannedVal = ops->dbl.create(interp, val);
        break;
      }
      case 's': {
        char buf[4096];
        size_t buflen = 0;
        success = scan_string_obj(ops, interp, strObj, strLen, &strPos, spec.width, buf, sizeof(buf), &buflen);
        if (success) scannedVal = ops->string.intern(interp, buf, buflen);
        break;
      }
      case '[': {
        char buf[4096];
        size_t buflen = 0;
        success = scan_charset_obj(ops, interp, strObj, strLen, &strPos, spec.width,
                               spec.charset, spec.charset_negated,
                               buf, sizeof(buf), &buflen);
        if (success) scannedVal = ops->string.intern(interp, buf, buflen);
        break;
      }
      default:
        break;
    }

    if (!success) break;

    if (spec.suppress) {
      continue;
    }

    if (spec.has_position) {
      int idx = spec.position - 1;
      if (idx >= 0 && idx < 64) {
        positions[idx] = (int)resultCount;
        results[resultCount++] = scannedVal;
      }
    } else {
      if (varMode) {
        if (varIndex < numVars) {
          FeatherObj varName = ops->list.at(interp, args, 2 + varIndex);
          if (feather_set_var(ops, interp, varName, scannedVal) != TCL_OK) {
            return TCL_ERROR;
          }
          varIndex++;
          conversions++;
        }
      } else {
        results[resultCount++] = scannedVal;
      }
    }
  }

  if (varMode) {
    if (usedPositional == 1) {
      conversions = 0;
      for (size_t i = 0; i < numVars && i < 64; i++) {
        if (positions[i] >= 0) {
          FeatherObj varName = ops->list.at(interp, args, 2 + i);
          if (feather_set_var(ops, interp, varName, results[positions[i]]) != TCL_OK) {
            return TCL_ERROR;
          }
          conversions++;
        }
      }
    }

    if (anyConversionAttempted && conversions == 0 && strPos >= strLen) {
      ops->interp.set_result(interp, ops->integer.create(interp, -1));
    } else {
      ops->interp.set_result(interp, ops->integer.create(interp, conversions));
    }
  } else {
    if (usedPositional == 1) {
      FeatherObj list = ops->list.create(interp);
      for (size_t i = 0; i < 64; i++) {
        if (positions[i] >= 0) {
          list = ops->list.push(interp, list, results[positions[i]]);
        }
      }
      ops->interp.set_result(interp, list);
    } else {
      FeatherObj list = ops->list.create(interp);
      for (size_t i = 0; i < resultCount; i++) {
        list = ops->list.push(interp, list, results[i]);
      }
      ops->interp.set_result(interp, list);
    }
  }

  return TCL_OK;
}

void feather_register_scan_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Parse string using conversion specifiers in the style of sscanf",
    "This command parses substrings from an input string in a fashion similar to "
    "the ANSI C sscanf procedure and returns a count of the number of conversions "
    "performed, or -1 if the end of the input string is reached before any "
    "conversions have been performed.\n\n"
    "String gives the input to be parsed and format indicates how to parse it, "
    "using % conversion specifiers as in sscanf. Each varName gives the name of a "
    "variable; when a substring is scanned from string that matches a conversion "
    "specifier, the substring is assigned to the corresponding variable.\n\n"
    "If no varName variables are specified, then scan works in an inline manner, "
    "returning the data that would otherwise be stored in the variables as a list. "
    "In the inline case, an empty string is returned when the end of the input "
    "string is reached before any conversions have been performed.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<string>");
  e = feather_usage_help(ops, interp, e,
    "The input string to parse");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<format>");
  e = feather_usage_help(ops, interp, e,
    "The format string containing conversion specifiers");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?varName?...");
  e = feather_usage_help(ops, interp, e,
    "Variable names to store parsed values (variable mode). If omitted, returns "
    "a list of values (inline mode).");
  spec = feather_usage_add(ops, interp, spec, e);

  /* Details on Scanning section */
  e = feather_usage_section(ops, interp, "Details On Scanning",
    "Scan operates by scanning string and format together. If the next character "
    "in format is a blank or tab then it matches any number of white space "
    "characters in string (including zero). Otherwise, if it is not a % character "
    "then it must match the next character of string. When a % is encountered in "
    "format, it indicates the start of a conversion specifier.\n\n"
    "A conversion specifier contains up to four fields after the %: a XPG3 "
    "position specifier (or a * to indicate the converted value is to be discarded "
    "instead of assigned to any variable); a number indicating a maximum substring "
    "width; a size modifier; and a conversion character. All of these fields are "
    "optional except for the conversion character.\n\n"
    "When scan finds a conversion specifier in format, it first skips any "
    "white-space characters in string (unless the conversion character is [ or c). "
    "Then it converts the next input characters according to the conversion "
    "specifier and stores the result in the variable given by the next argument "
    "to scan.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* Positional Specifiers section */
  e = feather_usage_section(ops, interp, "Positional Specifiers",
    "If the % is followed by a decimal number and a $, as in \"%2$d\", then the "
    "variable to use is not taken from the next sequential argument. Instead, it "
    "is taken from the argument indicated by the number, where 1 corresponds to "
    "the first varName.\n\n"
    "If there are any positional specifiers in format then all of the specifiers "
    "must be positional. In the inline case, any position can be specified at most "
    "once and the empty positions will be filled in with empty strings.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* Size Modifiers section */
  e = feather_usage_section(ops, interp, "Size Modifiers",
    "The size modifier field is used only when scanning a substring into one of "
    "Tcl's integer values. The size modifier field dictates the integer range "
    "acceptable to be stored in a variable.\n\n"
    "h          Truncate to 32-bit signed range (same as no modifier)\n\n"
    "l, j, q    Truncate to 64-bit range\n\n"
    "ll, L      No truncation (unlimited range)\n\n"
    "z, t       Platform-dependent (32-bit or 64-bit based on pointer size)");
  spec = feather_usage_add(ops, interp, spec, e);

  /* Conversion Specifiers section */
  e = feather_usage_section(ops, interp, "Conversion Specifiers",
    "d          Decimal integer. Truncated as required by the size modifier.\n\n"
    "o          Octal integer. Truncated as required by the size modifier.\n\n"
    "x, X       Hexadecimal integer. Truncated as required by the size modifier.\n\n"
    "b          Binary integer. Truncated as required by the size modifier.\n\n"
    "u          Unsigned decimal integer. The value is truncated as required by "
    "the size modifier, and the corresponding unsigned value for that truncated "
    "range is computed and stored.\n\n"
    "i          Integer with auto-detected base. The base is determined by C "
    "convention (leading 0 for octal; prefix 0x for hexadecimal).\n\n"
    "c          A single character is read and its Unicode value is stored as an "
    "integer. Initial white space is not skipped.\n\n"
    "s          The input substring consists of all characters up to the next "
    "white-space character; the characters are copied to the variable.\n\n"
    "e, f, g, E, G    Floating-point number consisting of an optional sign, a "
    "string of decimal digits possibly containing a decimal point, and an optional "
    "exponent consisting of an e or E followed by an optional sign and a string "
    "of decimal digits.\n\n"
    "[chars]    The input substring consists of one or more characters in chars. "
    "If the first character between the brackets is a ] then it is treated as "
    "part of chars rather than the closing bracket. If chars contains a sequence "
    "of the form a-b then any character between a and b (inclusive) will match.\n\n"
    "[^chars]   The input substring consists of one or more characters not in "
    "chars. The same escaping rules as [chars] apply.\n\n"
    "n          No input is consumed. Instead, the total number of characters "
    "scanned from the input string so far is stored in the variable.\n\n"
    "%%         A literal percent sign. No conversion is performed.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"25 100\" \"%d %d\" x y",
    "Parse two decimal integers, assign to x and y, return 2",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"25 100\" \"%d %d\"",
    "Parse two decimal integers inline, return list {25 100}",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"#08D03F\" \"#%2x%2x%2x\" r g b",
    "Parse RGB color: r=8, g=208, b=63",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"08:30\" \"%d:%d\" hours minutes",
    "Parse HH:MM time string",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"0xFF\" \"%i\" val",
    "Parse hexadecimal with auto-detect, assign 255 to val",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"A\" \"%c\" code",
    "Get Unicode codepoint of 'A' (65), assign to code",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"abc123def\" \"%[a-z]%d%s\" str num rest",
    "Parse with character set: str=abc, num=123, rest=def",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "scan \"20000000000000000000\" \"%lld\"",
    "Parse large integer with no truncation",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  /* See Also section */
  e = feather_usage_section(ops, interp, "See Also",
    "format(1), string(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "scan", spec);
}
