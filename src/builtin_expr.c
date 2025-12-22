#include "tclc.h"

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
 */

// ExprValue can be an integer, double, or string (TclObj)
typedef struct {
  int64_t int_val;
  double dbl_val;
  TclObj str_val;      // 0 means no string value
  int is_int;          // 1 if has valid integer rep
  int is_double;       // 1 if has valid double rep
} ExprValue;

typedef struct {
  const TclHostOps *ops;
  TclInterp interp;
  const char *expr;    // Original expression string (for error messages)
  size_t expr_len;
  const char *pos;     // Current position
  const char *end;     // End of expression
  int has_error;
  TclObj error_msg;
  int skip_mode;       // When true, skip evaluation (for lazy eval)
} ExprParser;

// Forward declarations
static ExprValue parse_ternary(ExprParser *p);
static ExprValue parse_logical_or(ExprParser *p);
static ExprValue parse_logical_and(ExprParser *p);
static ExprValue parse_bitwise_or(ExprParser *p);
static ExprValue parse_bitwise_xor(ExprParser *p);
static ExprValue parse_bitwise_and(ExprParser *p);
static ExprValue parse_equality(ExprParser *p);
static ExprValue parse_comparison(ExprParser *p);
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
static ExprValue make_str(TclObj obj) {
  ExprValue v = {.int_val = 0, .dbl_val = 0, .str_val = obj, .is_int = 0, .is_double = 0};
  return v;
}

// Create an error ExprValue (signals error without value)
static ExprValue make_error(void) {
  ExprValue v = {.int_val = 0, .dbl_val = 0, .str_val = 0, .is_int = 0, .is_double = 0};
  return v;
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

// Get TclObj from ExprValue
static TclObj get_obj(ExprParser *p, ExprValue *v) {
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

static void skip_whitespace(ExprParser *p) {
  while (p->pos < p->end) {
    char c = *p->pos;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      p->pos++;
    } else if (c == '#') {
      // Comment - skip to end of line or expression
      while (p->pos < p->end && *p->pos != '\n') {
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

  TclObj part1 = p->ops->string.intern(p->interp, "syntax error in expression \"", 28);
  TclObj part2 = p->ops->string.intern(p->interp, p->expr, p->expr_len);
  TclObj part3 = p->ops->string.intern(p->interp, "\"", 1);

  TclObj msg = p->ops->string.concat(p->interp, part1, part2);
  msg = p->ops->string.concat(p->interp, msg, part3);
  p->error_msg = msg;
}

static void set_integer_error(ExprParser *p, const char *start, size_t len) {
  if (p->has_error) return;
  p->has_error = 1;

  TclObj part1 = p->ops->string.intern(p->interp, "expected integer but got \"", 26);
  TclObj part2 = p->ops->string.intern(p->interp, start, len);
  TclObj part3 = p->ops->string.intern(p->interp, "\"", 1);

  TclObj msg = p->ops->string.concat(p->interp, part1, part2);
  msg = p->ops->string.concat(p->interp, msg, part3);
  p->error_msg = msg;
}

static void set_paren_error(ExprParser *p) {
  if (p->has_error) return;
  p->has_error = 1;

  TclObj part1 = p->ops->string.intern(p->interp, "unbalanced parentheses in expression \"", 38);
  TclObj part2 = p->ops->string.intern(p->interp, p->expr, p->expr_len);
  TclObj part3 = p->ops->string.intern(p->interp, "\"", 1);

  TclObj msg = p->ops->string.concat(p->interp, part1, part2);
  msg = p->ops->string.concat(p->interp, msg, part3);
  p->error_msg = msg;
}

static void set_close_paren_error(ExprParser *p) {
  if (p->has_error) return;
  p->has_error = 1;
  p->error_msg = p->ops->string.intern(p->interp, "unbalanced close paren", 22);
}

// Check if character is alphanumeric or underscore
static int is_alnum(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

// Check if we're at a word boundary for keyword matching
static int match_keyword(ExprParser *p, const char *kw, size_t len) {
  if ((size_t)(p->end - p->pos) < len) return 0;
  for (size_t i = 0; i < len; i++) {
    char c = p->pos[i];
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c != kw[i]) return 0;
  }
  // Ensure not followed by alphanumeric
  if (p->pos + len < p->end && is_alnum(p->pos[len])) return 0;
  return 1;
}

// Parse a variable reference $name or ${name}
static ExprValue parse_variable(ExprParser *p) {
  p->pos++; // skip $

  if (p->pos >= p->end) {
    set_syntax_error(p);
    return make_error();
  }

  const char *name_start;
  size_t name_len;

  if (*p->pos == '{') {
    p->pos++;
    name_start = p->pos;
    while (p->pos < p->end && *p->pos != '}') {
      p->pos++;
    }
    if (p->pos >= p->end) {
      set_syntax_error(p);
      return make_error();
    }
    name_len = p->pos - name_start;
    p->pos++;
  } else {
    name_start = p->pos;
    while (p->pos < p->end && is_alnum(*p->pos)) {
      p->pos++;
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

  TclObj name = p->ops->string.intern(p->interp, name_start, name_len);
  TclObj value = p->ops->var.get(p->interp, name);

  if (p->ops->list.is_nil(p->interp, value)) {
    TclObj part1 = p->ops->string.intern(p->interp, "can't read \"", 12);
    TclObj part3 = p->ops->string.intern(p->interp, "\": no such variable", 19);
    TclObj msg = p->ops->string.concat(p->interp, part1, name);
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

  const char *cmd_start = p->pos;
  int depth = 1;

  while (p->pos < p->end && depth > 0) {
    if (*p->pos == '[') {
      depth++;
    } else if (*p->pos == ']') {
      depth--;
    } else if (*p->pos == '\\' && p->pos + 1 < p->end) {
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

  // Use tcl_eval_string to evaluate the command
  TclResult result = tcl_eval_string(p->ops, p->interp, cmd_start, cmd_len, TCL_EVAL_LOCAL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  return make_str(p->ops->interp.get_result(p->interp));
}

// Parse braced string {...}
static ExprValue parse_braced(ExprParser *p) {
  p->pos++; // skip {
  const char *start = p->pos;
  int depth = 1;

  while (p->pos < p->end && depth > 0) {
    if (*p->pos == '{') depth++;
    else if (*p->pos == '}') depth--;
    p->pos++;
  }

  if (depth != 0) {
    set_syntax_error(p);
    return make_error();
  }

  size_t len = p->pos - start - 1;
  TclObj str = p->ops->string.intern(p->interp, start, len);
  return make_str(str);
}

// Parse quoted string "..." with variable and command substitution
static ExprValue parse_quoted(ExprParser *p) {
  p->pos++; // skip "
  const char *start = p->pos;

  // Find the closing quote, handling backslash escapes
  while (p->pos < p->end && *p->pos != '"') {
    if (*p->pos == '\\' && p->pos + 1 < p->end) {
      p->pos += 2;  // skip backslash and following char
    } else {
      p->pos++;
    }
  }

  if (p->pos >= p->end) {
    set_syntax_error(p);
    return make_error();
  }

  size_t len = p->pos - start;
  p->pos++; // skip closing "

  // In skip mode, just return a dummy value without evaluating
  if (p->skip_mode) {
    return make_int(0);
  }

  // Perform substitutions on the quoted content
  TclResult result = tcl_subst(p->ops, p->interp, start, len, TCL_SUBST_ALL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  return make_str(p->ops->interp.get_result(p->interp));
}

// Parse number literal (integer or floating-point)
// Integers: 123, 0x1f, 0b101, 0o17, with optional underscores
// Floats: 3.14, .5, 5., 1e10, 3.14e-5
static ExprValue parse_number(ExprParser *p) {
  const char *start = p->pos;
  int negative = 0;

  if (*p->pos == '-') {
    negative = 1;
    p->pos++;
  } else if (*p->pos == '+') {
    p->pos++;
  }

  // Handle leading decimal point: .5
  if (p->pos < p->end && *p->pos == '.') {
    p->pos++;
    if (p->pos >= p->end || *p->pos < '0' || *p->pos > '9') {
      set_integer_error(p, start, p->pos - start > 0 ? p->pos - start : 1);
      return make_error();
    }
    // Parse fractional digits
    double frac = 0.0;
    double place = 0.1;
    while (p->pos < p->end && ((*p->pos >= '0' && *p->pos <= '9') || *p->pos == '_')) {
      if (*p->pos == '_') { p->pos++; continue; }
      frac += (*p->pos - '0') * place;
      place *= 0.1;
      p->pos++;
    }
    double result = frac;
    // Check for exponent
    if (p->pos < p->end && (*p->pos == 'e' || *p->pos == 'E')) {
      p->pos++;
      int exp_neg = 0;
      if (p->pos < p->end && *p->pos == '-') { exp_neg = 1; p->pos++; }
      else if (p->pos < p->end && *p->pos == '+') { p->pos++; }
      int64_t exp = 0;
      while (p->pos < p->end && *p->pos >= '0' && *p->pos <= '9') {
        exp = exp * 10 + (*p->pos - '0');
        p->pos++;
      }
      double mult = 1.0;
      for (int64_t i = 0; i < exp; i++) mult *= 10.0;
      if (exp_neg) result /= mult; else result *= mult;
    }
    return make_double(negative ? -result : result);
  }

  if (p->pos >= p->end || (*p->pos < '0' || *p->pos > '9')) {
    set_integer_error(p, start, p->pos - start > 0 ? p->pos - start : 1);
    return make_error();
  }

  int base = 10;
  int is_float = 0;

  // Check for radix prefix (only for integers)
  if (*p->pos == '0' && p->pos + 1 < p->end) {
    char next = p->pos[1];
    if (next == 'x' || next == 'X') {
      base = 16;
      p->pos += 2;
    } else if (next == 'b' || next == 'B') {
      base = 2;
      p->pos += 2;
    } else if (next == 'o' || next == 'O') {
      base = 8;
      p->pos += 2;
    }
  }

  // Parse integer part
  int64_t int_value = 0;
  while (p->pos < p->end) {
    char c = *p->pos;
    if (c == '_') { p->pos++; continue; }
    int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    if (digit < 0 || digit >= base) break;
    int_value = int_value * base + digit;
    p->pos++;
  }

  // Check for decimal point (only base 10)
  if (base == 10 && p->pos < p->end && *p->pos == '.') {
    // Look ahead to distinguish 5.0 from 5.method()
    if (p->pos + 1 < p->end && p->pos[1] >= '0' && p->pos[1] <= '9') {
      is_float = 1;
      p->pos++; // skip .
      double frac = 0.0;
      double place = 0.1;
      while (p->pos < p->end && ((*p->pos >= '0' && *p->pos <= '9') || *p->pos == '_')) {
        if (*p->pos == '_') { p->pos++; continue; }
        frac += (*p->pos - '0') * place;
        place *= 0.1;
        p->pos++;
      }
      double result = (double)int_value + frac;
      // Check for exponent
      if (p->pos < p->end && (*p->pos == 'e' || *p->pos == 'E')) {
        p->pos++;
        int exp_neg = 0;
        if (p->pos < p->end && *p->pos == '-') { exp_neg = 1; p->pos++; }
        else if (p->pos < p->end && *p->pos == '+') { p->pos++; }
        int64_t exp = 0;
        while (p->pos < p->end && *p->pos >= '0' && *p->pos <= '9') {
          exp = exp * 10 + (*p->pos - '0');
          p->pos++;
        }
        double mult = 1.0;
        for (int64_t i = 0; i < exp; i++) mult *= 10.0;
        if (exp_neg) result /= mult; else result *= mult;
      }
      return make_double(negative ? -result : result);
    }
  }

  // Check for exponent without decimal point (e.g., 1e10) - only base 10
  if (base == 10 && p->pos < p->end && (*p->pos == 'e' || *p->pos == 'E')) {
    is_float = 1;
    p->pos++;
    int exp_neg = 0;
    if (p->pos < p->end && *p->pos == '-') { exp_neg = 1; p->pos++; }
    else if (p->pos < p->end && *p->pos == '+') { p->pos++; }
    int64_t exp = 0;
    while (p->pos < p->end && *p->pos >= '0' && *p->pos <= '9') {
      exp = exp * 10 + (*p->pos - '0');
      p->pos++;
    }
    double result = (double)int_value;
    double mult = 1.0;
    for (int64_t i = 0; i < exp; i++) mult *= 10.0;
    if (exp_neg) result /= mult; else result *= mult;
    return make_double(negative ? -result : result);
  }

  return make_int(negative ? -int_value : int_value);
}

// Parse function call: funcname(arg, arg, ...)
static ExprValue parse_function_call(ExprParser *p, const char *name, size_t name_len) {
  p->pos++; // skip (

  // Build command: tcl::mathfunc::name arg arg ...
  TclObj prefix = p->ops->string.intern(p->interp, "tcl::mathfunc::", 15);
  TclObj func_name = p->ops->string.intern(p->interp, name, name_len);
  TclObj full_cmd = p->ops->string.concat(p->interp, prefix, func_name);

  TclObj args = p->ops->list.create(p->interp);

  // Parse arguments
  skip_whitespace(p);
  while (p->pos < p->end && *p->pos != ')') {
    ExprValue arg = parse_ternary(p);
    if (p->has_error) return make_error();

    // Only collect argument values when not in skip mode
    if (!p->skip_mode) {
      TclObj arg_obj = get_obj(p, &arg);
      args = p->ops->list.push(p->interp, args, arg_obj);
    }

    skip_whitespace(p);
    if (p->pos < p->end && *p->pos == ',') {
      p->pos++;
      skip_whitespace(p);
    }
  }

  if (p->pos >= p->end || *p->pos != ')') {
    set_paren_error(p);
    return make_error();
  }
  p->pos++; // skip )

  // In skip mode, just return a dummy value without evaluating
  if (p->skip_mode) {
    return make_int(0);
  }

  // Build the command string: "tcl::mathfunc::name arg1 arg2 ..."
  TclObj cmd_str = full_cmd;
  size_t argc = p->ops->list.length(p->interp, args);
  for (size_t i = 0; i < argc; i++) {
    TclObj space = p->ops->string.intern(p->interp, " ", 1);
    TclObj arg = p->ops->list.at(p->interp, args, i);
    cmd_str = p->ops->string.concat(p->interp, cmd_str, space);
    cmd_str = p->ops->string.concat(p->interp, cmd_str, arg);
  }

  // Evaluate the command
  size_t cmd_len;
  const char *cmd_cstr = p->ops->string.get(p->interp, cmd_str, &cmd_len);
  TclResult result = tcl_eval_string(p->ops, p->interp, cmd_cstr, cmd_len, TCL_EVAL_LOCAL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return make_error();
  }

  return make_str(p->ops->interp.get_result(p->interp));
}

// Parse primary: number, variable, command, boolean, braced/quoted string, paren, function call
static ExprValue parse_primary(ExprParser *p) {
  skip_whitespace(p);

  if (p->has_error || p->pos >= p->end) {
    if (!p->has_error) set_syntax_error(p);
    return make_error();
  }

  char c = *p->pos;

  // Parenthesized expression
  if (c == '(') {
    p->pos++;
    ExprValue val = parse_ternary(p);
    if (p->has_error) return make_error();
    skip_whitespace(p);
    if (p->pos >= p->end || *p->pos != ')') {
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
      ((c == '-' || c == '+') && p->pos + 1 < p->end &&
       (p->pos[1] >= '0' && p->pos[1] <= '9' || p->pos[1] == '.')) ||
      (c == '.' && p->pos + 1 < p->end && p->pos[1] >= '0' && p->pos[1] <= '9')) {
    return parse_number(p);
  }

  // Boolean literals and function names (identifiers)
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
    const char *start = p->pos;
    while (p->pos < p->end && is_alnum(*p->pos)) {
      p->pos++;
    }
    size_t len = p->pos - start;

    // Check for function call
    skip_whitespace(p);
    if (p->pos < p->end && *p->pos == '(') {
      return parse_function_call(p, start, len);
    }

    // Check for boolean literals
    if (len == 4 && (start[0] == 't' || start[0] == 'T')) {
      if ((start[1] == 'r' || start[1] == 'R') &&
          (start[2] == 'u' || start[2] == 'U') &&
          (start[3] == 'e' || start[3] == 'E')) {
        return make_int(1);
      }
    }
    if (len == 5 && (start[0] == 'f' || start[0] == 'F')) {
      if ((start[1] == 'a' || start[1] == 'A') &&
          (start[2] == 'l' || start[2] == 'L') &&
          (start[3] == 's' || start[3] == 'S') &&
          (start[4] == 'e' || start[4] == 'E')) {
        return make_int(0);
      }
    }
    if (len == 3 && (start[0] == 'y' || start[0] == 'Y')) {
      if ((start[1] == 'e' || start[1] == 'E') &&
          (start[2] == 's' || start[2] == 'S')) {
        return make_int(1);
      }
    }
    if (len == 2 && (start[0] == 'n' || start[0] == 'N')) {
      if (start[1] == 'o' || start[1] == 'O') {
        return make_int(0);
      }
    }
    if (len == 2 && (start[0] == 'o' || start[0] == 'O')) {
      if (start[1] == 'n' || start[1] == 'N') {
        return make_int(1);
      }
    }
    if (len == 3 && (start[0] == 'o' || start[0] == 'O')) {
      if ((start[1] == 'f' || start[1] == 'F') &&
          (start[2] == 'f' || start[2] == 'F')) {
        return make_int(0);
      }
    }

    // Unknown identifier - error
    set_integer_error(p, start, len);
    return make_error();
  }

  // Unexpected close paren
  if (c == ')') {
    set_close_paren_error(p);
    return make_error();
  }

  // Unknown token
  const char *start = p->pos;
  while (p->pos < p->end && !is_alnum(*p->pos) &&
         *p->pos != ' ' && *p->pos != '\t' && *p->pos != '\n' &&
         *p->pos != '(' && *p->pos != ')' && *p->pos != '[' && *p->pos != ']') {
    p->pos++;
  }
  size_t len = p->pos - start;
  if (len == 0) len = 1;
  set_integer_error(p, start, len);
  return make_error();
}

// Check if next char starts a number (digit or decimal point followed by digit)
static int is_number_start(ExprParser *p) {
  if (p->pos >= p->end) return 0;
  char c = *p->pos;
  if (c >= '0' && c <= '9') return 1;
  if (c == '.' && p->pos + 1 < p->end && p->pos[1] >= '0' && p->pos[1] <= '9') return 1;
  return 0;
}

// Parse unary: - + ~ ! followed by unary
static ExprValue parse_unary(ExprParser *p) {
  skip_whitespace(p);
  if (p->has_error) return make_error();

  if (p->pos < p->end) {
    char c = *p->pos;

    // Unary minus (but not if followed by number - that's a negative number literal)
    if (c == '-') {
      const char *saved = p->pos;
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
      TclObj obj = get_obj(p, &v);
      size_t len;
      const char *s = p->ops->string.get(p->interp, obj, &len);
      set_integer_error(p, s, len);
      return make_error();
    }

    // Unary plus
    if (c == '+') {
      const char *saved = p->pos;
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
        TclObj obj = get_obj(p, &v);
        size_t len;
        const char *s = p->ops->string.get(p->interp, obj, &len);
        set_integer_error(p, s, len);
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
      TclObj obj = get_obj(p, &v);
      size_t len;
      const char *s = p->ops->string.get(p->interp, obj, &len);
      set_integer_error(p, s, len);
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

  skip_whitespace(p);
  if (p->pos + 1 < p->end && p->pos[0] == '*' && p->pos[1] == '*') {
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
      // pow(base, exp) - implemented as repeated multiplication for simplicity
      double result = 1.0;
      int neg = exp < 0;
      if (neg) exp = -exp;
      int64_t iexp = (int64_t)exp;
      for (int64_t i = 0; i < iexp; i++) {
        result *= base;
      }
      if (neg) result = 1.0 / result;
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
      double result = 1.0;
      int neg = dexp < 0;
      if (neg) dexp = -dexp;
      int64_t iexp = (int64_t)dexp;
      for (int64_t i = 0; i < iexp; i++) {
        result *= dbase;
      }
      if (neg) result = 1.0 / result;
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    char c = *p->pos;
    // Check for ** which is exponentiation, not multiplication
    if (c == '*' && p->pos + 1 < p->end && p->pos[1] == '*') break;

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
        left = make_double(lv * rv);
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          left = make_double(dlv * drv);
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
        if (rv == 0.0) {
          set_error(p, "divide by zero", 14);
          return make_error();
        }
        left = make_double(lv / rv);
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          if (drv == 0.0) {
            set_error(p, "divide by zero", 14);
            return make_error();
          }
          left = make_double(dlv / drv);
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    char c = *p->pos;
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
        left = make_double(lv + rv);
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          left = make_double(dlv + drv);
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
        left = make_double(lv - rv);
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          left = make_double(dlv - drv);
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

// Parse comparison: additive (< <= > >= lt le gt ge) additive
// Numeric-preferring: < <= > >= try int first, fall back to string
// String-only: lt le gt ge always use string compare
static ExprValue parse_comparison(ExprParser *p) {
  ExprValue left = parse_additive(p);
  if (p->has_error) return make_error();

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    char c = *p->pos;

    // String comparison operators: lt, le, gt, ge
    if (match_keyword(p, "lt", 2)) {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      TclObj lo = get_obj(p, &left);
      TclObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp < 0 ? 1 : 0);
    } else if (match_keyword(p, "le", 2)) {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      TclObj lo = get_obj(p, &left);
      TclObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp <= 0 ? 1 : 0);
    } else if (match_keyword(p, "gt", 2)) {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      TclObj lo = get_obj(p, &left);
      TclObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp > 0 ? 1 : 0);
    } else if (match_keyword(p, "ge", 2)) {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      TclObj lo = get_obj(p, &left);
      TclObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp >= 0 ? 1 : 0);
    }
    // Numeric-preferring comparison operators
    else if (c == '<' && p->pos + 1 < p->end && p->pos[1] == '=') {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv <= rv ? 1 : 0);
        } else {
          TclObj lo = get_obj(p, &left);
          TclObj ro = get_obj(p, &right);
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
            TclObj lo = get_obj(p, &left);
            TclObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp <= 0 ? 1 : 0);
          }
        }
      }
    } else if (c == '<') {
      p->pos++;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv < rv ? 1 : 0);
        } else {
          TclObj lo = get_obj(p, &left);
          TclObj ro = get_obj(p, &right);
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
            TclObj lo = get_obj(p, &left);
            TclObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp < 0 ? 1 : 0);
          }
        }
      }
    } else if (c == '>' && p->pos + 1 < p->end && p->pos[1] == '=') {
      p->pos += 2;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv >= rv ? 1 : 0);
        } else {
          TclObj lo = get_obj(p, &left);
          TclObj ro = get_obj(p, &right);
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
            TclObj lo = get_obj(p, &left);
            TclObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp >= 0 ? 1 : 0);
          }
        }
      }
    } else if (c == '>') {
      p->pos++;
      ExprValue right = parse_additive(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (get_double(p, &left, &lv) && get_double(p, &right, &rv)) {
          left = make_int(lv > rv ? 1 : 0);
        } else {
          TclObj lo = get_obj(p, &left);
          TclObj ro = get_obj(p, &right);
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
            TclObj lo = get_obj(p, &left);
            TclObj ro = get_obj(p, &right);
            int cmp = p->ops->string.compare(p->interp, lo, ro);
            left = make_int(cmp > 0 ? 1 : 0);
          }
        }
      }
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    // String equality operators: eq, ne
    if (match_keyword(p, "eq", 2)) {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      TclObj lo = get_obj(p, &left);
      TclObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp == 0 ? 1 : 0);
    } else if (match_keyword(p, "ne", 2)) {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      TclObj lo = get_obj(p, &left);
      TclObj ro = get_obj(p, &right);
      int cmp = p->ops->string.compare(p->interp, lo, ro);
      left = make_int(cmp != 0 ? 1 : 0);
    }
    // Numeric equality operators
    else if (p->pos + 1 < p->end && p->pos[0] == '=' && p->pos[1] == '=') {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (!get_double(p, &left, &lv) || !get_double(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        left = make_int(lv == rv ? 1 : 0);
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          left = make_int(dlv == drv ? 1 : 0);
        } else {
          left = make_int(lv == rv ? 1 : 0);
        }
      }
    } else if (p->pos + 1 < p->end && p->pos[0] == '!' && p->pos[1] == '=') {
      p->pos += 2;
      ExprValue right = parse_comparison(p);
      if (p->has_error) return make_error();
      if (needs_float_math(&left, &right)) {
        double lv, rv;
        if (!get_double(p, &left, &lv) || !get_double(p, &right, &rv)) {
          set_syntax_error(p);
          return make_error();
        }
        left = make_int(lv != rv ? 1 : 0);
      } else {
        int64_t lv, rv;
        if (!get_int(p, &left, &lv) || !get_int(p, &right, &rv)) {
          // Fall back to double
          double dlv, drv;
          if (!get_double(p, &left, &dlv) || !get_double(p, &right, &drv)) {
            set_syntax_error(p);
            return make_error();
          }
          left = make_int(dlv != drv ? 1 : 0);
        } else {
          left = make_int(lv != rv ? 1 : 0);
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    // Single & (not &&)
    if (*p->pos == '&' && (p->pos + 1 >= p->end || p->pos[1] != '&')) {
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    if (*p->pos == '^') {
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    // Single | (not ||)
    if (*p->pos == '|' && (p->pos + 1 >= p->end || p->pos[1] != '|')) {
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    if (p->pos + 1 < p->end && p->pos[0] == '&' && p->pos[1] == '&') {
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
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    if (p->pos + 1 < p->end && p->pos[0] == '|' && p->pos[1] == '|') {
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

  skip_whitespace(p);
  if (p->pos < p->end && *p->pos == '?') {
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

      skip_whitespace(p);
      if (p->pos >= p->end || *p->pos != ':') {
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

      skip_whitespace(p);
      if (p->pos >= p->end || *p->pos != ':') {
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

TclResult tcl_builtin_expr(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    TclObj msg = ops->string.intern(interp,
        "wrong # args: should be \"expr arg ?arg ...?\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Concatenate all arguments with spaces
  TclObj expr_obj = ops->list.shift(interp, args);
  argc--;

  while (argc > 0) {
    TclObj space = ops->string.intern(interp, " ", 1);
    TclObj next = ops->list.shift(interp, args);
    expr_obj = ops->string.concat(interp, expr_obj, space);
    expr_obj = ops->string.concat(interp, expr_obj, next);
    argc--;
  }

  // Get the expression string
  size_t expr_len;
  const char *expr_str = ops->string.get(interp, expr_obj, &expr_len);

  // Initialize parser
  ExprParser parser = {
    .ops = ops,
    .interp = interp,
    .expr = expr_str,
    .expr_len = expr_len,
    .pos = expr_str,
    .end = expr_str + expr_len,
    .has_error = 0,
    .error_msg = 0,
    .skip_mode = 0
  };

  // Parse and evaluate
  ExprValue result = parse_ternary(&parser);

  // Check for trailing content
  skip_whitespace(&parser);
  if (!parser.has_error && parser.pos < parser.end) {
    if (*parser.pos == ')') {
      set_close_paren_error(&parser);
    } else {
      set_syntax_error(&parser);
    }
  }

  if (parser.has_error) {
    ops->interp.set_result(interp, parser.error_msg);
    return TCL_ERROR;
  }

  // Return result
  TclObj result_obj = get_obj(&parser, &result);
  ops->interp.set_result(interp, result_obj);
  return TCL_OK;
}
