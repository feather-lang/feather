#include "feather.h"
#include "internal.h"
#include "charclass.h"

static int scan_is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int scan_is_hex_digit(char c) {
  return feather_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_binary_digit(char c) {
  return c == '0' || c == '1';
}

static int scan_hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  return -1;
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
} ScanSpec;

static int parse_scan_spec(const char *fmt, size_t len, ScanSpec *spec) {
  size_t pos = 0;

  spec->suppress = 0;
  spec->width = 0;
  spec->position = -1;
  spec->has_position = 0;
  spec->specifier = 0;
  spec->charset_negated = 0;
  spec->charset_len = 0;
  for (int i = 0; i < 256; i++) spec->charset[i] = 0;

  if (pos >= len) return -1;

  if (fmt[pos] == '%') {
    spec->specifier = '%';
    return 1;
  }

  size_t posStart = pos;
  while (pos < len && feather_is_digit(fmt[pos])) {
    pos++;
  }
  if (pos > posStart && pos < len && fmt[pos] == '$') {
    int idx = 0;
    for (size_t i = posStart; i < pos; i++) {
      idx = idx * 10 + (fmt[i] - '0');
    }
    spec->has_position = 1;
    spec->position = idx;
    pos++;
  } else {
    pos = posStart;
  }

  if (pos < len && fmt[pos] == '*') {
    spec->suppress = 1;
    pos++;
  }

  while (pos < len && feather_is_digit(fmt[pos])) {
    spec->width = spec->width * 10 + (fmt[pos] - '0');
    pos++;
  }

  if (pos < len) {
    if (fmt[pos] == 'l') {
      pos++;
      if (pos < len && fmt[pos] == 'l') pos++;
    } else if (fmt[pos] == 'h' || fmt[pos] == 'z' || fmt[pos] == 't' ||
               fmt[pos] == 'L' || fmt[pos] == 'j' || fmt[pos] == 'q') {
      pos++;
    }
  }

  if (pos >= len) return -1;

  char c = fmt[pos];
  if (c == 'd' || c == 'i' || c == 'u' || c == 'o' || c == 'x' || c == 'X' ||
      c == 'b' || c == 'c' || c == 's' || c == 'f' || c == 'e' || c == 'E' ||
      c == 'g' || c == 'G' || c == 'n') {
    spec->specifier = c;
    pos++;
    return (int)pos;
  }

  if (c == '[') {
    pos++;
    if (pos >= len) return -1;

    if (fmt[pos] == '^') {
      spec->charset_negated = 1;
      pos++;
    }

    if (pos < len && fmt[pos] == ']') {
      spec->charset[(unsigned char)']'] = 1;
      spec->charset_len++;
      pos++;
    }

    while (pos < len && fmt[pos] != ']') {
      char ch = fmt[pos];
      if (pos + 2 < len && fmt[pos + 1] == '-' && fmt[pos + 2] != ']') {
        char start = ch;
        char end = fmt[pos + 2];
        for (int i = (unsigned char)start; i <= (unsigned char)end; i++) {
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

    if (pos >= len) return -1;
    pos++;
    spec->specifier = '[';
    return (int)pos;
  }

  return -1;
}

static int scan_skip_whitespace(const char *str, size_t len, size_t *pos) {
  size_t start = *pos;
  while (*pos < len && scan_is_whitespace(str[*pos])) {
    (*pos)++;
  }
  return (int)(*pos - start);
}

static int scan_integer(const char *str, size_t len, size_t *pos, int base, int width, int64_t *out) {
  size_t start = *pos;
  int negative = 0;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  if (*pos < len && consumed < max && str[*pos] == '-') {
    negative = 1;
    (*pos)++;
    consumed++;
  } else if (*pos < len && consumed < max && str[*pos] == '+') {
    (*pos)++;
    consumed++;
  }

  if (base == 16 && *pos + 1 < len && consumed + 2 <= max &&
      str[*pos] == '0' && (str[*pos + 1] == 'x' || str[*pos + 1] == 'X')) {
    (*pos) += 2;
    consumed += 2;
  }

  int64_t val = 0;
  int digits = 0;

  while (*pos < len && consumed < max) {
    char c = str[*pos];
    int d = -1;

    if (base == 10 && feather_is_digit(c)) {
      d = c - '0';
    } else if (base == 8 && feather_is_octal_digit(c)) {
      d = c - '0';
    } else if (base == 16 && scan_is_hex_digit(c)) {
      d = scan_hex_value(c);
    } else if (base == 2 && is_binary_digit(c)) {
      d = c - '0';
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

static int scan_auto_integer(const char *str, size_t len, size_t *pos, int width, int64_t *out) {
  size_t start = *pos;
  int negative = 0;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  if (*pos < len && consumed < max && str[*pos] == '-') {
    negative = 1;
    (*pos)++;
    consumed++;
  } else if (*pos < len && consumed < max && str[*pos] == '+') {
    (*pos)++;
    consumed++;
  }

  int base = 10;
  if (*pos < len && consumed < max && str[*pos] == '0') {
    if (*pos + 1 < len && consumed + 2 <= max &&
        (str[*pos + 1] == 'x' || str[*pos + 1] == 'X')) {
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
    char c = str[*pos];
    int d = -1;

    if (base == 10 && feather_is_digit(c)) {
      d = c - '0';
    } else if (base == 8 && feather_is_octal_digit(c)) {
      d = c - '0';
    } else if (base == 16 && scan_is_hex_digit(c)) {
      d = scan_hex_value(c);
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

static int scan_float(const char *str, size_t len, size_t *pos, int width, double *out) {
  size_t start = *pos;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;
  int negative = 0;

  if (*pos < len && consumed < max && str[*pos] == '-') {
    negative = 1;
    (*pos)++;
    consumed++;
  } else if (*pos < len && consumed < max && str[*pos] == '+') {
    (*pos)++;
    consumed++;
  }

  double val = 0.0;
  int digits = 0;

  while (*pos < len && consumed < max && feather_is_digit(str[*pos])) {
    val = val * 10.0 + (str[*pos] - '0');
    digits++;
    (*pos)++;
    consumed++;
  }

  if (*pos < len && consumed < max && str[*pos] == '.') {
    (*pos)++;
    consumed++;
    double frac = 0.1;
    while (*pos < len && consumed < max && feather_is_digit(str[*pos])) {
      val += (str[*pos] - '0') * frac;
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

  if (*pos < len && consumed < max && (str[*pos] == 'e' || str[*pos] == 'E')) {
    (*pos)++;
    consumed++;
    int expNeg = 0;
    if (*pos < len && consumed < max && str[*pos] == '-') {
      expNeg = 1;
      (*pos)++;
      consumed++;
    } else if (*pos < len && consumed < max && str[*pos] == '+') {
      (*pos)++;
      consumed++;
    }
    int exp = 0;
    while (*pos < len && consumed < max && feather_is_digit(str[*pos])) {
      exp = exp * 10 + (str[*pos] - '0');
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

static int scan_string(const char *str, size_t len, size_t *pos, int width,
                       char *buf, size_t bufsize, size_t *outlen) {
  size_t start = *pos;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  while (*pos < len && consumed < max && !scan_is_whitespace(str[*pos])) {
    if (*outlen < bufsize - 1) {
      buf[(*outlen)++] = str[*pos];
    }
    (*pos)++;
    consumed++;
  }

  if (*pos == start) return 0;
  buf[*outlen] = '\0';
  return 1;
}

static int scan_charset(const char *str, size_t len, size_t *pos, int width,
                        const char *charset, int negated,
                        char *buf, size_t bufsize, size_t *outlen) {
  size_t start = *pos;
  int max = width > 0 ? width : (int)(len - *pos);
  int consumed = 0;

  while (*pos < len && consumed < max) {
    char c = str[*pos];
    int in_set = charset[(unsigned char)c];
    int match = negated ? !in_set : in_set;
    if (!match) break;
    if (*outlen < bufsize - 1) {
      buf[(*outlen)++] = c;
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

  size_t strLen;
  const char *str = ops->string.get(interp, strObj, &strLen);
  size_t fmtLen;
  const char *fmt = ops->string.get(interp, fmtObj, &fmtLen);

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
    char fc = fmt[fmtPos];

    if (scan_is_whitespace(fc)) {
      fmtPos++;
      scan_skip_whitespace(str, strLen, &strPos);
      continue;
    }

    if (fc != '%') {
      if (strPos >= strLen || str[strPos] != fc) {
        break;
      }
      strPos++;
      fmtPos++;
      continue;
    }

    fmtPos++;
    if (fmtPos >= fmtLen) break;

    ScanSpec spec;
    int consumed = parse_scan_spec(fmt + fmtPos, fmtLen - fmtPos, &spec);
    if (consumed < 0) break;
    fmtPos += (size_t)consumed;

    if (spec.specifier == '%') {
      if (strPos >= strLen || str[strPos] != '%') break;
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
            ops->var.set(interp, varName, val);
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
      scan_skip_whitespace(str, strLen, &strPos);
    }

    anyConversionAttempted = 1;

    FeatherObj scannedVal = 0;
    int success = 0;

    switch (spec.specifier) {
      case 'd':
      case 'u': {
        int64_t val;
        success = scan_integer(str, strLen, &strPos, 10, spec.width, &val);
        if (success) scannedVal = ops->integer.create(interp, val);
        break;
      }
      case 'o': {
        int64_t val;
        success = scan_integer(str, strLen, &strPos, 8, spec.width, &val);
        if (success) scannedVal = ops->integer.create(interp, val);
        break;
      }
      case 'x':
      case 'X': {
        int64_t val;
        success = scan_integer(str, strLen, &strPos, 16, spec.width, &val);
        if (success) scannedVal = ops->integer.create(interp, val);
        break;
      }
      case 'b': {
        int64_t val;
        success = scan_integer(str, strLen, &strPos, 2, spec.width, &val);
        if (success) scannedVal = ops->integer.create(interp, val);
        break;
      }
      case 'i': {
        int64_t val;
        success = scan_auto_integer(str, strLen, &strPos, spec.width, &val);
        if (success) scannedVal = ops->integer.create(interp, val);
        break;
      }
      case 'c': {
        if (strPos < strLen) {
          unsigned char c = (unsigned char)str[strPos];
          strPos++;
          scannedVal = ops->integer.create(interp, (int64_t)c);
          success = 1;
        }
        break;
      }
      case 'f':
      case 'e':
      case 'E':
      case 'g':
      case 'G': {
        double val;
        success = scan_float(str, strLen, &strPos, spec.width, &val);
        if (success) scannedVal = ops->dbl.create(interp, val);
        break;
      }
      case 's': {
        char buf[4096];
        size_t buflen = 0;
        success = scan_string(str, strLen, &strPos, spec.width, buf, sizeof(buf), &buflen);
        if (success) scannedVal = ops->string.intern(interp, buf, buflen);
        break;
      }
      case '[': {
        char buf[4096];
        size_t buflen = 0;
        success = scan_charset(str, strLen, &strPos, spec.width,
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
          ops->var.set(interp, varName, scannedVal);
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
          ops->var.set(interp, varName, results[positions[i]]);
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
