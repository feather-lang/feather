#include "feather.h"
#include "internal.h"
#include "charclass.h"

// Helper: convert integer to string representation
// Returns pointer to static buffer, caller should use immediately
static size_t int_to_str(int64_t val, char *buf, size_t bufsize, int base, int uppercase) {
  if (bufsize == 0) return 0;

  int negative = 0;
  uint64_t uval;

  if (val < 0 && base == 10) {
    negative = 1;
    uval = (uint64_t)(-val);
  } else {
    uval = (uint64_t)val;
  }

  // Build digits in reverse
  char digits[65];
  size_t ndigits = 0;

  if (uval == 0) {
    digits[ndigits++] = '0';
  } else {
    while (uval > 0) {
      int d = (int)(uval % (uint64_t)base);
      if (d < 10) {
        digits[ndigits++] = (char)('0' + d);
      } else {
        digits[ndigits++] = (char)((uppercase ? 'A' : 'a') + d - 10);
      }
      uval /= (uint64_t)base;
    }
  }

  // Calculate total size needed
  size_t total = ndigits + (negative ? 1 : 0);
  if (total > bufsize - 1) total = bufsize - 1;

  size_t pos = 0;
  if (negative && pos < total) {
    buf[pos++] = '-';
  }

  // Reverse digits into buffer
  while (ndigits > 0 && pos < total) {
    buf[pos++] = digits[--ndigits];
  }
  buf[pos] = '\0';

  return pos;
}

// Helper: convert unsigned integer to string
static size_t uint_to_str(uint64_t val, char *buf, size_t bufsize, int base, int uppercase) {
  if (bufsize == 0) return 0;

  char digits[65];
  size_t ndigits = 0;

  if (val == 0) {
    digits[ndigits++] = '0';
  } else {
    while (val > 0) {
      int d = (int)(val % (uint64_t)base);
      if (d < 10) {
        digits[ndigits++] = (char)('0' + d);
      } else {
        digits[ndigits++] = (char)((uppercase ? 'A' : 'a') + d - 10);
      }
      val /= (uint64_t)base;
    }
  }

  size_t pos = 0;
  while (ndigits > 0 && pos < bufsize - 1) {
    buf[pos++] = digits[--ndigits];
  }
  buf[pos] = '\0';

  return pos;
}



// Format specifier parsing state
typedef struct {
  int has_positional;     // Using %n$ style
  int position;           // Positional index (1-based) or -1 if sequential
  int left_justify;       // '-' flag
  int show_sign;          // '+' flag
  int space_sign;         // ' ' flag
  int zero_pad;           // '0' flag
  int alternate;          // '#' flag
  int width;              // Field width, -1 if from arg (*)
  int width_from_arg;     // Width should come from next arg
  int precision;          // Precision, -1 if from arg (*), -2 if not specified
  int precision_from_arg; // Precision should come from next arg
  char specifier;         // Conversion specifier (d, s, x, etc.)
  SizeModifier size_mod;  // Size modifier (h, l, ll, etc.)
} FormatSpec;

// Parse a format specifier starting after the '%'
// fmtObj: the format string object
// start: position in fmtObj to start parsing (after '%')
// fmtLen: total length of format string
// Returns number of characters consumed, or -1 on error
static int parse_format_spec_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj fmtObj, size_t start, size_t fmtLen,
                                 FormatSpec *spec) {
  size_t pos = start;

  // Initialize spec
  spec->has_positional = 0;
  spec->position = -1;
  spec->left_justify = 0;
  spec->show_sign = 0;
  spec->space_sign = 0;
  spec->zero_pad = 0;
  spec->alternate = 0;
  spec->width = 0;
  spec->width_from_arg = 0;
  spec->precision = -2; // Not specified
  spec->precision_from_arg = 0;
  spec->specifier = 0;
  spec->size_mod = SIZE_NONE;

  if (pos >= fmtLen) return -1;

  int ch = ops->string.byte_at(interp, fmtObj, pos);

  // Check for %% 
  if (ch == '%') {
    spec->specifier = '%';
    return 1;
  }

  // Check for positional specifier (n$)
  size_t posStart = pos;
  while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && feather_is_digit(ch)) {
    pos++;
  }
  if (pos > posStart && pos < fmtLen && ops->string.byte_at(interp, fmtObj, pos) == '$') {
    // Parse positional index
    int idx = 0;
    for (size_t i = posStart; i < pos; i++) {
      idx = idx * 10 + (ops->string.byte_at(interp, fmtObj, i) - '0');
    }
    spec->has_positional = 1;
    spec->position = idx;
    pos++; // Skip '$'
  } else {
    // Not positional, reset pos
    pos = posStart;
  }

  // Parse flags
  while (pos < fmtLen) {
    ch = ops->string.byte_at(interp, fmtObj, pos);
    if (ch == '-') {
      spec->left_justify = 1;
    } else if (ch == '+') {
      spec->show_sign = 1;
    } else if (ch == ' ') {
      spec->space_sign = 1;
    } else if (ch == '0') {
      spec->zero_pad = 1;
    } else if (ch == '#') {
      spec->alternate = 1;
    } else {
      break;
    }
    pos++;
  }

  // Parse width
  ch = (pos < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos) : -1;
  if (ch == '*') {
    spec->width_from_arg = 1;
    spec->width = -1;
    pos++;
  } else {
    while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && feather_is_digit(ch)) {
      spec->width = spec->width * 10 + (ch - '0');
      pos++;
    }
  }

  // Parse precision
  ch = (pos < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos) : -1;
  if (ch == '.') {
    pos++;
    spec->precision = 0;
    ch = (pos < fmtLen) ? ops->string.byte_at(interp, fmtObj, pos) : -1;
    if (ch == '*') {
      spec->precision_from_arg = 1;
      spec->precision = -1;
      pos++;
    } else {
      while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && feather_is_digit(ch)) {
        spec->precision = spec->precision * 10 + (ch - '0');
        pos++;
      }
    }
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

  // Parse specifier
  if (pos >= fmtLen) return -1;

  ch = ops->string.byte_at(interp, fmtObj, pos);
  if (ch == 'd' || ch == 'i' || ch == 'u' || ch == 'o' || ch == 'x' || ch == 'X' ||
      ch == 'b' || ch == 'c' || ch == 's' || ch == 'f' || ch == 'e' || ch == 'E' ||
      ch == 'g' || ch == 'G' || ch == 'a' || ch == 'A' || ch == 'p') {
    spec->specifier = (char)ch;
    pos++;
    return (int)(pos - start);
  }

  return -1; // Invalid specifier
}

// Apply field width and justification
// If padchar is '0' and the string starts with a sign, the zeros go after the sign
static FeatherObj apply_width(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj str, int width, int left_justify, char padchar) {
  if (width <= 0) return str;

  size_t len = ops->string.byte_length(interp, str);

  if (len >= (size_t)width) return str;

  size_t padlen = (size_t)width - len;
  char padbuf[256];
  if (padlen > sizeof(padbuf) - 1) padlen = sizeof(padbuf) - 1;

  for (size_t i = 0; i < padlen; i++) {
    padbuf[i] = padchar;
  }
  padbuf[padlen] = '\0';

  FeatherObj pad = ops->string.intern(interp, padbuf, padlen);

  if (left_justify) {
    return ops->string.concat(interp, str, pad);
  } else {
    // Special case: zero padding with sign - zeros go after sign
    int first_byte = ops->string.byte_at(interp, str, 0);
    if (padchar == '0' && len > 0 && (first_byte == '-' || first_byte == '+')) {
      char signbuf[2] = {(char)first_byte, '\0'};
      FeatherObj sign = ops->string.intern(interp, signbuf, 1);
      FeatherObj rest = ops->string.slice(interp, str, 1, len);
      FeatherObj result = ops->string.concat(interp, sign, pad);
      return ops->string.concat(interp, result, rest);
    }
    return ops->string.concat(interp, pad, str);
  }
}

// Format an integer value
static FeatherObj format_integer(const FeatherHostOps *ops, FeatherInterp interp,
                                int64_t val, FormatSpec *spec) {
  char buf[128];
  size_t buflen;
  int base = 10;
  int uppercase = 0;
  int is_unsigned = 0;

  // Apply truncation based on size modifier
  val = feather_apply_format_truncation(val, spec->size_mod);

  switch (spec->specifier) {
    case 'd':
    case 'i':
      base = 10;
      break;
    case 'u':
      base = 10;
      is_unsigned = 1;
      break;
    case 'o':
      base = 8;
      is_unsigned = 1;
      break;
    case 'x':
      base = 16;
      is_unsigned = 1;
      break;
    case 'X':
      base = 16;
      is_unsigned = 1;
      uppercase = 1;
      break;
    case 'b':
      base = 2;
      is_unsigned = 1;
      break;
    default:
      base = 10;
  }

  if (is_unsigned) {
    buflen = uint_to_str((uint64_t)val, buf, sizeof(buf), base, uppercase);
  } else {
    buflen = int_to_str(val, buf, sizeof(buf), base, uppercase);
  }

  // Apply precision (minimum digits)
  int precision = spec->precision;
  if (precision == -2) precision = 1; // Default
  if (precision == -1) precision = 0;

  // Handle precision: pad with leading zeros if needed
  size_t numDigits = buflen;
  int hasSign = (buf[0] == '-' || buf[0] == '+');
  if (hasSign) numDigits--;

  FeatherObj result;
  if ((int)numDigits < precision) {
    size_t padcount = (size_t)precision - numDigits;
    char padbuf[128];
    for (size_t i = 0; i < padcount && i < sizeof(padbuf) - 1; i++) {
      padbuf[i] = '0';
    }
    padbuf[padcount] = '\0';

    if (hasSign) {
      char signbuf[2] = {buf[0], '\0'};
      FeatherObj sign = ops->string.intern(interp, signbuf, 1);
      FeatherObj zeros = ops->string.intern(interp, padbuf, padcount);
      FeatherObj digits = ops->string.intern(interp, buf + 1, buflen - 1);
      result = ops->string.concat(interp, sign, zeros);
      result = ops->string.concat(interp, result, digits);
    } else {
      FeatherObj zeros = ops->string.intern(interp, padbuf, padcount);
      FeatherObj digits = ops->string.intern(interp, buf, buflen);
      result = ops->string.concat(interp, zeros, digits);
    }
  } else {
    result = ops->string.intern(interp, buf, buflen);
  }

  // Add sign if needed
  if (!hasSign && val >= 0 && !is_unsigned) {
    if (spec->show_sign) {
      FeatherObj plus = ops->string.intern(interp, "+", 1);
      result = ops->string.concat(interp, plus, result);
    } else if (spec->space_sign) {
      FeatherObj space = ops->string.intern(interp, " ", 1);
      result = ops->string.concat(interp, space, result);
    }
  }

  // Add alternate prefix
  int used_decimal_alternate = 0;
  if (spec->alternate && val != 0) {
    if (spec->specifier == 'x' || spec->specifier == 'X') {
      FeatherObj prefix = ops->string.intern(interp, "0x", 2);
      result = ops->string.concat(interp, prefix, result);
    } else if (spec->specifier == 'o') {
      FeatherObj prefix = ops->string.intern(interp, "0o", 2);
      result = ops->string.concat(interp, prefix, result);
    } else if (spec->specifier == 'b') {
      FeatherObj prefix = ops->string.intern(interp, "0b", 2);
      result = ops->string.concat(interp, prefix, result);
    } else if (spec->specifier == 'd' || spec->specifier == 'i') {
      used_decimal_alternate = 1;
      // Handle %#d specially: sign + 0d + zeros + digits
      // We'll apply width here instead of in apply_width
      FeatherObj prefix = ops->string.intern(interp, "0d", 2);
      int first_byte = ops->string.byte_at(interp, result, 0);
      int result_has_sign = (first_byte == '-' || first_byte == '+');
      size_t result_len = ops->string.byte_length(interp, result);

      // Calculate total length needed for width
      size_t total_len = result_len + 2; // result + "0d"

      if (spec->zero_pad && !spec->left_justify && spec->width > 0 &&
          (size_t)spec->width > total_len) {
        // Need zero padding: sign + 0d + zeros + digits
        size_t zeros_needed = (size_t)spec->width - total_len;
        char zeros[256];
        if (zeros_needed > sizeof(zeros) - 1) zeros_needed = sizeof(zeros) - 1;
        for (size_t i = 0; i < zeros_needed; i++) zeros[i] = '0';
        zeros[zeros_needed] = '\0';
        FeatherObj zerosObj = ops->string.intern(interp, zeros, zeros_needed);

        if (result_has_sign) {
          // -42 with width 8 -> -0d00042
          char signbuf[2] = {(char)first_byte, '\0'};
          FeatherObj sign = ops->string.intern(interp, signbuf, 1);
          FeatherObj digits = ops->string.slice(interp, result, 1, result_len);
          result = ops->string.concat(interp, sign, prefix);
          result = ops->string.concat(interp, result, zerosObj);
          result = ops->string.concat(interp, result, digits);
        } else {
          // 42 with width 8 -> 0d000042
          result = ops->string.concat(interp, prefix, zerosObj);
          FeatherObj digits = ops->string.intern(interp, buf, buflen);
          result = ops->string.concat(interp, result, digits);
        }
      } else {
        // No zero padding, just add prefix
        if (result_has_sign) {
          size_t len = ops->string.byte_length(interp, result);
          char signbuf[2] = {(char)first_byte, '\0'};
          FeatherObj sign = ops->string.intern(interp, signbuf, 1);
          FeatherObj rest = ops->string.slice(interp, result, 1, len);
          result = ops->string.concat(interp, sign, prefix);
          result = ops->string.concat(interp, result, rest);
        } else {
          result = ops->string.concat(interp, prefix, result);
        }
      }
    }
  }

  // Apply width (skip for %#d with non-zero value - already handled)
  if (!used_decimal_alternate) {
    char padchar = (spec->zero_pad && !spec->left_justify && spec->precision == -2) ? '0' : ' ';
    result = apply_width(ops, interp, result, spec->width, spec->left_justify, padchar);
  } else if (spec->width > 0 && !spec->zero_pad) {
    // %#d needs space padding (not zero), use apply_width
    result = apply_width(ops, interp, result, spec->width, spec->left_justify, ' ');
  }

  return result;
}

// Format a string value
static FeatherObj format_string(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj str, FormatSpec *spec) {
  // Apply precision (max chars)
  if (spec->precision >= 0) {
    size_t len = ops->rune.length(interp, str);
    if (len > (size_t)spec->precision) {
      str = ops->rune.range(interp, str, 0, spec->precision - 1);
    }
  }

  // Apply width
  str = apply_width(ops, interp, str, spec->width, spec->left_justify, ' ');

  return str;
}

FeatherResult feather_builtin_format(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"format formatString ?arg ...?\"", 55);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj fmtObj = ops->list.at(interp, args, 0);
  size_t fmtLen = ops->string.byte_length(interp, fmtObj);

  FeatherObj result = ops->string.intern(interp, "", 0);
  size_t argIndex = 1; // Next argument to use (1-based: args[0] is format string)
  int usedPositional = -1; // -1 = unknown, 0 = sequential, 1 = positional

  size_t pos = 0;
  while (pos < fmtLen) {
    // Find next %
    size_t start = pos;
    int ch;
    while (pos < fmtLen && (ch = ops->string.byte_at(interp, fmtObj, pos)) >= 0 && ch != '%') {
      pos++;
    }

    // Append literal text
    if (pos > start) {
      FeatherObj literal = ops->string.slice(interp, fmtObj, start, pos);
      result = ops->string.concat(interp, result, literal);
    }

    if (pos >= fmtLen) break;

    // Skip the %
    pos++;
    if (pos >= fmtLen) {
      FeatherObj msg = ops->string.intern(interp,
        "format string ended in middle of field specifier", 48);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Parse format specifier
    FormatSpec spec;
    int consumed = parse_format_spec_obj(ops, interp, fmtObj, pos, fmtLen, &spec);
    if (consumed < 0) {
      FeatherObj msg = ops->string.intern(interp,
        "format string ended in middle of field specifier", 48);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    pos += (size_t)consumed;

    // Handle %%
    if (spec.specifier == '%') {
      FeatherObj pct = ops->string.intern(interp, "%", 1);
      result = ops->string.concat(interp, result, pct);
      continue;
    }

    // Check positional vs sequential consistency
    if (spec.has_positional) {
      if (usedPositional == 0) {
        FeatherObj msg = ops->string.intern(interp,
          "cannot mix \"%\" and \"%n$\" conversion specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      usedPositional = 1;

      if (spec.position < 1 || (size_t)spec.position >= argc) {
        FeatherObj msg = ops->string.intern(interp,
          "\"%n$\" argument index out of range", 33);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    } else {
      if (usedPositional == 1) {
        FeatherObj msg = ops->string.intern(interp,
          "cannot mix \"%\" and \"%n$\" conversion specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      usedPositional = 0;
    }

    // Get width from argument if needed
    if (spec.width_from_arg) {
      if (argIndex >= argc) {
        FeatherObj msg = ops->string.intern(interp,
          "not enough arguments for all format specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj widthArg = ops->list.at(interp, args, argIndex++);
      int64_t w;
      if (ops->integer.get(interp, widthArg, &w) != TCL_OK) {
        return TCL_ERROR;
      }
      spec.width = (int)w;
      if (spec.width < 0) {
        spec.left_justify = 1;
        spec.width = -spec.width;
      }
    }

    // Get precision from argument if needed
    if (spec.precision_from_arg) {
      if (argIndex >= argc) {
        FeatherObj msg = ops->string.intern(interp,
          "not enough arguments for all format specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj precArg = ops->list.at(interp, args, argIndex++);
      int64_t p;
      if (ops->integer.get(interp, precArg, &p) != TCL_OK) {
        return TCL_ERROR;
      }
      spec.precision = (int)p;
      if (spec.precision < 0) spec.precision = 0;
    }

    // Get the value to format
    size_t valueIndex;
    if (spec.has_positional) {
      valueIndex = (size_t)spec.position;
    } else {
      if (argIndex >= argc) {
        FeatherObj msg = ops->string.intern(interp,
          "not enough arguments for all format specifiers", 46);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      valueIndex = argIndex++;
    }

    FeatherObj value = ops->list.at(interp, args, valueIndex);
    FeatherObj formatted;

    switch (spec.specifier) {
      case 'd':
      case 'i':
      case 'u':
      case 'o':
      case 'x':
      case 'X':
      case 'b': {
        int64_t intVal;
        if (ops->integer.get(interp, value, &intVal) != TCL_OK) {
          feather_error_expected(ops, interp, "integer", value);
          return TCL_ERROR;
        }
        formatted = format_integer(ops, interp, intVal, &spec);
        break;
      }

      case 'c': {
        int64_t charVal;
        if (ops->integer.get(interp, value, &charVal) != TCL_OK) {
          feather_error_expected(ops, interp, "integer", value);
          return TCL_ERROR;
        }
        // Convert integer to Unicode character
        // For simplicity, handle ASCII range directly
        if (charVal >= 0 && charVal <= 127) {
          char c = (char)charVal;
          formatted = ops->string.intern(interp, &c, 1);
        } else {
          // Use host's integer to character conversion via string creation
          formatted = ops->integer.create(interp, charVal);
          // Actually need to create a character - use rune.at on a string created from codepoint
          // For now, handle basic ASCII and let host handle Unicode
          char buf[8];
          if (charVal < 128) {
            buf[0] = (char)charVal;
            buf[1] = '\0';
            formatted = ops->string.intern(interp, buf, 1);
          } else {
            // UTF-8 encode
            if (charVal < 0x80) {
              buf[0] = (char)charVal;
              formatted = ops->string.intern(interp, buf, 1);
            } else if (charVal < 0x800) {
              buf[0] = (char)(0xC0 | (charVal >> 6));
              buf[1] = (char)(0x80 | (charVal & 0x3F));
              formatted = ops->string.intern(interp, buf, 2);
            } else if (charVal < 0x10000) {
              buf[0] = (char)(0xE0 | (charVal >> 12));
              buf[1] = (char)(0x80 | ((charVal >> 6) & 0x3F));
              buf[2] = (char)(0x80 | (charVal & 0x3F));
              formatted = ops->string.intern(interp, buf, 3);
            } else {
              buf[0] = (char)(0xF0 | (charVal >> 18));
              buf[1] = (char)(0x80 | ((charVal >> 12) & 0x3F));
              buf[2] = (char)(0x80 | ((charVal >> 6) & 0x3F));
              buf[3] = (char)(0x80 | (charVal & 0x3F));
              formatted = ops->string.intern(interp, buf, 4);
            }
          }
        }
        formatted = apply_width(ops, interp, formatted, spec.width, spec.left_justify, ' ');
        break;
      }

      case 's': {
        formatted = format_string(ops, interp, value, &spec);
        break;
      }

      case 'f':
      case 'e':
      case 'E':
      case 'g':
      case 'G':
      case 'a':
      case 'A': {
        double dblVal;
        if (ops->dbl.get(interp, value, &dblVal) != TCL_OK) {
          return TCL_ERROR;
        }
        // TCL format errors on NaN
        if (ops->dbl.classify(dblVal) == FEATHER_DBL_NAN) {
          FeatherObj msg = ops->string.intern(interp, "floating point value is Not a Number", 36);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        int precision = spec.precision;
        if (precision == -2) precision = 6; // Default
        if (precision == -1) precision = 0;

        formatted = ops->dbl.format(interp, dblVal, spec.specifier, precision, spec.alternate);
        formatted = apply_width(ops, interp, formatted, spec.width, spec.left_justify, ' ');
        break;
      }

      case 'p': {
        // Pointer format: 0x followed by hex address
        int64_t intVal;
        if (ops->integer.get(interp, value, &intVal) != TCL_OK) {
          return TCL_ERROR;
        }
        char buf[32];
        size_t buflen = uint_to_str((uint64_t)intVal, buf, sizeof(buf), 16, 0);
        FeatherObj hex = ops->string.intern(interp, buf, buflen);
        FeatherObj prefix = ops->string.intern(interp, "0x", 2);
        formatted = ops->string.concat(interp, prefix, hex);
        formatted = apply_width(ops, interp, formatted, spec.width, spec.left_justify, ' ');
        break;
      }

      default: {
        FeatherObj msg = ops->string.intern(interp,
          "format string ended in middle of field specifier", 49);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    }

    result = ops->string.concat(interp, result, formatted);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
