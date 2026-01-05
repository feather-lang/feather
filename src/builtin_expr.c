#include "feather.h"
#include "internal.h"
#include "charclass.h"

/**
 * Expression parser for TCL expr command.
 *
 * Operator precedence (lowest to highest):
 *   ?: (ternary, right-to-left)
 *   || (logical OR)
 *   && (logical AND)
 *   |  (bitwise OR)
 *   ^  (bitwise XOR)
 *   &  (bitwise AND)
 *   == != eq ne (equality)
 *   < <= > >= lt le gt ge (comparison)
 *   + - (additive)
 *   * / % (multiplicative)
 *   ** (exponentiation, right-to-left)
 *   unary - + ~ ! (negation, complement, logical not)
 *   () function calls, parentheses
 *   literals, variables, commands
 *
 * This parser uses the existing TCL parser for command substitution.
 * String comparison is delegated to the host via ops->string.compare.
 * Math functions are delegated to tcl::mathfunc::name commands.
 *
 * This version uses position-based iteration (byte_at, slice) instead of
 * pointer-based iteration, eliminating ops->string.get() calls.
 */

// ExprValue can be an integer, double, or string (FeatherObj)
typedef struct {
  int64_t int_val;
  double dbl_val;
  FeatherObj str_val;      // 0 means no string value
  int is_int;          // 1 if has valid integer rep
  int is_double;       // 1 if has valid double rep
} ExprValue;

typedef struct {
  const FeatherHostOps *ops;
  FeatherInterp interp;
  FeatherObj expr_obj; // Expression object
  size_t len;          // Length of expression in bytes
  size_t pos;          // Current position (byte index)
  int has_error;
  FeatherObj error_msg;
  int skip_mode;       // When true, skip evaluation (for lazy eval)
} ExprParser;

// Helper macro for byte access
#define BYTE_AT(p, i) ((p)->ops->string.byte_at((p)->interp, (p)->expr_obj, (i)))
#define CUR_BYTE(p) BYTE_AT(p, (p)->pos)
#define AT_END(p) ((p)->pos >= (p)->len)

// Forward declarations
static ExprValue parse_ternary(ExprParser *p);
static ExprValue parse_logical_or(ExprParser *p);
static ExprValue parse_logical_and(ExprParser *p);
static ExprValue parse_bitwise_or(ExprParser *p);
static ExprValue parse_bitwise_xor(ExprParser *p);
static ExprValue parse_bitwise_and(ExprParser *p);
static ExprValue parse_equality(ExprParser *p);
static ExprValue parse_comparison(ExprParser *p);
static ExprValue parse_shift(ExprParser *p);
static ExprValue parse_additive(ExprParser *p);
static ExprValue parse_multiplicative(ExprParser *p);
static ExprValue parse_exponentiation(ExprParser *p);
static ExprValue parse_unary(ExprParser *p);
static ExprValue parse_primary(ExprParser *p);

// Create an integer ExprValue
static ExprValue make_int(int64_t val) {
  ExprValue v = {.int_val = val, .dbl_val = 0, .str_val = 0, .is_int = 1, .is_double = 0};
  return v;
}

// Create a double ExprValue
static ExprValue make_double(double val) {
  ExprValue v = {.int_val = 0, .dbl_val = val, .str_val = 0, .is_int = 0, .is_double = 1};
  return v;
}

// Create a string ExprValue
static ExprValue make_str(FeatherObj obj) {
  ExprValue v = {.int_val = 0, .dbl_val = 0, .str_val = obj, .is_int = 0, .is_double = 0};
  return v;
}

// Create an error ExprValue (signals error without value)
static ExprValue make_error(void) {
  ExprValue v = {.int_val = 0, .dbl_val = 0, .str_val = 0, .is_int = 0, .is_double = 0};
  return v;
}

// Create a double ExprValue, but check for NaN and return error if so
// This is used for arithmetic operations that can produce NaN (e.g., 0.0/0)
static ExprValue make_double_checked(ExprParser *p, double val) {
  if (p->ops->dbl.classify(val) == FEATHER_DBL_NAN) {
    p->has_error = 1;
    p->error_msg = p->ops->string.intern(p->interp, "domain error: argument not in valid range", 41);
    return make_error();
  }
  return make_double(val);
}

// Get integer from ExprValue, shimmering if needed
static int get_int(ExprParser *p, ExprValue *v, int64_t *out) {
  if (v->is_int) {
    *out = v->int_val;
    return 1;
  }
  // Shimmer from double
  if (v->is_double) {
    v->int_val = (int64_t)v->dbl_val;
    v->is_int = 1;
    *out = v->int_val;
    return 1;
  }
  if (v->str_val == 0) {
    return 0;
  }
  // Try to convert string to integer
  if (p->ops->integer.get(p->interp, v->str_val, out) == TCL_OK) {
    v->int_val = *out;
    v->is_int = 1;
    return 1;
  }
  return 0;
}

// Get double from ExprValue, shimmering if needed
static int get_double(ExprParser *p, ExprValue *v, double *out) {
  if (v->is_double) {
    *out = v->dbl_val;
    return 1;
  }
  // Shimmer from int
  if (v->is_int) {
    v->dbl_val = (double)v->int_val;
    v->is_double = 1;
    *out = v->dbl_val;
    return 1;
  }
  if (v->str_val == 0) {
    return 0;
  }
  // Try to convert string to double
  if (p->ops->dbl.get(p->interp, v->str_val, out) == TCL_OK) {
    v->dbl_val = *out;
    v->is_double = 1;
    return 1;
  }
  return 0;
}

// Check if ExprValue is a floating-point type (has decimal point)
static int is_floating(ExprValue *v) {
  return v->is_double && !v->is_int;
}

// Get FeatherObj from ExprValue
static FeatherObj get_obj(ExprParser *p, ExprValue *v) {
  if (v->str_val != 0) {
    return v->str_val;
  }
  if (v->is_int) {
    v->str_val = p->ops->integer.create(p->interp, v->int_val);
    return v->str_val;
  }
  if (v->is_double) {
    v->str_val = p->ops->dbl.create(p->interp, v->dbl_val);
    return v->str_val;
  }
  return 0;
}

// Check if an object's string representation looks like a floating-point number.
// Returns 1 if the string contains '.', 'e', 'E', or special values like "Inf", "NaN".
// This is used to preserve numeric type when getting results from command/function calls.
static int looks_like_float_obj(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj obj) {
  size_t len = ops->string.byte_length(interp, obj);

  // Check for '.', 'e', 'E' in the string
  for (size_t i = 0; i < len; i++) {
    int ch = ops->string.byte_at(interp, obj, i);
    if (ch == '.' || ch == 'e' || ch == 'E') {
      return 1;
    }
  }

  // Check for special IEEE 754 values: Inf, -Inf, NaN
  if (len == 3) {
    int c0 = ops->string.byte_at(interp, obj, 0);
    int c1 = ops->string.byte_at(interp, obj, 1);
    int c2 = ops->string.byte_at(interp, obj, 2);
    if ((c0 == 'I' && c1 == 'n' && c2 == 'f') ||
        (c0 == 'N' && c1 == 'a' && c2 == 'N')) {
      return 1;
    }
  } else if (len == 4) {
    int c0 = ops->string.byte_at(interp, obj, 0);
    int c1 = ops->string.byte_at(interp, obj, 1);
    int c2 = ops->string.byte_at(interp, obj, 2);
    int c3 = ops->string.byte_at(interp, obj, 3);
    if (c0 == '-' && c1 == 'I' && c2 == 'n' && c3 == 'f') {
      return 1;
    }
  }

  return 0;
}

static void expr_skip_whitespace(ExprParser *p) {
  while (p->pos < p->len) {
    int c = CUR_BYTE(p);
    if (feather_is_whitespace_full(c)) {
      p->pos++;
    } else if (c == '#') {
      // Comment - skip to end of line or expression
      while (p->pos < p->len && CUR_BYTE(p) != '\n') {
        p->pos++;
      }
    } else {
      break;
    }
  }
}

static void set_error(ExprParser *p, const char *msg, size_t len) {
  if (p->has_error) return;
  p->has_error = 1;
  p->error_msg = p->ops->string.intern(p->interp, msg, len);
}

static void set_syntax_error(ExprParser *p) {
  if (p->has_error) return;
  p->has_error = 1;

  FeatherObj part1 = p->ops->string.intern(p->interp, "syntax error in expression \"", 28);
  FeatherObj part3 = p->ops->string.intern(p->interp, "\"", 1);

  FeatherObj msg = p->ops->string.concat(p->interp, part1, p->expr_obj);
  msg = p->ops->string.concat(p->interp, msg, part3);
  p->error_msg = msg;
}

// Version of set_integer_error that takes a FeatherObj
static void set_integer_error_obj(ExprParser *p, FeatherObj value) {
  if (p->has_error) return;
  p->has_error = 1;
  feather_error_expected(p->ops, p->interp, "integer", value);
  p->error_msg = p->ops->interp.get_result(p->interp);
}

static void set_bareword_error_obj(ExprParser *p, FeatherObj word) {
  if (p->has_error) return;
  p->has_error = 1;

  FeatherObj part1 = p->ops->string.intern(p->interp, "invalid bareword \"", 18);
  FeatherObj part3 = p->ops->string.intern(p->interp, "\"", 1);

  FeatherObj msg = p->ops->string.concat(p->interp, part1, word);
  msg = p->ops->string.concat(p->interp, msg, part3);
  p->error_msg = msg;
}

static void set_paren_error(ExprParser *p) {
  if (p->has_error) return;
  p->has_error = 1;

  FeatherObj part1 = p->ops->string.intern(p->interp, "unbalanced parentheses in expression \"", 38);
  FeatherObj part3 = p->ops->string.intern(p->interp, "\"", 1);

  FeatherObj msg = p->ops->string.concat(p->interp, part1, p->expr_obj);
  msg = p->ops->string.concat(p->interp, msg, part3);
  p->error_msg = msg;
}

static void set_close_paren_error(ExprParser *p) {
  if (p->has_error) return;
  p->has_error = 1;
  p->error_msg = p->ops->string.intern(p->interp, "unbalanced close paren", 22);
}

// Check if character is alphanumeric or underscore
static int is_alnum(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

// Check if we're at a word boundary for keyword matching
static int match_keyword(ExprParser *p, const char *kw, size_t kwlen) {
  if (p->len - p->pos < kwlen) return 0;
  for (size_t i = 0; i < kwlen; i++) {
    int c = BYTE_AT(p, p->pos + i);
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c != kw[i]) return 0;
  }
  // Ensure not followed by alphanumeric
  if (p->pos + kwlen < p->len && is_alnum(BYTE_AT(p, p->pos + kwlen))) return 0;
  return 1;
}

// Parse a variable reference $name or ${name}
static ExprValue parse_variable(ExprParser *p) {
  p->pos++; // skip $

  if (AT_END(p)) {
    set_syntax_error(p);
    return make_error();
  }

  size_t name_start;
  size_t name_len;

  if (CUR_BYTE(p) == '{') {
    p->pos++;
    name_start = p->pos;
    while (p->pos < p->len && CUR_BYTE(p) != '}') {
      p->pos++;
    }
    if (AT_END(p)) {
      set_syntax_error(p);
      return make_error();
    }
    name_len = p->pos - name_start;
    p->pos++;
  } else {
    name_start = p->pos;
    while (p->pos < p->len) {
      int ch = CUR_BYTE(p);
      if (feather_is_varname_char(ch) || ch == ':') {
        p->pos++;
      } else {
        break;
      }
    }
    name_len = p->pos - name_start;
  }

  if (name_len == 0) {
    set_syntax_error(p);
    return make_error();
  }

  // In skip mode, just return a dummy value without evaluating
  if (p->skip_mode) {
    return make_int(0);
  }

  // Extract the variable name as an object
  FeatherObj name_obj = p->ops->string.slice(p->interp, p->expr_obj, name_start, name_start + name_len);

  // feather_get_var handles qualified names and fires traces
  FeatherObj value;
  FeatherResult res = feather_get_var(p->ops, p->interp, name_obj, &value);
  if (res != TCL_OK) {
    // Read trace error - get error message from interpreter result
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  if (p->ops->list.is_nil(p->interp, value)) {
    FeatherObj part1 = p->ops->string.intern(p->interp, "can't read \"", 12);
    FeatherObj part3 = p->ops->string.intern(p->interp, "\": no such variable", 19);
    FeatherObj msg = p->ops->string.concat(p->interp, part1, name_obj);
    msg = p->ops->string.concat(p->interp, msg, part3);
    p->has_error = 1;
    p->error_msg = msg;
    return make_error();
  }

  return make_str(value);
}

// Parse command substitution [cmd args...]
static ExprValue parse_command(ExprParser *p) {
  p->pos++; // skip [

  size_t cmd_start = p->pos;
  int depth = 1;

  while (p->pos < p->len && depth > 0) {
    int c = CUR_BYTE(p);
    if (c == '[') {
      depth++;
    } else if (c == ']') {
      depth--;
    } else if (c == '\\' && p->pos + 1 < p->len) {
      p->pos++;
    }
    if (depth > 0) p->pos++;
  }

  if (depth != 0) {
    set_syntax_error(p);
    return make_error();
  }

  size_t cmd_len = p->pos - cmd_start;
  p->pos++; // skip ]

  // In skip mode, just return a dummy value without evaluating
  if (p->skip_mode) {
    return make_int(0);
  }

  // Extract command as object and use object-based eval
  FeatherObj cmd_obj = p->ops->string.slice(p->interp, p->expr_obj, cmd_start, cmd_start + cmd_len);
  FeatherResult result = feather_script_eval_obj(p->ops, p->interp, cmd_obj, TCL_EVAL_LOCAL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  // Try to preserve numeric type from command result.
  FeatherObj result_obj = p->ops->interp.get_result(p->interp);

  if (looks_like_float_obj(p->ops, p->interp, result_obj)) {
    double dval;
    if (p->ops->dbl.get(p->interp, result_obj, &dval) == TCL_OK) {
      return make_double(dval);
    }
  } else {
    int64_t ival;
    if (p->ops->integer.get(p->interp, result_obj, &ival) == TCL_OK) {
      return make_int(ival);
    }
  }
  return make_str(result_obj);
}

// Parse braced string {...}
static ExprValue parse_braced(ExprParser *p) {
  p->pos++; // skip {
  size_t start = p->pos;
  int depth = 1;

  while (p->pos < p->len && depth > 0) {
    int c = CUR_BYTE(p);
    if (c == '{') depth++;
    else if (c == '}') depth--;
    p->pos++;
  }

  if (depth != 0) {
    set_syntax_error(p);
    return make_error();
  }

  size_t content_len = p->pos - start - 1;
  FeatherObj str = p->ops->string.slice(p->interp, p->expr_obj, start, start + content_len);
  return make_str(str);
}

// Parse quoted string "..." with variable and command substitution
static ExprValue parse_quoted(ExprParser *p) {
  p->pos++; // skip "
  size_t start = p->pos;

  // Find the closing quote, handling backslash escapes
  while (p->pos < p->len && CUR_BYTE(p) != '"') {
    if (CUR_BYTE(p) == '\\' && p->pos + 1 < p->len) {
      p->pos += 2;  // skip backslash and following char
    } else {
      p->pos++;
    }
  }

  if (AT_END(p)) {
    set_syntax_error(p);
    return make_error();
  }

  size_t content_len = p->pos - start;
  p->pos++; // skip closing "

  // In skip mode, just return a dummy value without evaluating
  if (p->skip_mode) {
    return make_int(0);
  }

  // Extract the content and perform substitutions using object-based API
  FeatherObj content = p->ops->string.slice(p->interp, p->expr_obj, start, start + content_len);
  FeatherResult result = feather_subst_obj(p->ops, p->interp, content, TCL_SUBST_ALL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  return make_str(p->ops->interp.get_result(p->interp));
}

// Parse a floating-point number by extracting the string and using host parser
// This is more accurate than computing mantissa * 10^exp manually
static ExprValue parse_float_string(ExprParser *p, size_t start, int negative) {
  // Extract the number string from the expression
  FeatherObj num_str = p->ops->string.slice(p->interp, p->expr_obj, start, p->pos);

  // Remove any underscores from the string (not supported by standard parsers)
  // For now, just use the host's double parser directly
  double val;
  if (p->ops->dbl.get(p->interp, num_str, &val) == TCL_OK) {
    if (negative) val = -val;
    return make_double(val);
  }

  // Fallback: shouldn't happen for valid float syntax we parsed
  set_syntax_error(p);
  return make_error();
}

// Parse number literal (integer or floating-point)
// Integers: 123, 0x1f, 0b101, 0o17, with optional underscores
// Floats: 3.14, .5, 5., 1e10, 3.14e-5
static ExprValue parse_number(ExprParser *p) {
  size_t start = p->pos;
  int negative = 0;
  int is_float = 0;
  int has_underscores = 0;

  int c = CUR_BYTE(p);
  if (c == '-') {
    negative = 1;
    p->pos++;
  } else if (c == '+') {
    p->pos++;
  }

  size_t num_start = p->pos; // Start of number without sign

  // Handle leading decimal point: .5
  if (p->pos < p->len && CUR_BYTE(p) == '.') {
    is_float = 1;
    p->pos++;
    if (AT_END(p) || CUR_BYTE(p) < '0' || CUR_BYTE(p) > '9') {
      FeatherObj token = p->ops->string.slice(p->interp, p->expr_obj, start,
                                              p->pos > start ? p->pos : start + 1);
      set_integer_error_obj(p, token);
      return make_error();
    }
    // Skip fractional digits
    while (p->pos < p->len) {
      c = CUR_BYTE(p);
      if (c == '_') { has_underscores = 1; p->pos++; continue; }
      if (c < '0' || c > '9') break;
      p->pos++;
    }
    // Check for exponent
    if (p->pos < p->len && (CUR_BYTE(p) == 'e' || CUR_BYTE(p) == 'E')) {
      p->pos++;
      if (p->pos < p->len && (CUR_BYTE(p) == '-' || CUR_BYTE(p) == '+')) p->pos++;
      while (p->pos < p->len && CUR_BYTE(p) >= '0' && CUR_BYTE(p) <= '9') {
        p->pos++;
      }
    }
    // Use host parser for the float string
    return parse_float_string(p, num_start, negative);
  }

  if (AT_END(p) || (CUR_BYTE(p) < '0' || CUR_BYTE(p) > '9')) {
    FeatherObj token = p->ops->string.slice(p->interp, p->expr_obj, start,
                                            p->pos > start ? p->pos : start + 1);
    set_integer_error_obj(p, token);
    return make_error();
  }

  int base = 10;

  // Check for radix prefix (only for integers)
  if (CUR_BYTE(p) == '0' && p->pos + 1 < p->len) {
    int next = BYTE_AT(p, p->pos + 1);
    if (next == 'x' || next == 'X') {
      base = 16;
      p->pos += 2;
    } else if (next == 'b' || next == 'B') {
      base = 2;
      p->pos += 2;
    } else if (next == 'o' || next == 'O') {
      base = 8;
      p->pos += 2;
    } else if (next == 'd' || next == 'D') {
      // Explicit decimal prefix - base stays 10
      p->pos += 2;
    }
  }

  // Parse integer part
  int64_t int_value = 0;
  while (p->pos < p->len) {
    c = CUR_BYTE(p);
    if (c == '_') { has_underscores = 1; p->pos++; continue; }
    int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    if (digit < 0 || digit >= base) break;
    int_value = int_value * base + digit;
    p->pos++;
  }

  // Check for decimal point (only base 10)
  if (base == 10 && p->pos < p->len && CUR_BYTE(p) == '.') {
    // Look ahead to distinguish 5.0 from 5.method()
    if (p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) >= '0' && BYTE_AT(p, p->pos + 1) <= '9') {
      is_float = 1;
      p->pos++; // skip .
      // Skip fractional digits
      while (p->pos < p->len) {
        c = CUR_BYTE(p);
        if (c == '_') { has_underscores = 1; p->pos++; continue; }
        if (c < '0' || c > '9') break;
        p->pos++;
      }
      // Check for exponent
      if (p->pos < p->len && (CUR_BYTE(p) == 'e' || CUR_BYTE(p) == 'E')) {
        p->pos++;
        if (p->pos < p->len && (CUR_BYTE(p) == '-' || CUR_BYTE(p) == '+')) p->pos++;
        while (p->pos < p->len && CUR_BYTE(p) >= '0' && CUR_BYTE(p) <= '9') {
          p->pos++;
        }
      }
      // Use host parser for the float string
      return parse_float_string(p, num_start, negative);
    }
  }

  // Check for exponent without decimal point (e.g., 1e10) - only base 10
  if (base == 10 && p->pos < p->len && (CUR_BYTE(p) == 'e' || CUR_BYTE(p) == 'E')) {
    is_float = 1;
    p->pos++;
    if (p->pos < p->len && (CUR_BYTE(p) == '-' || CUR_BYTE(p) == '+')) p->pos++;
    while (p->pos < p->len && CUR_BYTE(p) >= '0' && CUR_BYTE(p) <= '9') {
      p->pos++;
    }
    // Use host parser for the float string
    return parse_float_string(p, num_start, negative);
  }

  return make_int(negative ? -int_value : int_value);
}

// Parse function call: funcname(arg, arg, ...)
static ExprValue parse_function_call(ExprParser *p, FeatherObj func_name_obj) {
  p->pos++; // skip (

  // Build command: tcl::mathfunc::name arg arg ...
  FeatherObj prefix = p->ops->string.intern(p->interp, "tcl::mathfunc::", 15);
  FeatherObj full_cmd = p->ops->string.concat(p->interp, prefix, func_name_obj);

  FeatherObj args = p->ops->list.create(p->interp);

  // Parse arguments
  expr_skip_whitespace(p);
  while (p->pos < p->len && CUR_BYTE(p) != ')') {
    ExprValue arg = parse_ternary(p);
    if (p->has_error) return make_error();

    // Only collect argument values when not in skip mode
    if (!p->skip_mode) {
      FeatherObj arg_obj = get_obj(p, &arg);
      args = p->ops->list.push(p->interp, args, arg_obj);
    }

    expr_skip_whitespace(p);
    if (p->pos < p->len && CUR_BYTE(p) == ',') {
      p->pos++;
      expr_skip_whitespace(p);
    }
  }

  if (AT_END(p) || CUR_BYTE(p) != ')') {
    set_paren_error(p);
    return make_error();
  }
  p->pos++; // skip )

  // In skip mode, just return a dummy value without evaluating
  if (p->skip_mode) {
    return make_int(0);
  }

  // Build the command string: "tcl::mathfunc::name arg1 arg2 ..."
  FeatherObj cmd_str = full_cmd;
  size_t argc = p->ops->list.length(p->interp, args);
  for (size_t i = 0; i < argc; i++) {
    FeatherObj space = p->ops->string.intern(p->interp, " ", 1);
    FeatherObj arg = p->ops->list.at(p->interp, args, i);
    cmd_str = p->ops->string.concat(p->interp, cmd_str, space);
    cmd_str = p->ops->string.concat(p->interp, cmd_str, arg);
  }

  // Evaluate the command using the object-based API
  FeatherResult result = feather_script_eval_obj(p->ops, p->interp, cmd_str, TCL_EVAL_LOCAL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  // Try to preserve numeric type from function result
  FeatherObj result_obj = p->ops->interp.get_result(p->interp);

  if (looks_like_float_obj(p->ops, p->interp, result_obj)) {
    double dval;
    if (p->ops->dbl.get(p->interp, result_obj, &dval) == TCL_OK) {
      return make_double(dval);
    }
  } else {
    int64_t ival;
    if (p->ops->integer.get(p->interp, result_obj, &ival) == TCL_OK) {
      return make_int(ival);
    }
  }
  return make_str(result_obj);
}

// Check if byte at position matches expected byte
static int byte_matches(ExprParser *p, size_t pos, int expected) {
  if (pos >= p->len) return 0;
  return BYTE_AT(p, pos) == expected;
}

// Case-insensitive comparison of identifier at [start, start+len) with target
static int ident_equals_ci(ExprParser *p, size_t start, size_t len, const char *target, size_t target_len) {
  if (len != target_len) return 0;
  for (size_t i = 0; i < len; i++) {
    int c = BYTE_AT(p, start + i);
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c != target[i]) return 0;
  }
  return 1;
}

// Parse primary: number, variable, command, boolean, braced/quoted string, paren, function call
static ExprValue parse_primary(ExprParser *p) {
  expr_skip_whitespace(p);

  if (p->has_error || AT_END(p)) {
    if (!p->has_error) set_syntax_error(p);
    return make_error();
  }

  int c = CUR_BYTE(p);

  // Parenthesized expression
  if (c == '(') {
    p->pos++;
    ExprValue val = parse_ternary(p);
    if (p->has_error) return make_error();
    expr_skip_whitespace(p);
    if (AT_END(p) || CUR_BYTE(p) != ')') {
      set_paren_error(p);
      return make_error();
    }
    p->pos++;
    return val;
  }

  // Variable
  if (c == '$') {
    return parse_variable(p);
  }

  // Command substitution
  if (c == '[') {
    return parse_command(p);
  }

  // Braced string
  if (c == '{') {
    return parse_braced(p);
  }

  // Quoted string
  if (c == '"') {
    return parse_quoted(p);
  }

  // Number (includes negative numbers and floats starting with .)
  if ((c >= '0' && c <= '9') ||
      ((c == '-' || c == '+') && p->pos + 1 < p->len &&
       (BYTE_AT(p, p->pos + 1) >= '0' && BYTE_AT(p, p->pos + 1) <= '9' || BYTE_AT(p, p->pos + 1) == '.')) ||
      (c == '.' && p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) >= '0' && BYTE_AT(p, p->pos + 1) <= '9')) {
    return parse_number(p);
  }

  // Boolean literals and function names (identifiers)
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
    size_t start = p->pos;
    while (p->pos < p->len && is_alnum(CUR_BYTE(p))) {
      p->pos++;
    }
    size_t len = p->pos - start;

    // Check for function call
    expr_skip_whitespace(p);
    if (p->pos < p->len && CUR_BYTE(p) == '(') {
      FeatherObj func_name = p->ops->string.slice(p->interp, p->expr_obj, start, start + len);
      return parse_function_call(p, func_name);
    }

    // Check for boolean literals (case-insensitive)
    if (ident_equals_ci(p, start, len, "true", 4)) {
      return make_int(1);
    }
    if (ident_equals_ci(p, start, len, "false", 5)) {
      return make_int(0);
    }
    if (ident_equals_ci(p, start, len, "yes", 3)) {
      return make_int(1);
    }
    if (ident_equals_ci(p, start, len, "no", 2)) {
      return make_int(0);
    }
    if (ident_equals_ci(p, start, len, "on", 2)) {
      return make_int(1);
    }
    if (ident_equals_ci(p, start, len, "off", 3)) {
      return make_int(0);
    }

    // Unknown identifier - error
    FeatherObj word = p->ops->string.slice(p->interp, p->expr_obj, start, start + len);
    set_bareword_error_obj(p, word);
    return make_error();
  }

  // Unexpected close paren
  if (c == ')') {
    set_close_paren_error(p);
    return make_error();
  }

  // Unknown token
  size_t start = p->pos;
  while (p->pos < p->len && !is_alnum(CUR_BYTE(p)) &&
         CUR_BYTE(p) != ' ' && CUR_BYTE(p) != '\t' && CUR_BYTE(p) != '\n' &&
         CUR_BYTE(p) != '(' && CUR_BYTE(p) != ')' && CUR_BYTE(p) != '[' && CUR_BYTE(p) != ']') {
    p->pos++;
  }
  size_t len = p->pos - start;
  if (len == 0) len = 1;
  FeatherObj token = p->ops->string.slice(p->interp, p->expr_obj, start, start + len);
  set_integer_error_obj(p, token);
  return make_error();
}

// Check if next char starts a number (digit or decimal point followed by digit)
static int is_number_start(ExprParser *p) {
  if (AT_END(p)) return 0;
  int c = CUR_BYTE(p);
  if (c >= '0' && c <= '9') return 1;
  if (c == '.' && p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) >= '0' && BYTE_AT(p, p->pos + 1) <= '9') return 1;
  return 0;
}

// Parse unary: - + ~ ! followed by unary
static ExprValue parse_unary(ExprParser *p) {
  expr_skip_whitespace(p);
  if (p->has_error) return make_error();

  if (p->pos < p->len) {
    int c = CUR_BYTE(p);

    // Unary minus (but not if followed by number - that's a negative number literal)
    if (c == '-') {
      size_t saved = p->pos;
      p->pos++;
      if (is_number_start(p)) {
        p->pos = saved;  // Let parse_primary handle it as a negative number
        return parse_primary(p);
      }
      ExprValue v = parse_unary(p);
      if (p->has_error) return make_error();
      // Try integer first, fall back to double
      int64_t ival;
      if (v.is_int && get_int(p, &v, &ival)) {
        return make_int(-ival);
      }
      double dval;
      if (get_double(p, &v, &dval)) {
        return make_double(-dval);
      }
      FeatherObj obj = get_obj(p, &v);
      set_integer_error_obj(p, obj);
      return make_error();
    }

    // Unary plus
    if (c == '+') {
      size_t saved = p->pos;
      p->pos++;
      if (is_number_start(p)) {
        p->pos = saved;
        return parse_primary(p);
      }
      return parse_unary(p);
    }

    // Bitwise NOT (integer only)
    if (c == '~') {
      p->pos++;
      ExprValue v = parse_unary(p);
      if (p->has_error) return make_error();
      int64_t val;
      if (!get_int(p, &v, &val)) {
        FeatherObj obj = get_obj(p, &v);
        set_integer_error_obj(p, obj);
        return make_error();
      }
      return make_int(~val);
    }

    // Logical NOT
    if (c == '!') {
      p->pos++;
      ExprValue v = parse_unary(p);
      if (p->has_error) return make_error();
      // Try integer first
      int64_t ival;
      if (get_int(p, &v, &ival)) {
        return make_int(ival ? 0 : 1);
      }
      // Try double
      double dval;
      if (get_double(p, &v, &dval)) {
        return make_int(dval != 0.0 ? 0 : 1);
      }
      FeatherObj obj = get_obj(p, &v);
      set_integer_error_obj(p, obj);
      return make_error();
    }
  }

  return parse_primary(p);
}

// Check if we need floating-point math (either operand is a float)
static int needs_float_math(ExprValue *a, ExprValue *b) {
  return (a->is_double && !a->is_int) || (b->is_double && !b->is_int);
}

// Parse exponentiation: unary ** exponentiation (right-to-left)
static ExprValue parse_exponentiation(ExprParser *p) {
  ExprValue left = parse_unary(p);
  if (p->has_error) return make_error();

  expr_skip_whitespace(p);
  if (p->pos + 1 < p->len && CUR_BYTE(p) == '*' && BYTE_AT(p, p->pos + 1) == '*') {
    p->pos += 2;
    ExprValue right = parse_exponentiation(p); // right-to-left
    if (p->has_error) return make_error();

    // Use floating-point if either operand is a float
    if (needs_float_math(&left, &right)) {
      double base, exp;
      if (!get_double(p, &left, &base) || !get_double(p, &right, &exp)) {
        set_syntax_error(p);
        return make_error();
      }
      // Use host pow for proper handling of all cases (including fractional exponents)
      double result;
      if (p->ops->dbl.math(p->interp, FEATHER_MATH_POW, base, exp, &result) != TCL_OK) {
        p->has_error = 1;
        p->error_msg = p->ops->interp.get_result(p->interp);
        return make_error();
      }
      return make_double(result);
    }

    int64_t base, exp;
    if (!get_int(p, &left, &base) || !get_int(p, &right, &exp)) {
      // Fall back to double if int conversion fails
      double dbase, dexp;
      if (!get_double(p, &left, &dbase) || !get_double(p, &right, &dexp)) {
        set_syntax_error(p);
        return make_error();
      }
      // Use host pow for proper handling
      double result;
      if (p->ops->dbl.math(p->interp, FEATHER_MATH_POW, dbase, dexp, &result) != TCL_OK) {
        p->has_error = 1;
        p->error_msg = p->ops->interp.get_result(p->interp);
        return make_error();
      }
      return make_double(result);
    }

    // Simple integer exponentiation
    int64_t result = 1;
    int neg = exp < 0;
    if (neg) exp = -exp;
    for (int64_t i = 0; i < exp; i++) {
      result *= base;
    }
    // Negative exponent gives 0 for integer math (truncation)
    if (neg && base != 1 && base != -1) result = 0;
    return make_int(result);
  }

  return left;
}

// Parse multiplicative: exponentiation (* / %) exponentiation
static ExprValue parse_multiplicative(ExprParser *p) {
  ExprValue left = parse_exponentiation(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    int c = CUR_BYTE(p);
    // Check for ** which is exponentiation, not multiplication
    if (c == '*' && p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) == '*') break;

    if (c == '*') {
      p->pos++;
      ExprValue right = parse_exponentiation(p);
      if (p->has_error) return make_error();
      // Use float math if either operand is a float
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (!get_double(p, &left, &lv) || !get_double(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        // NaN can occur from Inf * 0
        left = make_double_checked(p, lv * rv);
        if (p->has_error) return make_error();
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          // NaN can occur from Inf * 0
          left = make_double_checked(p, dlv * drv);
          if (p->has_error) return make_error();
        } else {
          left = make_int(lv * rv);
        }
      }
    } else if (c == '/') {
      p->pos++;
      ExprValue right = parse_exponentiation(p);
      if (p->has_error) return make_error();
      // Use float math if either operand is a float
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (!get_double(p, &left, &lv) || !get_double(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        // IEEE 754: float division by zero produces Inf, -Inf, or NaN
        // NaN is treated as a domain error in TCL
        left = make_double_checked(p, lv / rv);
        if (p->has_error) return make_error();
      } else {
        // Integer division
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          // IEEE 754: float division by zero produces Inf, -Inf, or NaN
          // NaN is treated as a domain error in TCL
          left = make_double_checked(p, dlv / drv);
          if (p->has_error) return make_error();
        } else {
          if (rv == 0) {
            set_error(p, "divide by zero", 14);
            return make_error();
          }
          left = make_int(lv / rv);
        }
      }
    } else if (c == '%') {
      p->pos++;
      ExprValue right = parse_exponentiation(p);
      if (p->has_error) return make_error();
      // Modulo is always integer in TCL
      int64_t lv, rv;
      if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
        set_syntax_error(p);
        return make_error();
      }
      if (rv == 0) {
        set_error(p, "divide by zero", 14);
        return make_error();
      }
      left = make_int(lv % rv);
    } else {
      break;
    }
  }

  return left;
}

// Parse additive: multiplicative (+ -) multiplicative
static ExprValue parse_additive(ExprParser *p) {
  ExprValue left = parse_multiplicative(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    int c = CUR_BYTE(p);
    if (c == '+') {
      p->pos++;
      ExprValue right = parse_multiplicative(p);
      if (p->has_error) return make_error();
      // Use float math if either operand is a float
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (!get_double(p, &left, &lv) || !get_double(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        // NaN can occur from Inf + (-Inf)
        left = make_double_checked(p, lv + rv);
        if (p->has_error) return make_error();
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          // NaN can occur from Inf + (-Inf)
          left = make_double_checked(p, dlv + drv);
          if (p->has_error) return make_error();
        } else {
          left = make_int(lv + rv);
        }
      }
    } else if (c == '-') {
      p->pos++;
      ExprValue right = parse_multiplicative(p);
      if (p->has_error) return make_error();
      // Use float math if either operand is a float
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (!get_double(p, &left, &lv) || !get_double(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        // NaN can occur from Inf - Inf
        left = make_double_checked(p, lv - rv);
        if (p->has_error) return make_error();
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          // NaN can occur from Inf - Inf
          left = make_double_checked(p, dlv - drv);
          if (p->has_error) return make_error();
        } else {
          left = make_int(lv - rv);
        }
      }
    } else {
      break;
    }
  }

  return left;
}

// Parse shift: additive (<< >>) additive
static ExprValue parse_shift(ExprParser *p) {
  ExprValue left = parse_additive(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    // Left shift <<
    if (p->pos + 1 < p->len && CUR_BYTE(p) == '<' && BYTE_AT(p, p->pos + 1) == '<') {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      int64_t lv, rv;
      if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
        set_syntax_error(p);
        return make_error();
      }
      left = make_int(lv << rv);
    }
    // Right shift >>
    else if (p->pos + 1 < p->len && CUR_BYTE(p) == '>' && BYTE_AT(p, p->pos + 1) == '>') {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      int64_t lv, rv;
      if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
        set_syntax_error(p);
        return make_error();
      }
      left = make_int(lv >> rv);
    } else {
      break;
    }
  }

  return left;
}

// Parse comparison: shift (< <= > >= lt le gt ge) shift
// Numeric-preferring: < <= > >= try int first, fall back to string
// String-only: lt le gt ge always use string compare
static ExprValue parse_comparison(ExprParser *p) {
  ExprValue left = parse_shift(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    int c = CUR_BYTE(p);

    // String comparison operators: lt, le, gt, ge
    if (match_keyword(p, "lt", 2)) {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      FeatherObj lo = get_obj(p, &left);
      FeatherObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp < 0 ? 1 : 0);
    } else if (match_keyword(p, "le", 2)) {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      FeatherObj lo = get_obj(p, &left);
      FeatherObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp <= 0 ? 1 : 0);
    } else if (match_keyword(p, "gt", 2)) {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      FeatherObj lo = get_obj(p, &left);
      FeatherObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp > 0 ? 1 : 0);
    } else if (match_keyword(p, "ge", 2)) {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      FeatherObj lo = get_obj(p, &left);
      FeatherObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp >= 0 ? 1 : 0);
    }
    // Numeric-preferring comparison operators
    // Check for <= (but not <<)
    else if (c == '<' && p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) == '=') {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv <= rv ? 1 : 0);
        } else {
          FeatherObj lo = get_obj(p, &left);
          FeatherObj ro = get_obj(p, &right);
          int cmp = p->ops->string.compare(p->interp, lo, ro);
          left = make_int(cmp <= 0 ? 1 : 0);
        }
      } else {
        int64_t lv, rv;
        if (get_int(p, &left, &lv) && get_int(p, &right, &rv)) {
          left = make_int(lv <= rv ? 1 : 0);
        } else {
          double dlv, drv;
          if (get_double(p, &left, &dlv) && get_double(p, &right, &drv)) {
            left = make_int(dlv <= drv ? 1 : 0);
          } else {
            FeatherObj lo = get_obj(p, &left);
            FeatherObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp <= 0 ? 1 : 0);
          }
        }
      }
    // Single < but not << (shift is handled by parse_shift)
    } else if (c == '<' && !(p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) == '<')) {
      p->pos++;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv < rv ? 1 : 0);
        } else {
          FeatherObj lo = get_obj(p, &left);
          FeatherObj ro = get_obj(p, &right);
          int cmp = p->ops->string.compare(p->interp, lo, ro);
          left = make_int(cmp < 0 ? 1 : 0);
        }
      } else {
        int64_t lv, rv;
        if (get_int(p, &left, &lv) && get_int(p, &right, &rv)) {
          left = make_int(lv < rv ? 1 : 0);
        } else {
          double dlv, drv;
          if (get_double(p, &left, &dlv) && get_double(p, &right, &drv)) {
            left = make_int(dlv < drv ? 1 : 0);
          } else {
            FeatherObj lo = get_obj(p, &left);
            FeatherObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp < 0 ? 1 : 0);
          }
        }
      }
    // Check for >= (but not >>)
    } else if (c == '>' && p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) == '=') {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv >= rv ? 1 : 0);
        } else {
          FeatherObj lo = get_obj(p, &left);
          FeatherObj ro = get_obj(p, &right);
          int cmp = p->ops->string.compare(p->interp, lo, ro);
          left = make_int(cmp >= 0 ? 1 : 0);
        }
      } else {
        int64_t lv, rv;
        if (get_int(p, &left, &lv) && get_int(p, &right, &rv)) {
          left = make_int(lv >= rv ? 1 : 0);
        } else {
          double dlv, drv;
          if (get_double(p, &left, &dlv) && get_double(p, &right, &drv)) {
            left = make_int(dlv >= drv ? 1 : 0);
          } else {
            FeatherObj lo = get_obj(p, &left);
            FeatherObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp >= 0 ? 1 : 0);
          }
        }
      }
    // Single > but not >> (shift is handled by parse_shift)
    } else if (c == '>' && !(p->pos + 1 < p->len && BYTE_AT(p, p->pos + 1) == '>')) {
      p->pos++;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv > rv ? 1 : 0);
        } else {
          FeatherObj lo = get_obj(p, &left);
          FeatherObj ro = get_obj(p, &right);
          int cmp = p->ops->string.compare(p->interp, lo, ro);
          left = make_int(cmp > 0 ? 1 : 0);
        }
      } else {
        int64_t lv, rv;
        if (get_int(p, &left, &lv) && get_int(p, &right, &rv)) {
          left = make_int(lv > rv ? 1 : 0);
        } else {
          double dlv, drv;
          if (get_double(p, &left, &dlv) && get_double(p, &right, &drv)) {
            left = make_int(dlv > drv ? 1 : 0);
          } else {
            FeatherObj lo = get_obj(p, &left);
            FeatherObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp > 0 ? 1 : 0);
          }
        }
      }
    }
    // List containment operators: in, ni
    else if (match_keyword(p, "in", 2)) {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      FeatherObj needle = get_obj(p, &left);
      FeatherObj haystack = get_obj(p, &right);
      // Convert haystack to list and search
      FeatherObj list = p->ops->list.from(p->interp, haystack);
      size_t len = p->ops->list.length(p->interp, list);
      int found = 0;
      for (size_t i = 0; i < len; i++) {
        FeatherObj elem = p->ops->list.at(p->interp, list, i);
        if (p->ops->string.compare(p->interp, needle, elem) == 0) {
          found = 1;
          break;
        }
      }
      left = make_int(found);
    } else if (match_keyword(p, "ni", 2)) {
      p->pos += 2;
      ExprValue right = parse_shift(p);
      if (p->has_error) return make_error();
      FeatherObj needle = get_obj(p, &left);
      FeatherObj haystack = get_obj(p, &right);
      // Convert haystack to list and search
      FeatherObj list = p->ops->list.from(p->interp, haystack);
      size_t len = p->ops->list.length(p->interp, list);
      int found = 0;
      for (size_t i = 0; i < len; i++) {
        FeatherObj elem = p->ops->list.at(p->interp, list, i);
        if (p->ops->string.compare(p->interp, needle, elem) == 0) {
          found = 1;
          break;
        }
      }
      left = make_int(found ? 0 : 1);  // ni is opposite of in
    } else {
      break;
    }
  }

  return left;
}

// Parse equality: comparison (== != eq ne) comparison
static ExprValue parse_equality(ExprParser *p) {
  ExprValue left = parse_comparison(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    // String equality operators: eq, ne
    if (match_keyword(p, "eq", 2)) {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      FeatherObj lo = get_obj(p, &left);
      FeatherObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp == 0 ? 1 : 0);
    } else if (match_keyword(p, "ne", 2)) {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      FeatherObj lo = get_obj(p, &left);
      FeatherObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp != 0 ? 1 : 0);
    }
    // Numeric equality operators (with string fallback)
    else if (p->pos + 1 < p->len && CUR_BYTE(p) == '=' && BYTE_AT(p, p->pos + 1) == '=') {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv == rv ? 1 : 0);
        } else {
          // Fall back to string comparison
          FeatherObj lo = get_obj(p, &left);
          FeatherObj ro = get_obj(p, &right);
          int cmp = p->ops->string.compare(p->interp, lo, ro);
          left = make_int(cmp == 0 ? 1 : 0);
        }
      } else {
        int64_t lv, rv;
        if (get_int(p, &left, &lv) && get_int(p, &right, &rv)) {
          left = make_int(lv == rv ? 1 : 0);
        } else {
          // Try double
          double dlv, drv;
          if (get_double(p, &left, &dlv) && get_double(p, &right, &drv)) {
            left = make_int(dlv == drv ? 1 : 0);
          } else {
            // Fall back to string comparison
            FeatherObj lo = get_obj(p, &left);
            FeatherObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp == 0 ? 1 : 0);
          }
        }
      }
    } else if (p->pos + 1 < p->len && CUR_BYTE(p) == '!' && BYTE_AT(p, p->pos + 1) == '=') {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv != rv ? 1 : 0);
        } else {
          // Fall back to string comparison
          FeatherObj lo = get_obj(p, &left);
          FeatherObj ro = get_obj(p, &right);
          int cmp = p->ops->string.compare(p->interp, lo, ro);
          left = make_int(cmp != 0 ? 1 : 0);
        }
      } else {
        int64_t lv, rv;
        if (get_int(p, &left, &lv) && get_int(p, &right, &rv)) {
          left = make_int(lv != rv ? 1 : 0);
        } else {
          // Try double
          double dlv, drv;
          if (get_double(p, &left, &dlv) && get_double(p, &right, &drv)) {
            left = make_int(dlv != drv ? 1 : 0);
          } else {
            // Fall back to string comparison
            FeatherObj lo = get_obj(p, &left);
            FeatherObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp != 0 ? 1 : 0);
          }
        }
      }
    } else {
      break;
    }
  }

  return left;
}

// Parse bitwise AND: equality & equality
static ExprValue parse_bitwise_and(ExprParser *p) {
  ExprValue left = parse_equality(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    // Single & (not &&)
    if (CUR_BYTE(p) == '&' && (p->pos + 1 >= p->len || BYTE_AT(p, p->pos + 1) != '&')) {
      p->pos++;
      ExprValue right = parse_equality(p);
      if (p->has_error) return make_error();
      int64_t lv, rv;
      if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
        set_syntax_error(p);
        return make_error();
      }
      left = make_int(lv & rv);
    } else {
      break;
    }
  }

  return left;
}

// Parse bitwise XOR: bitwise_and ^ bitwise_and
static ExprValue parse_bitwise_xor(ExprParser *p) {
  ExprValue left = parse_bitwise_and(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    if (CUR_BYTE(p) == '^') {
      p->pos++;
      ExprValue right = parse_bitwise_and(p);
      if (p->has_error) return make_error();
      int64_t lv, rv;
      if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
        set_syntax_error(p);
        return make_error();
      }
      left = make_int(lv ^ rv);
    } else {
      break;
    }
  }

  return left;
}

// Parse bitwise OR: bitwise_xor | bitwise_xor
static ExprValue parse_bitwise_or(ExprParser *p) {
  ExprValue left = parse_bitwise_xor(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    // Single | (not ||)
    if (CUR_BYTE(p) == '|' && (p->pos + 1 >= p->len || BYTE_AT(p, p->pos + 1) != '|')) {
      p->pos++;
      ExprValue right = parse_bitwise_xor(p);
      if (p->has_error) return make_error();
      int64_t lv, rv;
      if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
        set_syntax_error(p);
        return make_error();
      }
      left = make_int(lv | rv);
    } else {
      break;
    }
  }

  return left;
}

// Parse logical AND: bitwise_or && bitwise_or (short-circuit)
static ExprValue parse_logical_and(ExprParser *p) {
  ExprValue left = parse_bitwise_or(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    if (p->pos + 1 < p->len && CUR_BYTE(p) == '&' && BYTE_AT(p, p->pos + 1) == '&') {
      p->pos += 2;
      int64_t lv;
      if (!get_int(p, &left, &lv)) {
        set_syntax_error(p);
        return make_error();
      }
      // Short-circuit: if left is false, don't evaluate right
      if (!lv) {
        // Skip parsing the right operand (no side effects)
        int saved_skip = p->skip_mode;
        p->skip_mode = 1;
        parse_bitwise_or(p);
        p->skip_mode = saved_skip;
        if (p->has_error) return make_error();
        left = make_int(0);
      } else {
        ExprValue right = parse_bitwise_or(p);
        if (p->has_error) return make_error();
        int64_t rv;
        if (!get_int(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        left = make_int(rv ? 1 : 0);
      }
    } else {
      break;
    }
  }

  return left;
}

// Parse logical OR: logical_and || logical_and (short-circuit)
static ExprValue parse_logical_or(ExprParser *p) {
  ExprValue left = parse_logical_and(p);
  if (p->has_error) return make_error();

  while (1) {
    expr_skip_whitespace(p);
    if (AT_END(p)) break;

    if (p->pos + 1 < p->len && CUR_BYTE(p) == '|' && BYTE_AT(p, p->pos + 1) == '|') {
      p->pos += 2;
      int64_t lv;
      if (!get_int(p, &left, &lv)) {
        set_syntax_error(p);
        return make_error();
      }
      // Short-circuit: if left is true, don't evaluate right
      if (lv) {
        // Skip parsing the right operand (no side effects)
        int saved_skip = p->skip_mode;
        p->skip_mode = 1;
        parse_logical_and(p);
        p->skip_mode = saved_skip;
        if (p->has_error) return make_error();
        left = make_int(1);
      } else {
        ExprValue right = parse_logical_and(p);
        if (p->has_error) return make_error();
        int64_t rv;
        if (!get_int(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        left = make_int(rv ? 1 : 0);
      }
    } else {
      break;
    }
  }

  return left;
}

// Parse ternary: logical_or ? expr : expr (right-to-left)
static ExprValue parse_ternary(ExprParser *p) {
  ExprValue cond = parse_logical_or(p);
  if (p->has_error) return make_error();

  expr_skip_whitespace(p);
  if (p->pos < p->len && CUR_BYTE(p) == '?') {
    p->pos++;
    int64_t cv;
    if (!get_int(p, &cond, &cv)) {
      set_syntax_error(p);
      return make_error();
    }

    ExprValue result;
    int saved_skip = p->skip_mode;

    if (cv) {
      // Condition is true: evaluate then branch, skip else branch
      result = parse_ternary(p);
      if (p->has_error) return make_error();

      expr_skip_whitespace(p);
      if (AT_END(p) || CUR_BYTE(p) != ':') {
        set_syntax_error(p);
        return make_error();
      }
      p->pos++;

      p->skip_mode = 1;
      parse_ternary(p);
      p->skip_mode = saved_skip;
      if (p->has_error) return make_error();
    } else {
      // Condition is false: skip then branch, evaluate else branch
      p->skip_mode = 1;
      parse_ternary(p);
      p->skip_mode = saved_skip;
      if (p->has_error) return make_error();

      expr_skip_whitespace(p);
      if (AT_END(p) || CUR_BYTE(p) != ':') {
        set_syntax_error(p);
        return make_error();
      }
      p->pos++;

      result = parse_ternary(p);
      if (p->has_error) return make_error();
    }

    return result;
  }

  return cond;
}

FeatherResult feather_builtin_expr(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"expr arg ?arg ...?\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Concatenate all arguments with spaces
  FeatherObj expr_obj = ops->list.shift(interp, args);
  argc--;

  while (argc > 0) {
    FeatherObj space = ops->string.intern(interp, " ", 1);
    FeatherObj next = ops->list.shift(interp, args);
    expr_obj = ops->string.concat(interp, expr_obj, space);
    expr_obj = ops->string.concat(interp, expr_obj, next);
    argc--;
  }

  // Initialize parser with position-based iteration (no string.get needed)
  ExprParser parser = {
    .ops = ops,
    .interp = interp,
    .expr_obj = expr_obj,
    .len = ops->string.byte_length(interp, expr_obj),
    .pos = 0,
    .has_error = 0,
    .error_msg = 0,
    .skip_mode = 0
  };

  // Parse and evaluate
  ExprValue result = parse_ternary(&parser);

  // Check for trailing content
  expr_skip_whitespace(&parser);
  if (!parser.has_error && parser.pos < parser.len) {
    if (CUR_BYTE(&parser) == ')') {
      set_close_paren_error(&parser);
    } else {
      set_syntax_error(&parser);
    }
  }

  if (parser.has_error) {
    ops->interp.set_result(interp, parser.error_msg);
    return TCL_ERROR;
  }

  // Check for NaN result - TCL expr errors on NaN (unless checked by isnan)
  if (result.is_double && ops->dbl.classify(result.dbl_val) == FEATHER_DBL_NAN) {
    FeatherObj msg = ops->string.intern(interp, "domain error: argument not in valid range", 41);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Return result
  FeatherObj result_obj = get_obj(&parser, &result);
  ops->interp.set_result(interp, result_obj);
  return TCL_OK;
}

void feather_register_expr_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Evaluate mathematical expression",
    "Concatenates all arguments with spaces, parses the result as a mathematical "
    "expression, and returns the computed value.\n\n"
    "Supports arithmetic operations (+, -, *, /, %, **), comparison operators "
    "(<, >, <=, >=, ==, !=, eq, ne, lt, le, gt, ge), logical operators "
    "(&&, ||, !, ?:), bitwise operators (&, |, ^, ~, <<, >>), list membership "
    "(in, ni), variable substitution ($var, ${var}), command substitution ([cmd]), "
    "and math functions via tcl::mathfunc:: namespace.\n\n"
    "Operands can be integers (decimal, hex 0x, binary 0b, octal 0o), "
    "floating-point numbers (3.14, 1e10, .5), boolean literals "
    "(true, false, yes, no, on, off), variables, command results, "
    "or parenthesized subexpressions. Comments starting with # are supported.\n\n"
    "Math functions include: abs, acos, asin, atan, atan2, bool, ceil, cos, cosh, "
    "double, entier, exp, floor, fmod, hypot, int, isfinite, isinf, isnan, "
    "isnormal, issubnormal, isunordered, log, log10, max, min, pow, round, sin, "
    "sinh, sqrt, tan, tanh, wide. Use as: expr {funcname(args)}.\n\n"
    "Short-circuit evaluation applies to &&, ||, and ?:. Integer division "
    "truncates toward zero (may differ from TCL which uses floor division). "
    "NaN results produce domain errors.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Expression component");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e, "Additional expression components (concatenated with spaces)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "expr {2 + 2}",
    "Simple arithmetic",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "expr {$x > 10 ? \"big\" : \"small\"}",
    "Ternary conditional operator",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "expr {5 in {1 3 5 7}}",
    "List membership test",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "expr {sqrt(pow($a, 2) + pow($b, 2))}",
    "Math functions for Pythagorean theorem",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "expr {0xff & 0b1111}",
    "Bitwise AND with hex and binary literals",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "expr", spec);
}
