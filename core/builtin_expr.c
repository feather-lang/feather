/*
 * builtin_expr.c - TCL expr Command Implementation
 *
 * Recursive descent parser for Tcl expressions with proper operator precedence.
 *
 * Precedence (lowest to highest):
 *   1. ?: (ternary, right-to-left)
 *   2. || (logical or)
 *   3. && (logical and)
 *   4. | (bitwise or)
 *   5. ^ (bitwise xor)
 *   6. & (bitwise and)
 *   7. == != eq ne (equality)
 *   8. < > <= >= lt gt le ge in ni (relational)
 *   9. << >> (shift)
 *  10. + - (additive)
 *  11. * / % (multiplicative)
 *  12. ** (exponentiation, right-to-left)
 *  13. unary - + ! ~
 */

#include "internal.h"

/* ========================================================================
 * Expression Value Type
 * ======================================================================== */

typedef enum {
    EXPR_INT,
    EXPR_DOUBLE,
    EXPR_STRING,
} ExprValueType;

typedef struct {
    ExprValueType type;
    union {
        int64_t i;
        double d;
        struct {
            const char *s;
            size_t len;
        } str;
    } v;
} ExprValue;

/* ========================================================================
 * Expression Parser State
 * ======================================================================== */

typedef struct {
    TclInterp *interp;
    const TclHost *host;
    const char *pos;
    const char *end;
    const char *exprStart;  /* Original expression start for error messages */
    size_t exprLen;         /* Original expression length */
    void *arena;
    int error;
} ExprParser;

/* ========================================================================
 * Error Formatting Helpers
 * ======================================================================== */

/* Format error with position marker: "message at _@_\nin expression \"expr_@_\"" */
static void setExprError(ExprParser *p, const char *msg) {
    size_t msgLen = 0;
    while (msg[msgLen]) msgLen++;

    /* Calculate position in expression */
    size_t errorPos = p->pos - p->exprStart;
    if (errorPos > p->exprLen) errorPos = p->exprLen;

    /* Build: "msg at _@_\nin expression \"before_@_after\"" */
    size_t bufLen = msgLen + 20 + p->exprLen + 30;
    char *buf = p->host->arenaAlloc(p->arena, bufLen, 1);
    char *bp = buf;

    /* Copy message */
    for (size_t i = 0; i < msgLen; i++) *bp++ = msg[i];

    /* Add " at _@_" */
    const char *atMarker = " at _@_";
    while (*atMarker) *bp++ = *atMarker++;

    /* Add newline and "in expression \"" */
    *bp++ = '\n';
    const char *inExpr = "in expression \"";
    while (*inExpr) *bp++ = *inExpr++;

    /* Copy expression with _@_ at error position */
    for (size_t i = 0; i < errorPos; i++) *bp++ = p->exprStart[i];
    *bp++ = '_'; *bp++ = '@'; *bp++ = '_';
    for (size_t i = errorPos; i < p->exprLen; i++) *bp++ = p->exprStart[i];

    *bp++ = '"';
    *bp = '\0';

    tclSetError(p->interp, buf, bp - buf);
    p->error = 1;
}

/* Format error for invalid character: "invalid character \"c\"\nin expression \"...\"" */
static void setExprErrorChar(ExprParser *p, char c) {
    size_t bufLen = 50 + p->exprLen + 30;
    char *buf = p->host->arenaAlloc(p->arena, bufLen, 1);
    char *bp = buf;

    const char *prefix = "invalid character \"";
    while (*prefix) *bp++ = *prefix++;
    *bp++ = c;
    *bp++ = '"';
    *bp++ = '\n';

    const char *inExpr = "in expression \"";
    while (*inExpr) *bp++ = *inExpr++;
    for (size_t i = 0; i < p->exprLen; i++) *bp++ = p->exprStart[i];
    *bp++ = '"';
    *bp = '\0';

    tclSetError(p->interp, buf, bp - buf);
    p->error = 1;
}

/* Format error for non-numeric string: "cannot use non-numeric string \"val\" as operand of \"op\"" */
static void setExprErrorString(ExprParser *p, ExprValue v, const char *op, int isLeft) {
    const char *valStr = v.v.str.s;
    size_t valLen = v.v.str.len;

    size_t opLen = 0;
    while (op[opLen]) opLen++;

    size_t bufLen = 60 + valLen + opLen;
    char *buf = p->host->arenaAlloc(p->arena, bufLen, 1);
    char *bp = buf;

    const char *prefix = "cannot use non-numeric string \"";
    while (*prefix) *bp++ = *prefix++;
    for (size_t i = 0; i < valLen; i++) *bp++ = valStr[i];
    *bp++ = '"';

    const char *asOp = isLeft ? " as left operand of \"" : " as operand of \"";
    while (*asOp) *bp++ = *asOp++;
    for (size_t i = 0; i < opLen; i++) *bp++ = op[i];
    *bp++ = '"';
    *bp = '\0';

    tclSetError(p->interp, buf, bp - buf);
    p->error = 1;
}

/* Format error for type mismatch: "cannot use floating-point value \"val\" as operand of \"op\"" */
static void setExprErrorFloat(ExprParser *p, ExprValue v, const char *op, int isLeft) {
    /* Get string representation of value */
    const char *valStr;
    size_t valLen;
    char valBuf[32];
    if (v.type == EXPR_DOUBLE) {
        /* Format double */
        int len = 0;
        double d = v.v.d;
        int neg = 0;
        if (d < 0) { neg = 1; d = -d; }
        if (neg) valBuf[len++] = '-';
        int64_t intPart = (int64_t)d;
        double fracPart = d - (double)intPart;

        /* Integer part */
        char tmp[20];
        int ti = 0;
        int64_t n = intPart;
        do { tmp[ti++] = '0' + (n % 10); n /= 10; } while (n > 0);
        while (ti > 0) valBuf[len++] = tmp[--ti];

        valBuf[len++] = '.';
        /* One decimal place is enough for the error message */
        int digit = (int)(fracPart * 10);
        valBuf[len++] = '0' + digit;
        valBuf[len] = '\0';
        valStr = valBuf;
        valLen = len;
    } else {
        valStr = "?";
        valLen = 1;
    }

    size_t opLen = 0;
    while (op[opLen]) opLen++;

    size_t bufLen = 60 + valLen + opLen;
    char *buf = p->host->arenaAlloc(p->arena, bufLen, 1);
    char *bp = buf;

    const char *prefix = "cannot use floating-point value \"";
    while (*prefix) *bp++ = *prefix++;
    for (size_t i = 0; i < valLen; i++) *bp++ = valStr[i];
    *bp++ = '"';

    const char *asOp = isLeft ? " as left operand of \"" : " as operand of \"";
    while (*asOp) *bp++ = *asOp++;
    for (size_t i = 0; i < opLen; i++) *bp++ = op[i];
    *bp++ = '"';
    *bp = '\0';

    tclSetError(p->interp, buf, bp - buf);
    p->error = 1;
}

/* Format error for simple message with expression: "msg\nin expression \"...\"" */
static void setExprErrorSimple(ExprParser *p, const char *msg) {
    size_t msgLen = 0;
    while (msg[msgLen]) msgLen++;

    size_t bufLen = msgLen + 20 + p->exprLen;
    char *buf = p->host->arenaAlloc(p->arena, bufLen, 1);
    char *bp = buf;

    for (size_t i = 0; i < msgLen; i++) *bp++ = msg[i];
    *bp++ = '\n';

    const char *inExpr = "in expression \"";
    while (*inExpr) *bp++ = *inExpr++;
    for (size_t i = 0; i < p->exprLen; i++) *bp++ = p->exprStart[i];
    *bp++ = '"';
    *bp = '\0';

    tclSetError(p->interp, buf, bp - buf);
    p->error = 1;
}

/* Forward declarations */
static ExprValue parseExpr(ExprParser *p);
static ExprValue parseTernary(ExprParser *p);
static ExprValue parseLogicalOr(ExprParser *p);
static ExprValue parseLogicalAnd(ExprParser *p);
static ExprValue parseBitwiseOr(ExprParser *p);
static ExprValue parseBitwiseXor(ExprParser *p);
static ExprValue parseBitwiseAnd(ExprParser *p);
static ExprValue parseEquality(ExprParser *p);
static ExprValue parseRelational(ExprParser *p);
static ExprValue parseShift(ExprParser *p);
static ExprValue parseAdditive(ExprParser *p);
static ExprValue parseMultiplicative(ExprParser *p);
static ExprValue parseExponentiation(ExprParser *p);
static ExprValue parseUnary(ExprParser *p);
static ExprValue parsePrimary(ExprParser *p);

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

static void skipWhitespace(ExprParser *p) {
    while (p->pos < p->end && (*p->pos == ' ' || *p->pos == '\t' ||
           *p->pos == '\n' || *p->pos == '\r')) {
        p->pos++;
    }
}

static int isDigit(char c) {
    return c >= '0' && c <= '9';
}

static int isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int isAlnum(char c) {
    return isDigit(c) || isAlpha(c);
}

static int isHexDigit(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static int matchKeyword(ExprParser *p, const char *kw, size_t kwLen) {
    if ((size_t)(p->end - p->pos) >= kwLen &&
        tclStrncmp(p->pos, kw, kwLen) == 0 &&
        (p->pos + kwLen >= p->end || !isAlnum(p->pos[kwLen]))) {
        return 1;
    }
    return 0;
}

static ExprValue makeInt(int64_t i) {
    ExprValue v;
    v.type = EXPR_INT;
    v.v.i = i;
    return v;
}

static ExprValue makeDouble(double d) {
    ExprValue v;
    v.type = EXPR_DOUBLE;
    v.v.d = d;
    return v;
}

static ExprValue makeString(const char *s, size_t len) {
    ExprValue v;
    v.type = EXPR_STRING;
    v.v.str.s = s;
    v.v.str.len = len;
    return v;
}

static double toDouble(ExprValue v) {
    if (v.type == EXPR_INT) return (double)v.v.i;
    if (v.type == EXPR_DOUBLE) return v.v.d;
    return 0.0;
}

/* Currently unused but may be needed for future math functions
static int64_t toInt(ExprValue v) {
    if (v.type == EXPR_INT) return v.v.i;
    if (v.type == EXPR_DOUBLE) return (int64_t)v.v.d;
    return 0;
}
*/

static int toBool(ExprValue v) {
    if (v.type == EXPR_INT) return v.v.i != 0;
    if (v.type == EXPR_DOUBLE) return v.v.d != 0.0;
    if (v.type == EXPR_STRING) {
        if (v.v.str.len == 0) return 0;
        /* Check for boolean strings */
        if (v.v.str.len == 1) {
            if (v.v.str.s[0] == '0') return 0;
            if (v.v.str.s[0] == '1') return 1;
        }
        if (v.v.str.len == 4 && tclStrncmp(v.v.str.s, "true", 4) == 0) return 1;
        if (v.v.str.len == 5 && tclStrncmp(v.v.str.s, "false", 5) == 0) return 0;
        if (v.v.str.len == 3 && tclStrncmp(v.v.str.s, "yes", 3) == 0) return 1;
        if (v.v.str.len == 2 && tclStrncmp(v.v.str.s, "no", 2) == 0) return 0;
        if (v.v.str.len == 2 && tclStrncmp(v.v.str.s, "on", 2) == 0) return 1;
        if (v.v.str.len == 3 && tclStrncmp(v.v.str.s, "off", 3) == 0) return 0;
        return 1; /* Non-empty string is true */
    }
    return 0;
}

static int strncmpLen(const char *a, size_t alen, const char *b, size_t blen) {
    size_t minLen = alen < blen ? alen : blen;
    for (size_t i = 0; i < minLen; i++) {
        if ((unsigned char)a[i] < (unsigned char)b[i]) return -1;
        if ((unsigned char)a[i] > (unsigned char)b[i]) return 1;
    }
    if (alen < blen) return -1;
    if (alen > blen) return 1;
    return 0;
}

/* Get string representation of value */
static void getStringRep(ExprParser *p, ExprValue v, const char **s, size_t *len) {
    if (v.type == EXPR_STRING) {
        *s = v.v.str.s;
        *len = v.v.str.len;
    } else if (v.type == EXPR_INT) {
        /* Convert int to string in arena */
        char *buf = p->host->arenaAlloc(p->arena, 32, 1);
        int64_t n = v.v.i;
        int neg = 0;
        if (n < 0) { neg = 1; n = -n; }
        char *ep = buf + 31;
        *ep = '\0';
        do {
            *--ep = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        if (neg) *--ep = '-';
        *s = ep;
        *len = (buf + 31) - ep;
    } else {
        /* Double - simplified conversion */
        char *buf = p->host->arenaAlloc(p->arena, 64, 1);
        double d = v.v.d;
        int neg = 0;
        if (d < 0) { neg = 1; d = -d; }
        int64_t intPart = (int64_t)d;
        double fracPart = d - (double)intPart;

        char *bp = buf;
        if (neg) *bp++ = '-';

        /* Integer part */
        char tmp[32];
        char *tp = tmp + 31;
        *tp = '\0';
        int64_t n = intPart;
        do {
            *--tp = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        while (*tp) *bp++ = *tp++;

        /* Fractional part */
        *bp++ = '.';
        for (int i = 0; i < 6; i++) {
            fracPart *= 10;
            int digit = (int)fracPart;
            *bp++ = '0' + digit;
            fracPart -= digit;
        }
        *bp = '\0';
        *s = buf;
        *len = bp - buf;
    }
}

/* ========================================================================
 * Number Parsing
 * ======================================================================== */

static ExprValue parseNumber(ExprParser *p) {
    int neg = 0;

    /* Handle leading sign for unary context */
    if (*p->pos == '-') {
        neg = 1;
        p->pos++;
    } else if (*p->pos == '+') {
        p->pos++;
    }

    /* Skip underscores between digits */
    #define SKIP_UNDERSCORE() while (p->pos < p->end && *p->pos == '_') p->pos++

    /* Check for radix prefix */
    if (p->pos + 1 < p->end && p->pos[0] == '0') {
        if (p->pos[1] == 'x' || p->pos[1] == 'X') {
            /* Hexadecimal */
            p->pos += 2;
            int64_t val = 0;
            while (p->pos < p->end && (isHexDigit(*p->pos) || *p->pos == '_')) {
                if (*p->pos != '_') {
                    val = val * 16 + hexValue(*p->pos);
                }
                p->pos++;
            }
            return makeInt(neg ? -val : val);
        } else if (p->pos[1] == 'b' || p->pos[1] == 'B') {
            /* Binary */
            p->pos += 2;
            int64_t val = 0;
            while (p->pos < p->end && (*p->pos == '0' || *p->pos == '1' || *p->pos == '_')) {
                if (*p->pos != '_') {
                    val = val * 2 + (*p->pos - '0');
                }
                p->pos++;
            }
            return makeInt(neg ? -val : val);
        } else if (p->pos[1] == 'o' || p->pos[1] == 'O') {
            /* Octal */
            p->pos += 2;
            int64_t val = 0;
            while (p->pos < p->end && ((*p->pos >= '0' && *p->pos <= '7') || *p->pos == '_')) {
                if (*p->pos != '_') {
                    val = val * 8 + (*p->pos - '0');
                }
                p->pos++;
            }
            return makeInt(neg ? -val : val);
        }
    }

    /* Decimal integer or floating point */
    int64_t intVal = 0;
    while (p->pos < p->end && (isDigit(*p->pos) || *p->pos == '_')) {
        if (*p->pos != '_') {
            intVal = intVal * 10 + (*p->pos - '0');
        }
        p->pos++;
    }

    /* Check for floating point */
    int isFloat = 0;
    double floatVal = (double)intVal;

    if (p->pos < p->end && *p->pos == '.') {
        isFloat = 1;
        p->pos++;
        double frac = 0.1;
        while (p->pos < p->end && (isDigit(*p->pos) || *p->pos == '_')) {
            if (*p->pos != '_') {
                floatVal += (*p->pos - '0') * frac;
                frac *= 0.1;
            }
            p->pos++;
        }
    }

    /* Check for exponent */
    if (p->pos < p->end && (*p->pos == 'e' || *p->pos == 'E')) {
        isFloat = 1;
        p->pos++;
        int expNeg = 0;
        if (p->pos < p->end && *p->pos == '-') {
            expNeg = 1;
            p->pos++;
        } else if (p->pos < p->end && *p->pos == '+') {
            p->pos++;
        }
        int exp = 0;
        while (p->pos < p->end && (isDigit(*p->pos) || *p->pos == '_')) {
            if (*p->pos != '_') {
                exp = exp * 10 + (*p->pos - '0');
            }
            p->pos++;
        }
        if (expNeg) exp = -exp;
        /* Apply exponent */
        double mult = 1.0;
        int absExp = exp < 0 ? -exp : exp;
        for (int i = 0; i < absExp; i++) mult *= 10.0;
        if (exp < 0) floatVal /= mult;
        else floatVal *= mult;
    }

    if (isFloat) {
        return makeDouble(neg ? -floatVal : floatVal);
    }
    return makeInt(neg ? -intVal : intVal);
}

/* ========================================================================
 * String Parsing
 * ======================================================================== */

static ExprValue parseString(ExprParser *p, char quote) {
    p->pos++; /* Skip opening quote */
    const char *start = p->pos;

    /* Find end, handling escapes */
    while (p->pos < p->end && *p->pos != quote) {
        if (*p->pos == '\\' && p->pos + 1 < p->end) {
            p->pos += 2;
        } else {
            p->pos++;
        }
    }

    size_t len = p->pos - start;
    if (p->pos < p->end) p->pos++; /* Skip closing quote */

    return makeString(start, len);
}

/* ========================================================================
 * Braced String Parsing
 * ======================================================================== */

static ExprValue parseBraced(ExprParser *p) {
    p->pos++; /* Skip opening brace */
    const char *start = p->pos;
    int depth = 1;

    while (p->pos < p->end && depth > 0) {
        if (*p->pos == '{') depth++;
        else if (*p->pos == '}') depth--;
        if (depth > 0) p->pos++;
    }

    size_t len = p->pos - start;
    if (p->pos < p->end) p->pos++; /* Skip closing brace */

    return makeString(start, len);
}

/* ========================================================================
 * Variable and Command Substitution
 * ======================================================================== */

/* Helper to set variable read error with name */
static void setVarReadError(ExprParser *p, const char *name, size_t nameLen) {
    /* Format: can't read "varname": no such variable */
    char *buf = p->host->arenaAlloc(p->arena, nameLen + 50, 1);
    char *bp = buf;
    const char *prefix = "can't read \"";
    while (*prefix) *bp++ = *prefix++;
    for (size_t i = 0; i < nameLen; i++) *bp++ = name[i];
    const char *suffix = "\": no such variable";
    while (*suffix) *bp++ = *suffix++;
    *bp = '\0';
    tclSetError(p->interp, buf, bp - buf);
}

static ExprValue parseVariable(ExprParser *p) {
    p->pos++; /* Skip $ */
    const char *start = p->pos;

    /* Handle ${name} form */
    if (p->pos < p->end && *p->pos == '{') {
        p->pos++;
        start = p->pos;
        while (p->pos < p->end && *p->pos != '}') p->pos++;
        size_t nameLen = p->pos - start;
        if (p->pos < p->end) p->pos++;

        TclObj *val = p->host->varGet(p->interp->hostCtx, start, nameLen);
        if (!val) {
            setVarReadError(p, start, nameLen);
            p->error = 1;
            return makeInt(0);
        }
        size_t len;
        const char *s = p->host->getStringPtr(val, &len);

        /* Try to parse as number */
        int64_t ival;
        if (p->host->asInt(val, &ival) == 0) {
            return makeInt(ival);
        }
        double dval;
        if (p->host->asDouble(val, &dval) == 0) {
            return makeDouble(dval);
        }
        return makeString(s, len);
    }

    /* Simple variable name */
    while (p->pos < p->end && (isAlnum(*p->pos) || *p->pos == ':')) {
        if (*p->pos == ':' && p->pos + 1 < p->end && p->pos[1] == ':') {
            p->pos += 2; /* namespace separator */
        } else if (*p->pos == ':') {
            break;
        } else {
            p->pos++;
        }
    }

    size_t nameLen = p->pos - start;
    TclObj *val = p->host->varGet(p->interp->hostCtx, start, nameLen);
    if (!val) {
        setVarReadError(p, start, nameLen);
        p->error = 1;
        return makeInt(0);
    }

    size_t len;
    const char *s = p->host->getStringPtr(val, &len);

    /* Try to parse as number */
    int64_t ival;
    if (p->host->asInt(val, &ival) == 0) {
        return makeInt(ival);
    }
    double dval;
    if (p->host->asDouble(val, &dval) == 0) {
        return makeDouble(dval);
    }
    return makeString(s, len);
}

static ExprValue parseCommand(ExprParser *p) {
    p->pos++; /* Skip [ */
    const char *start = p->pos;
    int depth = 1;

    while (p->pos < p->end && depth > 0) {
        if (*p->pos == '[') depth++;
        else if (*p->pos == ']') depth--;
        if (depth > 0) p->pos++;
    }

    size_t cmdLen = p->pos - start;
    if (p->pos < p->end) p->pos++; /* Skip ] */

    /* Evaluate the command */
    TclResult res = tclEvalBracketed(p->interp, start, cmdLen);
    if (res != TCL_OK) {
        p->error = 1;
        return makeInt(0);
    }

    TclObj *result = tclGetResult(p->interp);
    size_t len;
    const char *s = p->host->getStringPtr(result, &len);

    /* Try to parse as number */
    int64_t ival;
    if (p->host->asInt(result, &ival) == 0) {
        return makeInt(ival);
    }
    double dval;
    if (p->host->asDouble(result, &dval) == 0) {
        return makeDouble(dval);
    }
    return makeString(s, len);
}

/* ========================================================================
 * Primary Expression
 * ======================================================================== */

static ExprValue parsePrimary(ExprParser *p) {
    skipWhitespace(p);

    if (p->pos >= p->end) {
        setExprError(p, "missing operand");
        return makeInt(0);
    }

    char c = *p->pos;

    /* Parenthesized expression */
    if (c == '(') {
        p->pos++;
        ExprValue v = parseExpr(p);
        skipWhitespace(p);
        if (p->pos < p->end && *p->pos == ')') {
            p->pos++;
        } else if (!p->error) {
            setExprErrorSimple(p, "unbalanced open paren");
        }
        return v;
    }

    /* Number */
    if (isDigit(c) || (c == '.' && p->pos + 1 < p->end && isDigit(p->pos[1]))) {
        return parseNumber(p);
    }

    /* String literals */
    if (c == '"') {
        return parseString(p, '"');
    }

    /* Braced string */
    if (c == '{') {
        return parseBraced(p);
    }

    /* Variable */
    if (c == '$') {
        return parseVariable(p);
    }

    /* Command substitution */
    if (c == '[') {
        return parseCommand(p);
    }

    /* Boolean/special literals */
    if (matchKeyword(p, "true", 4)) {
        p->pos += 4;
        return makeInt(1);
    }
    if (matchKeyword(p, "false", 5)) {
        p->pos += 5;
        return makeInt(0);
    }
    if (matchKeyword(p, "yes", 3)) {
        p->pos += 3;
        return makeInt(1);
    }
    if (matchKeyword(p, "no", 2)) {
        p->pos += 2;
        return makeInt(0);
    }

    /* Inf and NaN */
    if (matchKeyword(p, "Inf", 3) || matchKeyword(p, "inf", 3) || matchKeyword(p, "INF", 3)) {
        p->pos += 3;
        return makeDouble(1.0 / 0.0); /* Infinity */
    }
    if (matchKeyword(p, "NaN", 3) || matchKeyword(p, "nan", 3) || matchKeyword(p, "NAN", 3)) {
        p->pos += 3;
        return makeDouble(0.0 / 0.0); /* NaN */
    }

    /* Function call - identifier followed by ( */
    if (isAlpha(c)) {
        const char *start = p->pos;
        while (p->pos < p->end && (isAlnum(*p->pos) || *p->pos == ':')) {
            p->pos++;
        }
        size_t nameLen = p->pos - start;
        skipWhitespace(p);

        if (p->pos < p->end && *p->pos == '(') {
            /* It's a function call - for now, report as unsupported */
            tclSetError(p->interp, "math functions not yet supported", -1);
            p->error = 1;
            return makeInt(0);
        }

        /* Not a function - treat as bareword string */
        return makeString(start, nameLen);
    }

    /* Check if it's a binary operator - if so, report missing operand */
    if (c == '*' || c == '/' || c == '%' || c == '&' || c == '|' ||
        c == '^' || c == '?' || c == ':' || c == ')') {
        setExprError(p, "missing operand");
        return makeInt(0);
    }

    /* Unknown character - report as invalid */
    setExprErrorChar(p, c);
    return makeInt(0);
}

/* ========================================================================
 * Unary Operators
 * ======================================================================== */

static ExprValue parseUnary(ExprParser *p) {
    skipWhitespace(p);

    if (p->pos >= p->end) {
        return parsePrimary(p);
    }

    char c = *p->pos;

    /* Unary minus */
    if (c == '-') {
        p->pos++;
        ExprValue v = parseUnary(p);
        if (p->error) return v;
        if (v.type == EXPR_INT) return makeInt(-v.v.i);
        if (v.type == EXPR_DOUBLE) return makeDouble(-v.v.d);
        tclSetError(p->interp, "can't use non-numeric value as operand of \"-\"", -1);
        p->error = 1;
        return makeInt(0);
    }

    /* Unary plus */
    if (c == '+') {
        p->pos++;
        ExprValue v = parseUnary(p);
        if (p->error) return v;
        if (v.type == EXPR_INT || v.type == EXPR_DOUBLE) return v;
        tclSetError(p->interp, "can't use non-numeric value as operand of \"+\"", -1);
        p->error = 1;
        return makeInt(0);
    }

    /* Logical NOT */
    if (c == '!') {
        p->pos++;
        ExprValue v = parseUnary(p);
        if (p->error) return v;
        return makeInt(!toBool(v));
    }

    /* Bitwise NOT */
    if (c == '~') {
        p->pos++;
        ExprValue v = parseUnary(p);
        if (p->error) return v;
        if (v.type != EXPR_INT) {
            setExprErrorFloat(p, v, "~", 0);
            return makeInt(0);
        }
        return makeInt(~v.v.i);
    }

    return parsePrimary(p);
}

/* ========================================================================
 * Exponentiation (right-to-left associative)
 * ======================================================================== */

static ExprValue parseExponentiation(ExprParser *p) {
    ExprValue left = parseUnary(p);
    if (p->error) return left;

    skipWhitespace(p);

    if (p->pos + 1 < p->end && p->pos[0] == '*' && p->pos[1] == '*') {
        p->pos += 2;
        ExprValue right = parseExponentiation(p); /* Right-to-left */
        if (p->error) return left;

        double base = toDouble(left);
        double exp = toDouble(right);
        double result = 1.0;

        /* Simple power implementation */
        if (exp == 0) {
            result = 1.0;
        } else if (exp > 0 && exp == (int64_t)exp) {
            int64_t e = (int64_t)exp;
            result = 1.0;
            double b = base;
            while (e > 0) {
                if (e & 1) result *= b;
                b *= b;
                e >>= 1;
            }
        } else {
            /* Use exp and log for non-integer exponents */
            /* Simplified: just do repeated multiplication for small positive ints */
            tclSetError(p->interp, "non-integer exponent not yet supported", -1);
            p->error = 1;
            return makeInt(0);
        }

        /* Return int if both operands were int and result fits */
        if (left.type == EXPR_INT && right.type == EXPR_INT &&
            result == (double)(int64_t)result) {
            return makeInt((int64_t)result);
        }
        return makeDouble(result);
    }

    return left;
}

/* ========================================================================
 * Multiplicative
 * ======================================================================== */

static ExprValue parseMultiplicative(ExprParser *p) {
    ExprValue left = parseExponentiation(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        char c = *p->pos;

        /* Make sure it's not ** */
        if (c == '*' && p->pos + 1 < p->end && p->pos[1] == '*') break;

        if (c == '*') {
            p->pos++;
            ExprValue right = parseExponentiation(p);
            if (p->error) return left;

            if (left.type == EXPR_INT && right.type == EXPR_INT) {
                left = makeInt(left.v.i * right.v.i);
            } else {
                left = makeDouble(toDouble(left) * toDouble(right));
            }
        } else if (c == '/') {
            p->pos++;
            ExprValue right = parseExponentiation(p);
            if (p->error) return left;

            if (left.type == EXPR_INT && right.type == EXPR_INT) {
                if (right.v.i == 0) {
                    tclSetError(p->interp, "divide by zero", -1);
                    p->error = 1;
                    return makeInt(0);
                }
                /* Tcl uses floor division (toward negative infinity) */
                int64_t a = left.v.i;
                int64_t b = right.v.i;
                int64_t q = a / b;
                int64_t r = a % b;
                /* Adjust for floor division: if remainder != 0 and signs differ, subtract 1 */
                if (r != 0 && ((a < 0) != (b < 0))) {
                    q -= 1;
                }
                left = makeInt(q);
            } else {
                double d = toDouble(right);
                if (d == 0.0) {
                    /* Float division by zero returns Inf */
                    left = makeDouble(toDouble(left) / d);
                } else {
                    left = makeDouble(toDouble(left) / d);
                }
            }
        } else if (c == '%') {
            p->pos++;
            ExprValue right = parseExponentiation(p);
            if (p->error) return left;

            if (left.type != EXPR_INT || right.type != EXPR_INT) {
                ExprValue badVal = (left.type != EXPR_INT) ? left : right;
                setExprErrorFloat(p, badVal, "%", left.type != EXPR_INT);
                return makeInt(0);
            }
            if (right.v.i == 0) {
                tclSetError(p->interp, "divide by zero", -1);
                p->error = 1;
                return makeInt(0);
            }
            /* Tcl remainder has same sign as divisor */
            int64_t rem = left.v.i % right.v.i;
            if (rem != 0 && (rem < 0) != (right.v.i < 0)) {
                rem += right.v.i;
            }
            left = makeInt(rem);
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Additive
 * ======================================================================== */

static ExprValue parseAdditive(ExprParser *p) {
    ExprValue left = parseMultiplicative(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        char c = *p->pos;

        if (c == '+') {
            p->pos++;
            ExprValue right = parseMultiplicative(p);
            if (p->error) return left;

            /* Check for string operands */
            if (left.type == EXPR_STRING) {
                setExprErrorString(p, left, "+", 1);
                return makeInt(0);
            }
            if (right.type == EXPR_STRING) {
                setExprErrorString(p, right, "+", 0);
                return makeInt(0);
            }

            if (left.type == EXPR_INT && right.type == EXPR_INT) {
                left = makeInt(left.v.i + right.v.i);
            } else {
                left = makeDouble(toDouble(left) + toDouble(right));
            }
        } else if (c == '-') {
            p->pos++;
            ExprValue right = parseMultiplicative(p);
            if (p->error) return left;

            /* Check for string operands */
            if (left.type == EXPR_STRING) {
                setExprErrorString(p, left, "-", 1);
                return makeInt(0);
            }
            if (right.type == EXPR_STRING) {
                setExprErrorString(p, right, "-", 0);
                return makeInt(0);
            }

            if (left.type == EXPR_INT && right.type == EXPR_INT) {
                left = makeInt(left.v.i - right.v.i);
            } else {
                left = makeDouble(toDouble(left) - toDouble(right));
            }
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Shift
 * ======================================================================== */

static ExprValue parseShift(ExprParser *p) {
    ExprValue left = parseAdditive(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos + 1 >= p->end) break;

        if (p->pos[0] == '<' && p->pos[1] == '<') {
            p->pos += 2;
            ExprValue right = parseAdditive(p);
            if (p->error) return left;

            if (left.type != EXPR_INT || right.type != EXPR_INT) {
                ExprValue badVal = (left.type != EXPR_INT) ? left : right;
                setExprErrorFloat(p, badVal, "<<", left.type != EXPR_INT);
                return makeInt(0);
            }
            left = makeInt(left.v.i << right.v.i);
        } else if (p->pos[0] == '>' && p->pos[1] == '>') {
            p->pos += 2;
            ExprValue right = parseAdditive(p);
            if (p->error) return left;

            if (left.type != EXPR_INT || right.type != EXPR_INT) {
                ExprValue badVal = (left.type != EXPR_INT) ? left : right;
                setExprErrorFloat(p, badVal, ">>", left.type != EXPR_INT);
                return makeInt(0);
            }
            /* Arithmetic right shift (preserves sign) */
            left = makeInt(left.v.i >> right.v.i);
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Relational
 * ======================================================================== */

static ExprValue parseRelational(ExprParser *p) {
    ExprValue left = parseShift(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        /* Check for two-char operators first */
        if (p->pos + 1 < p->end) {
            if (p->pos[0] == '<' && p->pos[1] == '=') {
                p->pos += 2;
                ExprValue right = parseShift(p);
                if (p->error) return left;

                if (left.type == EXPR_STRING || right.type == EXPR_STRING) {
                    const char *ls, *rs;
                    size_t ll, rl;
                    getStringRep(p, left, &ls, &ll);
                    getStringRep(p, right, &rs, &rl);
                    left = makeInt(strncmpLen(ls, ll, rs, rl) <= 0);
                } else {
                    left = makeInt(toDouble(left) <= toDouble(right));
                }
                continue;
            }
            if (p->pos[0] == '>' && p->pos[1] == '=') {
                p->pos += 2;
                ExprValue right = parseShift(p);
                if (p->error) return left;

                if (left.type == EXPR_STRING || right.type == EXPR_STRING) {
                    const char *ls, *rs;
                    size_t ll, rl;
                    getStringRep(p, left, &ls, &ll);
                    getStringRep(p, right, &rs, &rl);
                    left = makeInt(strncmpLen(ls, ll, rs, rl) >= 0);
                } else {
                    left = makeInt(toDouble(left) >= toDouble(right));
                }
                continue;
            }
        }

        /* String comparison operators */
        if (matchKeyword(p, "lt", 2)) {
            p->pos += 2;
            ExprValue right = parseShift(p);
            if (p->error) return left;
            const char *ls, *rs;
            size_t ll, rl;
            getStringRep(p, left, &ls, &ll);
            getStringRep(p, right, &rs, &rl);
            left = makeInt(strncmpLen(ls, ll, rs, rl) < 0);
            continue;
        }
        if (matchKeyword(p, "gt", 2)) {
            p->pos += 2;
            ExprValue right = parseShift(p);
            if (p->error) return left;
            const char *ls, *rs;
            size_t ll, rl;
            getStringRep(p, left, &ls, &ll);
            getStringRep(p, right, &rs, &rl);
            left = makeInt(strncmpLen(ls, ll, rs, rl) > 0);
            continue;
        }
        if (matchKeyword(p, "le", 2)) {
            p->pos += 2;
            ExprValue right = parseShift(p);
            if (p->error) return left;
            const char *ls, *rs;
            size_t ll, rl;
            getStringRep(p, left, &ls, &ll);
            getStringRep(p, right, &rs, &rl);
            left = makeInt(strncmpLen(ls, ll, rs, rl) <= 0);
            continue;
        }
        if (matchKeyword(p, "ge", 2)) {
            p->pos += 2;
            ExprValue right = parseShift(p);
            if (p->error) return left;
            const char *ls, *rs;
            size_t ll, rl;
            getStringRep(p, left, &ls, &ll);
            getStringRep(p, right, &rs, &rl);
            left = makeInt(strncmpLen(ls, ll, rs, rl) >= 0);
            continue;
        }

        /* in / ni operators */
        if (matchKeyword(p, "in", 2)) {
            p->pos += 2;
            ExprValue right = parseShift(p);
            if (p->error) return left;

            /* Get needle string */
            const char *needle;
            size_t needleLen;
            getStringRep(p, left, &needle, &needleLen);

            /* Get list string */
            const char *list;
            size_t listLen;
            getStringRep(p, right, &list, &listLen);

            /* Parse list and search */
            TclObj *listObj = p->host->newString(list, listLen);
            size_t count = p->host->listLength(listObj);
            int found = 0;
            for (size_t i = 0; i < count; i++) {
                TclObj *elem = p->host->listIndex(listObj, i);
                size_t elemLen;
                const char *elemStr = p->host->getStringPtr(elem, &elemLen);
                if (elemLen == needleLen && tclStrncmp(elemStr, needle, needleLen) == 0) {
                    found = 1;
                    break;
                }
            }
            left = makeInt(found);
            continue;
        }
        if (matchKeyword(p, "ni", 2)) {
            p->pos += 2;
            ExprValue right = parseShift(p);
            if (p->error) return left;

            const char *needle;
            size_t needleLen;
            getStringRep(p, left, &needle, &needleLen);

            const char *list;
            size_t listLen;
            getStringRep(p, right, &list, &listLen);

            TclObj *listObj = p->host->newString(list, listLen);
            size_t count = p->host->listLength(listObj);
            int found = 0;
            for (size_t i = 0; i < count; i++) {
                TclObj *elem = p->host->listIndex(listObj, i);
                size_t elemLen;
                const char *elemStr = p->host->getStringPtr(elem, &elemLen);
                if (elemLen == needleLen && tclStrncmp(elemStr, needle, needleLen) == 0) {
                    found = 1;
                    break;
                }
            }
            left = makeInt(!found);
            continue;
        }

        /* Single char < > */
        if (*p->pos == '<' && (p->pos + 1 >= p->end || p->pos[1] != '<')) {
            p->pos++;
            ExprValue right = parseShift(p);
            if (p->error) return left;

            if (left.type == EXPR_STRING || right.type == EXPR_STRING) {
                const char *ls, *rs;
                size_t ll, rl;
                getStringRep(p, left, &ls, &ll);
                getStringRep(p, right, &rs, &rl);
                left = makeInt(strncmpLen(ls, ll, rs, rl) < 0);
            } else {
                left = makeInt(toDouble(left) < toDouble(right));
            }
            continue;
        }
        if (*p->pos == '>' && (p->pos + 1 >= p->end || p->pos[1] != '>')) {
            p->pos++;
            ExprValue right = parseShift(p);
            if (p->error) return left;

            if (left.type == EXPR_STRING || right.type == EXPR_STRING) {
                const char *ls, *rs;
                size_t ll, rl;
                getStringRep(p, left, &ls, &ll);
                getStringRep(p, right, &rs, &rl);
                left = makeInt(strncmpLen(ls, ll, rs, rl) > 0);
            } else {
                left = makeInt(toDouble(left) > toDouble(right));
            }
            continue;
        }

        break;
    }

    return left;
}

/* ========================================================================
 * Equality
 * ======================================================================== */

static ExprValue parseEquality(ExprParser *p) {
    ExprValue left = parseRelational(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        if (p->pos + 1 < p->end && p->pos[0] == '=' && p->pos[1] == '=') {
            p->pos += 2;
            ExprValue right = parseRelational(p);
            if (p->error) return left;

            if (left.type == EXPR_STRING || right.type == EXPR_STRING) {
                const char *ls, *rs;
                size_t ll, rl;
                getStringRep(p, left, &ls, &ll);
                getStringRep(p, right, &rs, &rl);
                left = makeInt(strncmpLen(ls, ll, rs, rl) == 0);
            } else if (left.type == EXPR_INT && right.type == EXPR_INT) {
                left = makeInt(left.v.i == right.v.i);
            } else {
                left = makeInt(toDouble(left) == toDouble(right));
            }
            continue;
        }
        if (p->pos + 1 < p->end && p->pos[0] == '!' && p->pos[1] == '=') {
            p->pos += 2;
            ExprValue right = parseRelational(p);
            if (p->error) return left;

            if (left.type == EXPR_STRING || right.type == EXPR_STRING) {
                const char *ls, *rs;
                size_t ll, rl;
                getStringRep(p, left, &ls, &ll);
                getStringRep(p, right, &rs, &rl);
                left = makeInt(strncmpLen(ls, ll, rs, rl) != 0);
            } else if (left.type == EXPR_INT && right.type == EXPR_INT) {
                left = makeInt(left.v.i != right.v.i);
            } else {
                left = makeInt(toDouble(left) != toDouble(right));
            }
            continue;
        }

        /* String equality operators */
        if (matchKeyword(p, "eq", 2)) {
            p->pos += 2;
            ExprValue right = parseRelational(p);
            if (p->error) return left;
            const char *ls, *rs;
            size_t ll, rl;
            getStringRep(p, left, &ls, &ll);
            getStringRep(p, right, &rs, &rl);
            left = makeInt(strncmpLen(ls, ll, rs, rl) == 0);
            continue;
        }
        if (matchKeyword(p, "ne", 2)) {
            p->pos += 2;
            ExprValue right = parseRelational(p);
            if (p->error) return left;
            const char *ls, *rs;
            size_t ll, rl;
            getStringRep(p, left, &ls, &ll);
            getStringRep(p, right, &rs, &rl);
            left = makeInt(strncmpLen(ls, ll, rs, rl) != 0);
            continue;
        }

        break;
    }

    return left;
}

/* ========================================================================
 * Bitwise AND
 * ======================================================================== */

static ExprValue parseBitwiseAnd(ExprParser *p) {
    ExprValue left = parseEquality(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        /* Make sure it's not && */
        if (*p->pos == '&' && (p->pos + 1 >= p->end || p->pos[1] != '&')) {
            p->pos++;
            ExprValue right = parseEquality(p);
            if (p->error) return left;

            if (left.type != EXPR_INT || right.type != EXPR_INT) {
                ExprValue badVal = (left.type != EXPR_INT) ? left : right;
                setExprErrorFloat(p, badVal, "&", left.type != EXPR_INT);
                return makeInt(0);
            }
            left = makeInt(left.v.i & right.v.i);
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Bitwise XOR
 * ======================================================================== */

static ExprValue parseBitwiseXor(ExprParser *p) {
    ExprValue left = parseBitwiseAnd(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        if (*p->pos == '^') {
            p->pos++;
            ExprValue right = parseBitwiseAnd(p);
            if (p->error) return left;

            if (left.type != EXPR_INT || right.type != EXPR_INT) {
                ExprValue badVal = (left.type != EXPR_INT) ? left : right;
                setExprErrorFloat(p, badVal, "^", left.type != EXPR_INT);
                return makeInt(0);
            }
            left = makeInt(left.v.i ^ right.v.i);
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Bitwise OR
 * ======================================================================== */

static ExprValue parseBitwiseOr(ExprParser *p) {
    ExprValue left = parseBitwiseXor(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos >= p->end) break;

        /* Make sure it's not || */
        if (*p->pos == '|' && (p->pos + 1 >= p->end || p->pos[1] != '|')) {
            p->pos++;
            ExprValue right = parseBitwiseXor(p);
            if (p->error) return left;

            if (left.type != EXPR_INT || right.type != EXPR_INT) {
                ExprValue badVal = (left.type != EXPR_INT) ? left : right;
                setExprErrorFloat(p, badVal, "|", left.type != EXPR_INT);
                return makeInt(0);
            }
            left = makeInt(left.v.i | right.v.i);
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Logical AND
 * ======================================================================== */

static ExprValue parseLogicalAnd(ExprParser *p) {
    ExprValue left = parseBitwiseOr(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos + 1 >= p->end) break;

        if (p->pos[0] == '&' && p->pos[1] == '&') {
            p->pos += 2;

            /* Short-circuit: if left is false, don't evaluate right */
            if (!toBool(left)) {
                /* Still need to parse right side for syntax, but skip evaluation */
                ExprValue right = parseBitwiseOr(p);
                (void)right;
                left = makeInt(0);
            } else {
                ExprValue right = parseBitwiseOr(p);
                if (p->error) return left;
                left = makeInt(toBool(right));
            }
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Logical OR
 * ======================================================================== */

static ExprValue parseLogicalOr(ExprParser *p) {
    ExprValue left = parseLogicalAnd(p);
    if (p->error) return left;

    while (!p->error) {
        skipWhitespace(p);
        if (p->pos + 1 >= p->end) break;

        if (p->pos[0] == '|' && p->pos[1] == '|') {
            p->pos += 2;

            /* Short-circuit: if left is true, don't evaluate right */
            if (toBool(left)) {
                ExprValue right = parseLogicalAnd(p);
                (void)right;
                left = makeInt(1);
            } else {
                ExprValue right = parseLogicalAnd(p);
                if (p->error) return left;
                left = makeInt(toBool(right));
            }
        } else {
            break;
        }
    }

    return left;
}

/* ========================================================================
 * Ternary (right-to-left associative)
 * ======================================================================== */

static ExprValue parseTernary(ExprParser *p) {
    ExprValue cond = parseLogicalOr(p);
    if (p->error) return cond;

    skipWhitespace(p);

    if (p->pos < p->end && *p->pos == '?') {
        p->pos++;
        ExprValue trueVal = parseTernary(p); /* Right-to-left for nesting */
        if (p->error) return cond;

        skipWhitespace(p);
        if (p->pos >= p->end || *p->pos != ':') {
            setExprError(p, "missing operator \":\"");
            return cond;
        }
        p->pos++;

        ExprValue falseVal = parseTernary(p);
        if (p->error) return cond;

        return toBool(cond) ? trueVal : falseVal;
    }

    return cond;
}

/* ========================================================================
 * Top-level Expression
 * ======================================================================== */

static ExprValue parseExpr(ExprParser *p) {
    return parseTernary(p);
}

/* ========================================================================
 * expr Command Entry Point
 * ======================================================================== */

TclResult tclCmdExpr(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"expr arg ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    /* Concatenate all args with spaces */
    void *arena = host->arenaPush(interp->hostCtx);
    size_t totalLen = 0;
    for (int i = 1; i < objc; i++) {
        size_t len;
        host->getStringPtr(objv[i], &len);
        totalLen += len + 1;
    }

    char *exprStr = host->arenaAlloc(arena, totalLen + 1, 1);
    char *p = exprStr;
    for (int i = 1; i < objc; i++) {
        size_t len;
        const char *s = host->getStringPtr(objv[i], &len);
        if (i > 1) *p++ = ' ';
        for (size_t j = 0; j < len; j++) {
            *p++ = s[j];
        }
    }
    *p = '\0';
    size_t exprLen = p - exprStr;

    /* Perform substitution on the expression */
    TclObj *substResult = tclSubstString(interp, exprStr, exprLen, TCL_SUBST_ALL);
    if (!substResult) {
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    size_t substLen;
    const char *substStr = host->getStringPtr(substResult, &substLen);

    /* Initialize parser */
    ExprParser parser;
    parser.interp = interp;
    parser.host = host;
    parser.pos = substStr;
    parser.end = substStr + substLen;
    parser.exprStart = substStr;
    parser.exprLen = substLen;
    parser.arena = arena;
    parser.error = 0;

    /* Check for empty expression (only whitespace) */
    skipWhitespace(&parser);
    if (parser.pos >= parser.end) {
        /* Empty expression */
        setExprErrorSimple(&parser, "empty expression");
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }
    parser.pos = substStr; /* Reset position for parsing */

    /* Parse and evaluate */
    ExprValue result = parseExpr(&parser);

    /* Check for trailing garbage */
    if (!parser.error) {
        skipWhitespace(&parser);
        if (parser.pos < parser.end) {
            if (*parser.pos == ')') {
                setExprErrorSimple(&parser, "unbalanced close paren");
            } else {
                setExprErrorChar(&parser, *parser.pos);
            }
        }
    }

    if (parser.error) {
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    /* Convert result to TclObj */
    TclObj *resultObj;
    if (result.type == EXPR_INT) {
        resultObj = host->newInt(result.v.i);
    } else if (result.type == EXPR_DOUBLE) {
        resultObj = host->newDouble(result.v.d);
    } else {
        resultObj = host->newString(result.v.str.s, result.v.str.len);
    }

    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, resultObj);
    return TCL_OK;
}
