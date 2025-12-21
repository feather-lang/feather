#include "tclc.h"

/**
 * Expression parser for TCL expr command.
 *
 * Operator precedence (lowest to highest):
 *   || (logical OR)
 *   && (logical AND)
 *   |  (bitwise OR)
 *   &  (bitwise AND)
 *   == != (equality)
 *   < <= > >= (comparison)
 *   unary - (negation)
 *   () (parentheses)
 *   literals, variables, commands
 */

typedef struct {
  const TclHostOps *ops;
  TclInterp interp;
  const char *expr;    // Original expression string (for error messages)
  size_t expr_len;
  const char *pos;     // Current position
  const char *end;     // End of expression
  int has_error;
  TclObj error_msg;
} ExprParser;

// Forward declarations
static int64_t parse_logical_or(ExprParser *p);
static int64_t parse_logical_and(ExprParser *p);
static int64_t parse_bitwise_or(ExprParser *p);
static int64_t parse_bitwise_and(ExprParser *p);
static int64_t parse_equality(ExprParser *p);
static int64_t parse_comparison(ExprParser *p);
static int64_t parse_unary(ExprParser *p);
static int64_t parse_primary(ExprParser *p);

static void skip_whitespace(ExprParser *p) {
  while (p->pos < p->end && (*p->pos == ' ' || *p->pos == '\t' ||
                              *p->pos == '\n' || *p->pos == '\r')) {
    p->pos++;
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

  // Build: syntax error in expression "..."
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

  // Build: expected integer but got "..."
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

  // Build: unbalanced parentheses in expression "..."
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

// Parse a variable reference $name or ${name}
static int64_t parse_variable(ExprParser *p) {
  p->pos++; // skip $

  if (p->pos >= p->end) {
    set_syntax_error(p);
    return 0;
  }

  const char *name_start;
  size_t name_len;

  if (*p->pos == '{') {
    // ${name} form
    p->pos++; // skip {
    name_start = p->pos;
    while (p->pos < p->end && *p->pos != '}') {
      p->pos++;
    }
    if (p->pos >= p->end) {
      set_syntax_error(p);
      return 0;
    }
    name_len = p->pos - name_start;
    p->pos++; // skip }
  } else {
    // $name form - alphanumeric and underscore
    name_start = p->pos;
    while (p->pos < p->end &&
           ((*p->pos >= 'a' && *p->pos <= 'z') ||
            (*p->pos >= 'A' && *p->pos <= 'Z') ||
            (*p->pos >= '0' && *p->pos <= '9') ||
            *p->pos == '_')) {
      p->pos++;
    }
    name_len = p->pos - name_start;
  }

  if (name_len == 0) {
    set_syntax_error(p);
    return 0;
  }

  TclObj name = p->ops->string.intern(p->interp, name_start, name_len);
  TclObj value = p->ops->var.get(p->interp, name);

  if (p->ops->list.is_nil(p->interp, value)) {
    // Variable doesn't exist
    TclObj part1 = p->ops->string.intern(p->interp, "can't read \"", 12);
    TclObj part2 = name;
    TclObj part3 = p->ops->string.intern(p->interp, "\": no such variable", 19);

    TclObj msg = p->ops->string.concat(p->interp, part1, part2);
    msg = p->ops->string.concat(p->interp, msg, part3);

    p->has_error = 1;
    p->error_msg = msg;
    return 0;
  }

  // Convert value to integer
  int64_t result;
  if (p->ops->integer.get(p->interp, value, &result) != TCL_OK) {
    size_t len;
    const char *str = p->ops->string.get(p->interp, value, &len);
    set_integer_error(p, str, len);
    return 0;
  }

  return result;
}

// Parse a command substitution [cmd args...]
static int64_t parse_command(ExprParser *p) {
  p->pos++; // skip [

  const char *cmd_start = p->pos;
  int depth = 1;

  while (p->pos < p->end && depth > 0) {
    if (*p->pos == '[') {
      depth++;
    } else if (*p->pos == ']') {
      depth--;
    } else if (*p->pos == '\\' && p->pos + 1 < p->end) {
      p->pos++; // skip escaped char
    }
    if (depth > 0) p->pos++;
  }

  if (depth != 0) {
    set_syntax_error(p);
    return 0;
  }

  size_t cmd_len = p->pos - cmd_start;
  p->pos++; // skip ]

  // Evaluate the command
  TclResult result = tcl_eval_string(p->ops, p->interp, cmd_start, cmd_len, TCL_EVAL_LOCAL);
  if (result != TCL_OK) {
    p->has_error = 1;
    p->error_msg = p->ops->interp.get_result(p->interp);
    return 0;
  }

  // Get result and convert to integer
  TclObj value = p->ops->interp.get_result(p->interp);
  int64_t int_result;
  if (p->ops->integer.get(p->interp, value, &int_result) != TCL_OK) {
    size_t len;
    const char *str = p->ops->string.get(p->interp, value, &len);
    set_integer_error(p, str, len);
    return 0;
  }

  return int_result;
}

// Parse a number (integer literal)
static int64_t parse_number(ExprParser *p) {
  const char *start = p->pos;
  int negative = 0;

  if (*p->pos == '-') {
    negative = 1;
    p->pos++;
  } else if (*p->pos == '+') {
    p->pos++;
  }

  if (p->pos >= p->end || *p->pos < '0' || *p->pos > '9') {
    set_integer_error(p, start, p->pos - start > 0 ? p->pos - start : 1);
    return 0;
  }

  int64_t value = 0;
  while (p->pos < p->end && *p->pos >= '0' && *p->pos <= '9') {
    value = value * 10 + (*p->pos - '0');
    p->pos++;
  }

  return negative ? -value : value;
}

// Helper to check if remaining input starts with a keyword (case insensitive)
static int match_keyword(ExprParser *p, const char *keyword, size_t klen) {
  if ((size_t)(p->end - p->pos) < klen) return 0;
  for (size_t i = 0; i < klen; i++) {
    char c = p->pos[i];
    // Convert to lowercase
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c != keyword[i]) return 0;
  }
  // Check that the keyword ends (not followed by alphanumeric)
  if (p->pos + klen < p->end) {
    char next = p->pos[klen];
    if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
        (next >= '0' && next <= '9') || next == '_') {
      return 0;
    }
  }
  return 1;
}

// Parse primary: number, variable, command, boolean literal, or parenthesized expression
static int64_t parse_primary(ExprParser *p) {
  skip_whitespace(p);

  if (p->has_error || p->pos >= p->end) {
    if (!p->has_error) set_syntax_error(p);
    return 0;
  }

  char c = *p->pos;

  if (c == '(') {
    p->pos++; // skip (
    int64_t value = parse_logical_or(p);
    skip_whitespace(p);
    if (p->pos >= p->end || *p->pos != ')') {
      set_paren_error(p);
      return 0;
    }
    p->pos++; // skip )
    return value;
  }

  if (c == '$') {
    return parse_variable(p);
  }

  if (c == '[') {
    return parse_command(p);
  }

  if ((c >= '0' && c <= '9') || c == '-' || c == '+') {
    // Could be a number or unary operator
    // Check if it's a number (digit or sign followed by digit)
    if (c >= '0' && c <= '9') {
      return parse_number(p);
    }
    // Check if next char is a digit
    if (p->pos + 1 < p->end && p->pos[1] >= '0' && p->pos[1] <= '9') {
      return parse_number(p);
    }
    // Otherwise it's an operator, fall through to error
  }

  // Check for boolean literals: true, false, yes, no, on, off
  if (match_keyword(p, "true", 4)) {
    p->pos += 4;
    return 1;
  }
  if (match_keyword(p, "false", 5)) {
    p->pos += 5;
    return 0;
  }
  if (match_keyword(p, "yes", 3)) {
    p->pos += 3;
    return 1;
  }
  if (match_keyword(p, "no", 2)) {
    p->pos += 2;
    return 0;
  }
  if (match_keyword(p, "on", 2)) {
    p->pos += 2;
    return 1;
  }
  if (match_keyword(p, "off", 3)) {
    p->pos += 3;
    return 0;
  }

  // Check for unexpected close paren
  if (c == ')') {
    set_close_paren_error(p);
    return 0;
  }

  // Unknown token - try to find its extent for error message
  const char *start = p->pos;
  while (p->pos < p->end && *p->pos != ' ' && *p->pos != '\t' &&
         *p->pos != '\n' && *p->pos != ')' && *p->pos != '(' &&
         *p->pos != '<' && *p->pos != '>' && *p->pos != '=' &&
         *p->pos != '!' && *p->pos != '&' && *p->pos != '|') {
    p->pos++;
  }
  size_t len = p->pos - start;
  if (len == 0) len = 1;
  set_integer_error(p, start, len);
  return 0;
}

// Parse unary: -expr or primary
static int64_t parse_unary(ExprParser *p) {
  skip_whitespace(p);
  if (p->has_error) return 0;

  if (p->pos < p->end && *p->pos == '-') {
    // Check it's not part of a number
    if (p->pos + 1 < p->end && p->pos[1] >= '0' && p->pos[1] <= '9') {
      return parse_primary(p);
    }
    p->pos++;
    return -parse_unary(p);
  }

  if (p->pos < p->end && *p->pos == '+') {
    if (p->pos + 1 < p->end && p->pos[1] >= '0' && p->pos[1] <= '9') {
      return parse_primary(p);
    }
    p->pos++;
    return parse_unary(p);
  }

  return parse_primary(p);
}

// Parse comparison: unary (< | <= | > | >=) unary
static int64_t parse_comparison(ExprParser *p) {
  int64_t left = parse_unary(p);
  if (p->has_error) return 0;

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    char c = *p->pos;
    if (c == '<') {
      if (p->pos + 1 < p->end && p->pos[1] == '=') {
        p->pos += 2;
        int64_t right = parse_unary(p);
        if (p->has_error) return 0;
        left = (left <= right) ? 1 : 0;
      } else {
        p->pos++;
        int64_t right = parse_unary(p);
        if (p->has_error) return 0;
        left = (left < right) ? 1 : 0;
      }
    } else if (c == '>') {
      if (p->pos + 1 < p->end && p->pos[1] == '=') {
        p->pos += 2;
        int64_t right = parse_unary(p);
        if (p->has_error) return 0;
        left = (left >= right) ? 1 : 0;
      } else {
        p->pos++;
        int64_t right = parse_unary(p);
        if (p->has_error) return 0;
        left = (left > right) ? 1 : 0;
      }
    } else {
      break;
    }
  }

  return left;
}

// Parse equality: comparison (== | !=) comparison
static int64_t parse_equality(ExprParser *p) {
  int64_t left = parse_comparison(p);
  if (p->has_error) return 0;

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    if (p->pos + 1 < p->end && p->pos[0] == '=' && p->pos[1] == '=') {
      p->pos += 2;
      int64_t right = parse_comparison(p);
      if (p->has_error) return 0;
      left = (left == right) ? 1 : 0;
    } else if (p->pos + 1 < p->end && p->pos[0] == '!' && p->pos[1] == '=') {
      p->pos += 2;
      int64_t right = parse_comparison(p);
      if (p->has_error) return 0;
      left = (left != right) ? 1 : 0;
    } else {
      break;
    }
  }

  return left;
}

// Parse bitwise AND: equality & equality
static int64_t parse_bitwise_and(ExprParser *p) {
  int64_t left = parse_equality(p);
  if (p->has_error) return 0;

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    // Check for single & (not &&)
    if (*p->pos == '&' && (p->pos + 1 >= p->end || p->pos[1] != '&')) {
      p->pos++;
      int64_t right = parse_equality(p);
      if (p->has_error) return 0;
      left = left & right;
    } else {
      break;
    }
  }

  return left;
}

// Parse bitwise OR: bitwise_and | bitwise_and
static int64_t parse_bitwise_or(ExprParser *p) {
  int64_t left = parse_bitwise_and(p);
  if (p->has_error) return 0;

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    // Check for single | (not ||)
    if (*p->pos == '|' && (p->pos + 1 >= p->end || p->pos[1] != '|')) {
      p->pos++;
      int64_t right = parse_bitwise_and(p);
      if (p->has_error) return 0;
      left = left | right;
    } else {
      break;
    }
  }

  return left;
}

// Parse logical AND: bitwise_or && bitwise_or
static int64_t parse_logical_and(ExprParser *p) {
  int64_t left = parse_bitwise_or(p);
  if (p->has_error) return 0;

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    if (p->pos + 1 < p->end && p->pos[0] == '&' && p->pos[1] == '&') {
      p->pos += 2;
      int64_t right = parse_bitwise_or(p);
      if (p->has_error) return 0;
      left = (left && right) ? 1 : 0;
    } else {
      break;
    }
  }

  return left;
}

// Parse logical OR: logical_and || logical_and
static int64_t parse_logical_or(ExprParser *p) {
  int64_t left = parse_logical_and(p);
  if (p->has_error) return 0;

  while (1) {
    skip_whitespace(p);
    if (p->pos >= p->end) break;

    if (p->pos + 1 < p->end && p->pos[0] == '|' && p->pos[1] == '|') {
      p->pos += 2;
      int64_t right = parse_logical_and(p);
      if (p->has_error) return 0;
      left = (left || right) ? 1 : 0;
    } else {
      break;
    }
  }

  return left;
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
    .error_msg = 0
  };

  // Parse and evaluate
  int64_t result = parse_logical_or(&parser);

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

  // Return result as integer object
  TclObj result_obj = ops->integer.create(interp, result);
  ops->interp.set_result(interp, result_obj);
  return TCL_OK;
}
