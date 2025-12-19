/*
 * builtins.c - TCL Built-in Commands
 *
 * Static table of built-in commands and their implementations.
 */

#include "internal.h"

/* ========================================================================
 * puts Command
 * ======================================================================== */

TclResult tclCmdPuts(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;
    int newline = 1;
    int argStart = 1;
    TclChannel *chan = host->chanStdout(interp->hostCtx);

    /* Check for -nonewline flag */
    if (objc >= 2) {
        size_t len;
        const char *arg = host->getStringPtr(objv[1], &len);
        if (len == 10 && tclStrncmp(arg, "-nonewline", 10) == 0) {
            newline = 0;
            argStart = 2;
        }
    }

    /* Check argument count */
    int remaining = objc - argStart;
    if (remaining < 1 || remaining > 2) {
        tclSetError(interp, "wrong # args: should be \"puts ?-nonewline? ?channelId? string\"", -1);
        return TCL_ERROR;
    }

    /* Get channel and string */
    TclObj *strObj;
    if (remaining == 2) {
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[argStart], &chanLen);

        if (chanLen == 6 && tclStrncmp(chanName, "stdout", 6) == 0) {
            chan = host->chanStdout(interp->hostCtx);
        } else if (chanLen == 6 && tclStrncmp(chanName, "stderr", 6) == 0) {
            chan = host->chanStderr(interp->hostCtx);
        } else {
            tclSetError(interp, "can not find channel", -1);
            return TCL_ERROR;
        }
        strObj = objv[argStart + 1];
    } else {
        strObj = objv[argStart];
    }

    /* Write string */
    size_t strLen;
    const char *str = host->getStringPtr(strObj, &strLen);
    host->chanWrite(chan, str, strLen);
    if (newline) {
        host->chanWrite(chan, "\n", 1);
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * set Command
 * ======================================================================== */

TclResult tclCmdSet(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"set varName ?newValue?\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *name = host->getStringPtr(objv[1], &nameLen);
    void *vars = interp->currentFrame->varsHandle;

    if (objc == 3) {
        /* Setting a value */
        TclObj *value = host->dup(objv[2]);
        host->varSet(vars, name, nameLen, value);
        tclSetResult(interp, host->dup(objv[2]));
    } else {
        /* Getting a value */
        TclObj *value = host->varGet(vars, name, nameLen);

        /* Try global frame if not found */
        if (!value && interp->currentFrame != interp->globalFrame) {
            value = host->varGet(interp->globalFrame->varsHandle, name, nameLen);
        }

        if (!value) {
            tclSetError(interp, "can't read variable: no such variable", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->dup(value));
    }

    return TCL_OK;
}

/* ========================================================================
 * string Command
 * ======================================================================== */

TclResult tclCmdString(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"string subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    if (subcmdLen == 6 && tclStrncmp(subcmd, "length", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"string length string\"", -1);
            return TCL_ERROR;
        }
        size_t len;
        host->getStringPtr(objv[2], &len);
        tclSetResult(interp, host->newInt((int64_t)len));
        return TCL_OK;
    }

    if (subcmdLen == 7 && tclStrncmp(subcmd, "toupper", 7) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"string toupper string\"", -1);
            return TCL_ERROR;
        }
        size_t len;
        const char *str = host->getStringPtr(objv[2], &len);

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, len + 1, 1);
        for (size_t i = 0; i < len; i++) {
            char c = str[i];
            buf[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
        }
        buf[len] = '\0';

        TclObj *result = host->newString(buf, len);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    if (subcmdLen == 7 && tclStrncmp(subcmd, "tolower", 7) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"string tolower string\"", -1);
            return TCL_ERROR;
        }
        size_t len;
        const char *str = host->getStringPtr(objv[2], &len);

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, len + 1, 1);
        for (size_t i = 0; i < len; i++) {
            char c = str[i];
            buf[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        }
        buf[len] = '\0';

        TclObj *result = host->newString(buf, len);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    if (subcmdLen == 6 && tclStrncmp(subcmd, "repeat", 6) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"string repeat string count\"", -1);
            return TCL_ERROR;
        }
        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        int64_t count;
        if (host->asInt(objv[3], &count) != 0 || count < 0) {
            tclSetError(interp, "expected integer but got invalid value", -1);
            return TCL_ERROR;
        }

        size_t resultLen = strLen * (size_t)count;
        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, resultLen + 1, 1);

        char *p = buf;
        for (int64_t i = 0; i < count; i++) {
            for (size_t j = 0; j < strLen; j++) {
                *p++ = str[j];
            }
        }
        *p = '\0';

        TclObj *result = host->newString(buf, resultLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    tclSetError(interp, "unknown or ambiguous subcommand", -1);
    return TCL_ERROR;
}

/* ========================================================================
 * expr Command (Simple Arithmetic)
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

    char *expr = host->arenaAlloc(arena, totalLen + 1, 1);
    char *p = expr;
    for (int i = 1; i < objc; i++) {
        size_t len;
        const char *s = host->getStringPtr(objv[i], &len);
        if (i > 1) *p++ = ' ';
        for (size_t j = 0; j < len; j++) {
            *p++ = s[j];
        }
    }
    *p = '\0';

    /* Simple expression parser: integers with + - * / */
    const char *ep = expr;
    while (*ep == ' ') ep++;

    int64_t result = 0;
    int haveResult = 0;
    char op = '+';

    while (*ep) {
        while (*ep == ' ') ep++;
        if (!*ep) break;

        /* Parse number */
        int neg = 0;
        if (*ep == '-' && !haveResult) {
            neg = 1;
            ep++;
        } else if (*ep == '+' && !haveResult) {
            ep++;
        }

        int64_t num = 0;
        while (*ep >= '0' && *ep <= '9') {
            num = num * 10 + (*ep - '0');
            ep++;
        }
        if (neg) num = -num;

        if (!haveResult) {
            result = num;
            haveResult = 1;
        } else {
            switch (op) {
                case '+': result += num; break;
                case '-': result -= num; break;
                case '*': result *= num; break;
                case '/': if (num != 0) result /= num; break;
            }
        }

        while (*ep == ' ') ep++;
        if (*ep == '+' || *ep == '-' || *ep == '*' || *ep == '/') {
            op = *ep++;
        }
    }

    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, host->newInt(result));
    return TCL_OK;
}

/* ========================================================================
 * subst Command
 * ======================================================================== */

TclResult tclCmdSubst(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"subst ?-nobackslashes? ?-nocommands? ?-novariables? string\"", -1);
        return TCL_ERROR;
    }

    int flags = TCL_SUBST_ALL;
    int stringIdx = 1;

    /* Parse flags */
    for (int i = 1; i < objc - 1; i++) {
        size_t len;
        const char *arg = host->getStringPtr(objv[i], &len);

        if (len == 14 && tclStrncmp(arg, "-nobackslashes", 14) == 0) {
            flags &= ~TCL_SUBST_BACKSLASH;
            stringIdx = i + 1;
        } else if (len == 11 && tclStrncmp(arg, "-nocommands", 11) == 0) {
            flags &= ~TCL_SUBST_COMMANDS;
            stringIdx = i + 1;
        } else if (len == 12 && tclStrncmp(arg, "-novariables", 12) == 0) {
            flags &= ~TCL_SUBST_VARIABLES;
            stringIdx = i + 1;
        }
    }

    /* Get the string */
    size_t strLen;
    const char *str = host->getStringPtr(objv[stringIdx], &strLen);

    /* Perform substitution */
    TclObj *result = tclSubstString(interp, str, strLen, flags);
    if (!result) {
        return TCL_ERROR;
    }

    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * proc Command
 * ======================================================================== */

TclResult tclCmdProc(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 4) {
        tclSetError(interp, "wrong # args: should be \"proc name args body\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *name = host->getStringPtr(objv[1], &nameLen);

    /* Register the procedure with the host */
    void *handle = host->procRegister(interp->hostCtx, name, nameLen, objv[2], objv[3]);
    if (!handle) {
        tclSetError(interp, "failed to register procedure", -1);
        return TCL_ERROR;
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * return Command
 * ======================================================================== */

TclResult tclCmdReturn(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Default return value is empty string */
    TclObj *result = host->newString("", 0);
    int code = TCL_OK;
    int level = 1;

    /* Parse options: return ?-code code? ?-level level? ?result? */
    int i = 1;
    while (i < objc) {
        size_t len;
        const char *arg = host->getStringPtr(objv[i], &len);

        if (len >= 1 && arg[0] == '-') {
            if (len == 5 && tclStrncmp(arg, "-code", 5) == 0) {
                if (i + 1 >= objc) {
                    tclSetError(interp, "wrong # args: should be \"-code code\"", -1);
                    return TCL_ERROR;
                }
                i++;
                size_t codeLen;
                const char *codeStr = host->getStringPtr(objv[i], &codeLen);

                /* Parse code value */
                if (codeLen == 2 && tclStrncmp(codeStr, "ok", 2) == 0) {
                    code = TCL_OK;
                } else if (codeLen == 5 && tclStrncmp(codeStr, "error", 5) == 0) {
                    code = TCL_ERROR;
                } else if (codeLen == 6 && tclStrncmp(codeStr, "return", 6) == 0) {
                    code = TCL_RETURN;
                } else if (codeLen == 5 && tclStrncmp(codeStr, "break", 5) == 0) {
                    code = TCL_BREAK;
                } else if (codeLen == 8 && tclStrncmp(codeStr, "continue", 8) == 0) {
                    code = TCL_CONTINUE;
                } else {
                    /* Try as integer */
                    int64_t val;
                    if (host->asInt(objv[i], &val) == 0) {
                        code = (int)val;
                    } else {
                        tclSetError(interp, "bad completion code", -1);
                        return TCL_ERROR;
                    }
                }
                i++;
            } else if (len == 6 && tclStrncmp(arg, "-level", 6) == 0) {
                if (i + 1 >= objc) {
                    tclSetError(interp, "wrong # args: should be \"-level level\"", -1);
                    return TCL_ERROR;
                }
                i++;
                int64_t val;
                if (host->asInt(objv[i], &val) != 0 || val < 0) {
                    tclSetError(interp, "bad level", -1);
                    return TCL_ERROR;
                }
                level = (int)val;
                i++;
            } else {
                /* Unknown option - treat as result value */
                result = objv[i];
                i++;
            }
        } else {
            /* Result value */
            result = objv[i];
            i++;
        }
    }

    /* Set interpreter state for return */
    interp->result = result;
    interp->returnCode = code;
    interp->returnLevel = level;

    return TCL_RETURN;
}

/* ========================================================================
 * Expression Boolean Evaluation Helper
 * ======================================================================== */

/* Evaluate expression string and return boolean result */
static int evalExprBool(TclInterp *interp, const char *exprRaw, size_t lenRaw, int *result) {
    const TclHost *host = interp->host;

    /* First, perform variable/command substitution on the expression */
    TclObj *substResult = tclSubstString(interp, exprRaw, lenRaw, TCL_SUBST_ALL);
    if (!substResult) {
        return -1;
    }

    size_t len;
    const char *expr = host->getStringPtr(substResult, &len);

    /* Simple expression evaluator - handles:
     * - Integer literals (0 = false, non-zero = true)
     * - Comparisons: ==, !=, <, >, <=, >=
     * - Boolean literals: true, false, yes, no, on, off
     */
    const char *p = expr;
    const char *end = expr + len;

    /* Skip whitespace */
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end) {
        *result = 0;
        return 0;
    }

    /* Check for boolean literals */
    size_t remaining = end - p;
    if (remaining == 4 && tclStrncmp(p, "true", 4) == 0) { *result = 1; return 0; }
    if (remaining == 5 && tclStrncmp(p, "false", 5) == 0) { *result = 0; return 0; }
    if (remaining == 3 && tclStrncmp(p, "yes", 3) == 0) { *result = 1; return 0; }
    if (remaining == 2 && tclStrncmp(p, "no", 2) == 0) { *result = 0; return 0; }
    if (remaining == 2 && tclStrncmp(p, "on", 2) == 0) { *result = 1; return 0; }
    if (remaining == 3 && tclStrncmp(p, "off", 3) == 0) { *result = 0; return 0; }

    /* Try to parse as simple comparison or arithmetic */
    /* First, look for comparison operators */
    const char *op = NULL;
    int opLen = 0;
    const char *scan = p;
    while (scan < end) {
        if (scan + 1 < end) {
            if ((scan[0] == '=' && scan[1] == '=') ||
                (scan[0] == '!' && scan[1] == '=') ||
                (scan[0] == '<' && scan[1] == '=') ||
                (scan[0] == '>' && scan[1] == '=')) {
                op = scan;
                opLen = 2;
                break;
            }
        }
        if (*scan == '<' || *scan == '>') {
            op = scan;
            opLen = 1;
            break;
        }
        scan++;
    }

    if (op) {
        /* Comparison expression */
        /* Parse left operand */
        const char *leftStart = p;
        while (leftStart < op && (*leftStart == ' ' || *leftStart == '\t')) leftStart++;
        const char *leftEnd = op;
        while (leftEnd > leftStart && (leftEnd[-1] == ' ' || leftEnd[-1] == '\t')) leftEnd--;

        /* Strip quotes from left operand */
        if (leftEnd > leftStart && *leftStart == '"' && leftEnd[-1] == '"') {
            leftStart++;
            leftEnd--;
        }

        /* Parse right operand */
        const char *rightStart = op + opLen;
        while (rightStart < end && (*rightStart == ' ' || *rightStart == '\t')) rightStart++;
        const char *rightEnd = end;
        while (rightEnd > rightStart && (rightEnd[-1] == ' ' || rightEnd[-1] == '\t')) rightEnd--;

        /* Strip quotes from right operand */
        if (rightEnd > rightStart && *rightStart == '"' && rightEnd[-1] == '"') {
            rightStart++;
            rightEnd--;
        }

        /* Try as integers first */
        int64_t leftVal = 0, rightVal = 0;
        int leftIsInt = 1, rightIsInt = 1;

        /* Parse left as int */
        const char *lp = leftStart;
        int lneg = 0;
        if (lp < leftEnd && *lp == '-') { lneg = 1; lp++; }
        else if (lp < leftEnd && *lp == '+') { lp++; }
        if (lp < leftEnd && *lp >= '0' && *lp <= '9') {
            while (lp < leftEnd && *lp >= '0' && *lp <= '9') {
                leftVal = leftVal * 10 + (*lp - '0');
                lp++;
            }
            if (lneg) leftVal = -leftVal;
            /* Check for trailing non-digits */
            while (lp < leftEnd && (*lp == ' ' || *lp == '\t')) lp++;
            if (lp != leftEnd) leftIsInt = 0;
        } else {
            leftIsInt = 0;
        }

        /* Parse right as int */
        const char *rp = rightStart;
        int rneg = 0;
        if (rp < rightEnd && *rp == '-') { rneg = 1; rp++; }
        else if (rp < rightEnd && *rp == '+') { rp++; }
        if (rp < rightEnd && *rp >= '0' && *rp <= '9') {
            while (rp < rightEnd && *rp >= '0' && *rp <= '9') {
                rightVal = rightVal * 10 + (*rp - '0');
                rp++;
            }
            if (rneg) rightVal = -rightVal;
            while (rp < rightEnd && (*rp == ' ' || *rp == '\t')) rp++;
            if (rp != rightEnd) rightIsInt = 0;
        } else {
            rightIsInt = 0;
        }

        if (leftIsInt && rightIsInt) {
            /* Integer comparison */
            if (opLen == 2 && op[0] == '=' && op[1] == '=') {
                *result = (leftVal == rightVal);
            } else if (opLen == 2 && op[0] == '!' && op[1] == '=') {
                *result = (leftVal != rightVal);
            } else if (opLen == 2 && op[0] == '<' && op[1] == '=') {
                *result = (leftVal <= rightVal);
            } else if (opLen == 2 && op[0] == '>' && op[1] == '=') {
                *result = (leftVal >= rightVal);
            } else if (opLen == 1 && op[0] == '<') {
                *result = (leftVal < rightVal);
            } else if (opLen == 1 && op[0] == '>') {
                *result = (leftVal > rightVal);
            }
            return 0;
        } else {
            /* String comparison */
            size_t leftLen = leftEnd - leftStart;
            size_t rightLen = rightEnd - rightStart;
            int cmp = 0;
            size_t minLen = leftLen < rightLen ? leftLen : rightLen;
            cmp = tclStrncmp(leftStart, rightStart, minLen);
            if (cmp == 0) {
                if (leftLen < rightLen) cmp = -1;
                else if (leftLen > rightLen) cmp = 1;
            }

            if (opLen == 2 && op[0] == '=' && op[1] == '=') {
                *result = (cmp == 0);
            } else if (opLen == 2 && op[0] == '!' && op[1] == '=') {
                *result = (cmp != 0);
            } else if (opLen == 2 && op[0] == '<' && op[1] == '=') {
                *result = (cmp <= 0);
            } else if (opLen == 2 && op[0] == '>' && op[1] == '=') {
                *result = (cmp >= 0);
            } else if (opLen == 1 && op[0] == '<') {
                *result = (cmp < 0);
            } else if (opLen == 1 && op[0] == '>') {
                *result = (cmp > 0);
            }
            return 0;
        }
    }

    /* Try as simple integer (non-zero = true) */
    int64_t val = 0;
    int neg = 0;
    if (p < end && *p == '-') { neg = 1; p++; }
    else if (p < end && *p == '+') { p++; }

    if (p < end && *p >= '0' && *p <= '9') {
        while (p < end && *p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (neg) val = -val;
        *result = (val != 0);
        return 0;
    }

    /* Use expr command and interpret result */
    TclObj *exprObj = host->newString(expr, len);
    TclObj *args[2] = { host->newString("expr", 4), exprObj };
    TclResult res = tclCmdExpr(interp, 2, args);
    if (res != TCL_OK) {
        return -1;
    }

    /* Check result */
    int64_t intResult;
    if (host->asInt(interp->result, &intResult) == 0) {
        *result = (intResult != 0);
        return 0;
    }

    /* Try as boolean */
    int boolResult;
    if (host->asBool(interp->result, &boolResult) == 0) {
        *result = boolResult;
        return 0;
    }

    tclSetError(interp, "expected boolean expression", -1);
    return -1;
}

/* ========================================================================
 * if Command
 * ======================================================================== */

TclResult tclCmdIf(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"if expr1 ?then? body1 elseif expr2 ?then? body2 ... ?else? ?bodyN?\"", -1);
        return TCL_ERROR;
    }

    int i = 1;
    while (i < objc) {
        /* Get condition expression */
        size_t exprLen;
        const char *exprStr = host->getStringPtr(objv[i], &exprLen);
        i++;

        /* Evaluate condition */
        int condResult;
        if (evalExprBool(interp, exprStr, exprLen, &condResult) != 0) {
            return TCL_ERROR;
        }

        /* Skip optional "then" */
        if (i < objc) {
            size_t kwLen;
            const char *kw = host->getStringPtr(objv[i], &kwLen);
            if (kwLen == 4 && tclStrncmp(kw, "then", 4) == 0) {
                i++;
            }
        }

        if (i >= objc) {
            tclSetError(interp, "wrong # args: no body after condition", -1);
            return TCL_ERROR;
        }

        if (condResult) {
            /* Execute this branch */
            size_t bodyLen;
            const char *bodyStr = host->getStringPtr(objv[i], &bodyLen);
            return tclEvalScript(interp, bodyStr, bodyLen);
        }

        i++;  /* Skip body */

        /* Check for elseif or else */
        if (i >= objc) {
            /* No more branches, return empty */
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        size_t kwLen;
        const char *kw = host->getStringPtr(objv[i], &kwLen);

        if (kwLen == 6 && tclStrncmp(kw, "elseif", 6) == 0) {
            i++;
            continue;  /* Process next condition */
        }

        if (kwLen == 4 && tclStrncmp(kw, "else", 4) == 0) {
            i++;
            if (i >= objc) {
                tclSetError(interp, "wrong # args: no body after else", -1);
                return TCL_ERROR;
            }
            /* Execute else branch */
            size_t bodyLen;
            const char *bodyStr = host->getStringPtr(objv[i], &bodyLen);
            return tclEvalScript(interp, bodyStr, bodyLen);
        }

        /* Must be else body without "else" keyword */
        size_t bodyLen;
        const char *bodyStr = host->getStringPtr(objv[i], &bodyLen);
        return tclEvalScript(interp, bodyStr, bodyLen);
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * while Command
 * ======================================================================== */

TclResult tclCmdWhile(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 3) {
        tclSetError(interp, "wrong # args: should be \"while test command\"", -1);
        return TCL_ERROR;
    }

    size_t testLen, bodyLen;
    const char *testStr = host->getStringPtr(objv[1], &testLen);
    const char *bodyStr = host->getStringPtr(objv[2], &bodyLen);

    tclSetResult(interp, host->newString("", 0));

    while (1) {
        /* Evaluate condition */
        int condResult;
        if (evalExprBool(interp, testStr, testLen, &condResult) != 0) {
            return TCL_ERROR;
        }

        if (!condResult) {
            break;
        }

        /* Execute body */
        TclResult result = tclEvalScript(interp, bodyStr, bodyLen);

        if (result == TCL_BREAK) {
            break;
        }
        if (result == TCL_CONTINUE) {
            continue;
        }
        if (result == TCL_ERROR || result == TCL_RETURN) {
            return result;
        }
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * for Command
 * ======================================================================== */

TclResult tclCmdFor(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 5) {
        tclSetError(interp, "wrong # args: should be \"for start test next command\"", -1);
        return TCL_ERROR;
    }

    size_t startLen, testLen, nextLen, bodyLen;
    const char *startStr = host->getStringPtr(objv[1], &startLen);
    const char *testStr = host->getStringPtr(objv[2], &testLen);
    const char *nextStr = host->getStringPtr(objv[3], &nextLen);
    const char *bodyStr = host->getStringPtr(objv[4], &bodyLen);

    /* Execute initialization */
    TclResult result = tclEvalScript(interp, startStr, startLen);
    if (result != TCL_OK) {
        return result;
    }

    while (1) {
        /* Evaluate condition */
        int condResult;
        if (evalExprBool(interp, testStr, testLen, &condResult) != 0) {
            return TCL_ERROR;
        }

        if (!condResult) {
            break;
        }

        /* Execute body */
        result = tclEvalScript(interp, bodyStr, bodyLen);

        if (result == TCL_BREAK) {
            break;
        }
        if (result == TCL_CONTINUE) {
            /* Fall through to next */
        } else if (result == TCL_ERROR || result == TCL_RETURN) {
            return result;
        }

        /* Execute next */
        result = tclEvalScript(interp, nextStr, nextLen);
        if (result != TCL_OK && result != TCL_CONTINUE) {
            return result;
        }
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * foreach Command
 * ======================================================================== */

TclResult tclCmdForeach(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 4) {
        tclSetError(interp, "wrong # args: should be \"foreach varname list body\"", -1);
        return TCL_ERROR;
    }

    size_t varNameLen;
    const char *varName = host->getStringPtr(objv[1], &varNameLen);

    /* Parse list */
    TclObj **elems = NULL;
    size_t elemCount = 0;
    if (host->asList(objv[2], &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    size_t bodyLen;
    const char *bodyStr = host->getStringPtr(objv[3], &bodyLen);

    void *vars = interp->currentFrame->varsHandle;

    for (size_t i = 0; i < elemCount; i++) {
        /* Set loop variable */
        host->varSet(vars, varName, varNameLen, host->dup(elems[i]));

        /* Execute body */
        TclResult result = tclEvalScript(interp, bodyStr, bodyLen);

        if (result == TCL_BREAK) {
            break;
        }
        if (result == TCL_CONTINUE) {
            continue;
        }
        if (result == TCL_ERROR || result == TCL_RETURN) {
            return result;
        }
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * break Command
 * ======================================================================== */

TclResult tclCmdBreak(TclInterp *interp, int objc, TclObj **objv) {
    (void)objv;

    if (objc != 1) {
        tclSetError(interp, "wrong # args: should be \"break\"", -1);
        return TCL_ERROR;
    }

    tclSetResult(interp, interp->host->newString("", 0));
    return TCL_BREAK;
}

/* ========================================================================
 * continue Command
 * ======================================================================== */

TclResult tclCmdContinue(TclInterp *interp, int objc, TclObj **objv) {
    (void)objv;

    if (objc != 1) {
        tclSetError(interp, "wrong # args: should be \"continue\"", -1);
        return TCL_ERROR;
    }

    tclSetResult(interp, interp->host->newString("", 0));
    return TCL_CONTINUE;
}

/* ========================================================================
 * Builtin Table
 * ======================================================================== */

/* Sorted alphabetically for binary search */
static const TclBuiltinEntry builtinTable[] = {
    {"break",    tclCmdBreak},
    {"continue", tclCmdContinue},
    {"expr",     tclCmdExpr},
    {"for",      tclCmdFor},
    {"foreach",  tclCmdForeach},
    {"if",       tclCmdIf},
    {"proc",     tclCmdProc},
    {"puts",     tclCmdPuts},
    {"return",   tclCmdReturn},
    {"set",      tclCmdSet},
    {"string",   tclCmdString},
    {"subst",    tclCmdSubst},
    {"while",    tclCmdWhile},
};

static const int builtinCount = sizeof(builtinTable) / sizeof(builtinTable[0]);

/* Binary search for builtin command */
int tclBuiltinLookup(const char *name, size_t len) {
    int lo = 0;
    int hi = builtinCount - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const char *midName = builtinTable[mid].name;
        size_t midLen = tclStrlen(midName);

        /* Compare */
        int cmp;
        size_t minLen = len < midLen ? len : midLen;
        cmp = tclStrncmp(name, midName, minLen);
        if (cmp == 0) {
            if (len < midLen) cmp = -1;
            else if (len > midLen) cmp = 1;
        }

        if (cmp < 0) {
            hi = mid - 1;
        } else if (cmp > 0) {
            lo = mid + 1;
        } else {
            return mid;
        }
    }

    return -1;  /* Not found */
}

const TclBuiltinEntry *tclBuiltinGet(int index) {
    if (index < 0 || index >= builtinCount) {
        return NULL;
    }
    return &builtinTable[index];
}

int tclBuiltinCount(void) {
    return builtinCount;
}
