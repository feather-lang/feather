#include "feather.h"
#include "internal.h"

// Helper: check if character is a digit
static int is_digit(char c) {
  return c >= '0' && c <= '9';
}

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

// Helper: convert double to string
// This is a simplified implementation for basic floating point formatting
static FeatherObj double_to_str(const FeatherHostOps *ops, FeatherInterp interp,
                              double val, char specifier, int precision, int trim_zeros) {
  char buf[128];
  size_t pos = 0;

  // Handle negative values
  int negative = 0;
  if (val < 0) {
    negative = 1;
    val = -val;
  }

  // For %g/%G, decide between %f and %e style
  if (specifier == 'g' || specifier == 'G') {
    // Calculate exponent
    int exponent = 0;
    double temp = val;
    if (temp != 0.0) {
      while (temp >= 10.0) { temp /= 10.0; exponent++; }
      while (temp < 1.0 && temp != 0.0) { temp *= 10.0; exponent--; }
    }

    int sigfigs = (precision > 0) ? precision : 6;
    // Use exponential notation if exponent < -4 or exponent >= precision
    if (exponent < -4 || exponent >= sigfigs) {
      specifier = (specifier == 'g') ? 'e' : 'E';
      precision = sigfigs - 1;
    } else {
      specifier = 'f';
      // For %g, precision is total significant digits, not decimal places
      precision = sigfigs - exponent - 1;
      if (precision < 0) precision = 0;
    }
    trim_zeros = 1;
  }

  if (specifier == 'e' || specifier == 'E') {
    // Scientific notation: x.yyye±zz
    int exponent = 0;

    if (val != 0.0) {
      while (val >= 10.0) { val /= 10.0; exponent++; }
      while (val < 1.0) { val *= 10.0; exponent--; }
    }

    // Round the mantissa
    double rounding = 0.5;
    for (int i = 0; i < precision; i++) rounding /= 10.0;
    val += rounding;
    if (val >= 10.0) {
      val /= 10.0;
      exponent++;
    }

    // Write sign
    if (negative) buf[pos++] = '-';

    // Write mantissa integer part
    int intPart = (int)val;
    buf[pos++] = (char)('0' + intPart);
    val -= intPart;

    // Write decimal point and fraction
    size_t decimalPos = pos;
    if (precision > 0) {
      buf[pos++] = '.';
      for (int i = 0; i < precision && pos < sizeof(buf) - 10; i++) {
        val *= 10.0;
        int digit = (int)val;
        if (digit > 9) digit = 9;
        buf[pos++] = (char)('0' + digit);
        val -= digit;
      }
    }

    // Trim trailing zeros for %g
    if (trim_zeros && precision > 0) {
      while (pos > decimalPos + 1 && buf[pos - 1] == '0') {
        pos--;
      }
      if (pos == decimalPos + 1) {
        pos = decimalPos; // Remove decimal point too
      }
    }

    // Write exponent
    buf[pos++] = specifier; // 'e' or 'E'
    buf[pos++] = (exponent >= 0) ? '+' : '-';
    if (exponent < 0) exponent = -exponent;
    if (exponent >= 100) {
      buf[pos++] = (char)('0' + exponent / 100);
      buf[pos++] = (char)('0' + (exponent / 10) % 10);
      buf[pos++] = (char)('0' + exponent % 10);
    } else {
      buf[pos++] = (char)('0' + exponent / 10);
      buf[pos++] = (char)('0' + exponent % 10);
    }
  } else {
    // Fixed notation: xxx.yyy
    if (negative) buf[pos++] = '-';

    // Round the value first
    double rounding = 0.5;
    for (int i = 0; i < precision; i++) rounding /= 10.0;
    val += rounding;

    // Extract integer part
    uint64_t intPart = (uint64_t)val;
    double fracPart = val - (double)intPart;

    // Write integer part
    char intBuf[32];
    size_t intLen = uint_to_str(intPart, intBuf, sizeof(intBuf), 10, 0);
    for (size_t i = 0; i < intLen && pos < sizeof(buf) - 10; i++) {
      buf[pos++] = intBuf[i];
    }

    // Write decimal point and fraction
    size_t decimalPos = pos;
    if (precision > 0) {
      buf[pos++] = '.';
      for (int i = 0; i < precision && pos < sizeof(buf) - 2; i++) {
        fracPart *= 10.0;
        int digit = (int)fracPart;
        if (digit > 9) digit = 9;
        buf[pos++] = (char)('0' + digit);
        fracPart -= digit;
      }
    }

    // Trim trailing zeros for %g
    if (trim_zeros && precision > 0) {
      while (pos > decimalPos + 1 && buf[pos - 1] == '0') {
        pos--;
      }
      if (pos == decimalPos + 1) {
        pos = decimalPos; // Remove decimal point too
      }
    }
  }

  buf[pos] = '\0';
  return ops->string.intern(interp, buf, pos);
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
} FormatSpec;

// Parse a format specifier starting after the '%'
// Returns number of characters consumed, or -1 on error
static int parse_format_spec(const char *fmt, size_t len, FormatSpec *spec) {
  size_t pos = 0;

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

  if (pos >= len) return -1;

  // Check for %% 
  if (fmt[pos] == '%') {
    spec->specifier = '%';
    return 1;
  }

  // Check for positional specifier (n$)
  size_t posStart = pos;
  while (pos < len && is_digit(fmt[pos])) {
    pos++;
  }
  if (pos > posStart && pos < len && fmt[pos] == '$') {
    // Parse positional index
    int idx = 0;
    for (size_t i = posStart; i < pos; i++) {
      idx = idx * 10 + (fmt[i] - '0');
    }
    spec->has_positional = 1;
    spec->position = idx;
    pos++; // Skip '$'
  } else {
    // Not positional, reset pos
    pos = posStart;
  }

  // Parse flags
  while (pos < len) {
    if (fmt[pos] == '-') {
      spec->left_justify = 1;
    } else if (fmt[pos] == '+') {
      spec->show_sign = 1;
    } else if (fmt[pos] == ' ') {
      spec->space_sign = 1;
    } else if (fmt[pos] == '0') {
      spec->zero_pad = 1;
    } else if (fmt[pos] == '#') {
      spec->alternate = 1;
    } else {
      break;
    }
    pos++;
  }

  // Parse width
  if (pos < len && fmt[pos] == '*') {
    spec->width_from_arg = 1;
    spec->width = -1;
    pos++;
  } else {
    while (pos < len && is_digit(fmt[pos])) {
      spec->width = spec->width * 10 + (fmt[pos] - '0');
      pos++;
    }
  }

  // Parse precision
  if (pos < len && fmt[pos] == '.') {
    pos++;
    spec->precision = 0;
    if (pos < len && fmt[pos] == '*') {
      spec->precision_from_arg = 1;
      spec->precision = -1;
      pos++;
    } else {
      while (pos < len && is_digit(fmt[pos])) {
        spec->precision = spec->precision * 10 + (fmt[pos] - '0');
        pos++;
      }
    }
  }

  // Skip size modifiers (ll, h, l, z, t, L)
  if (pos < len) {
    if (fmt[pos] == 'l') {
      pos++;
      if (pos < len && fmt[pos] == 'l') pos++;
    } else if (fmt[pos] == 'h' || fmt[pos] == 'z' || fmt[pos] == 't' || fmt[pos] == 'L' ||
               fmt[pos] == 'j' || fmt[pos] == 'q') {
      pos++;
    }
  }

  // Parse specifier
  if (pos >= len) return -1;

  char c = fmt[pos];
  if (c == 'd' || c == 'i' || c == 'u' || c == 'o' || c == 'x' || c == 'X' ||
      c == 'b' || c == 'c' || c == 's' || c == 'f' || c == 'e' || c == 'E' ||
      c == 'g' || c == 'G' || c == 'a' || c == 'A' || c == 'p') {
    spec->specifier = c;
    pos++;
    return (int)pos;
  }

  return -1; // Invalid specifier
}

// Apply field width and justification
// If padchar is '0' and the string starts with a sign, the zeros go after the sign
static FeatherObj apply_width(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj str, int width, int left_justify, char padchar) {
  if (width <= 0) return str;

  size_t len;
  const char *s = ops->string.get(interp, str, &len);

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
    if (padchar == '0' && len > 0 && (s[0] == '-' || s[0] == '+')) {
      char signbuf[2] = {s[0], '\0'};
      FeatherObj sign = ops->string.intern(interp, signbuf, 1);
      FeatherObj rest = ops->string.intern(interp, s + 1, len - 1);
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
    }
  }

  // Apply width
  char padchar = (spec->zero_pad && !spec->left_justify && spec->precision == -2) ? '0' : ' ';
  result = apply_width(ops, interp, result, spec->width, spec->left_justify, padchar);

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
  size_t fmtLen;
  const char *fmt = ops->string.get(interp, fmtObj, &fmtLen);

  FeatherObj result = ops->string.intern(interp, "", 0);
  size_t argIndex = 1; // Next argument to use (1-based: args[0] is format string)
  int usedPositional = -1; // -1 = unknown, 0 = sequential, 1 = positional

  size_t pos = 0;
  while (pos < fmtLen) {
    // Find next %
    size_t start = pos;
    while (pos < fmtLen && fmt[pos] != '%') {
      pos++;
    }

    // Append literal text
    if (pos > start) {
      FeatherObj literal = ops->string.intern(interp, fmt + start, pos - start);
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
    int consumed = parse_format_spec(fmt + pos, fmtLen - pos, &spec);
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
          size_t vlen;
          const char *vstr = ops->string.get(interp, value, &vlen);
          FeatherObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
          FeatherObj valStr = ops->string.intern(interp, vstr, vlen);
          msg = ops->string.concat(interp, msg, valStr);
          FeatherObj suffix = ops->string.intern(interp, "\"", 1);
          msg = ops->string.concat(interp, msg, suffix);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        formatted = format_integer(ops, interp, intVal, &spec);
        break;
      }

      case 'c': {
        int64_t charVal;
        if (ops->integer.get(interp, value, &charVal) != TCL_OK) {
          FeatherObj msg = ops->string.intern(interp,
            "expected integer but got \"", 26);
          msg = ops->string.concat(interp, msg, value);
          FeatherObj suffix = ops->string.intern(interp, "\"", 1);
          msg = ops->string.concat(interp, msg, suffix);
          ops->interp.set_result(interp, msg);
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
        int precision = spec.precision;
        if (precision == -2) precision = 6; // Default
        if (precision == -1) precision = 0;

        formatted = double_to_str(ops, interp, dblVal, spec.specifier, precision, 0);
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
