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
            /* Build error message: can't read "varname": no such variable */
            void *arena = host->arenaPush(interp->hostCtx);
            char *errBuf = host->arenaAlloc(arena, nameLen + 50, 1);
            char *ep = errBuf;
            const char *prefix = "can't read \"";
            while (*prefix) *ep++ = *prefix++;
            for (size_t i = 0; i < nameLen; i++) *ep++ = name[i];
            const char *suffix = "\": no such variable";
            while (*suffix) *ep++ = *suffix++;
            *ep = '\0';
            tclSetError(interp, errBuf, ep - errBuf);
            host->arenaPop(interp->hostCtx, arena);
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
 * expr Command - Implementation in builtin_expr.c
 * ======================================================================== */

/* tclCmdExpr is defined in builtin_expr.c */

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
 * incr Command
 * ======================================================================== */

TclResult tclCmdIncr(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"incr varName ?increment?\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *name = host->getStringPtr(objv[1], &nameLen);
    void *vars = interp->currentFrame->varsHandle;

    /* Get increment value (default 1) */
    int64_t increment = 1;
    if (objc == 3) {
        if (host->asInt(objv[2], &increment) != 0) {
            tclSetError(interp, "expected integer but got non-integer value", -1);
            return TCL_ERROR;
        }
    }

    /* Get current value (or 0 if doesn't exist) */
    int64_t currentVal = 0;
    TclObj *current = host->varGet(vars, name, nameLen);
    if (!current && interp->currentFrame != interp->globalFrame) {
        current = host->varGet(interp->globalFrame->varsHandle, name, nameLen);
    }

    if (current) {
        if (host->asInt(current, &currentVal) != 0) {
            tclSetError(interp, "expected integer but got non-integer value", -1);
            return TCL_ERROR;
        }
    }

    /* Calculate new value */
    int64_t newVal = currentVal + increment;

    /* Store and return */
    TclObj *result = host->newInt(newVal);
    host->varSet(vars, name, nameLen, host->dup(result));
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * append Command
 * ======================================================================== */

TclResult tclCmdAppend(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"append varName ?value ...?\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *name = host->getStringPtr(objv[1], &nameLen);
    void *vars = interp->currentFrame->varsHandle;

    /* Get current value (or empty if doesn't exist) */
    TclObj *current = host->varGet(vars, name, nameLen);
    if (!current && interp->currentFrame != interp->globalFrame) {
        current = host->varGet(interp->globalFrame->varsHandle, name, nameLen);
    }

    size_t currentLen = 0;
    const char *currentStr = "";
    if (current) {
        currentStr = host->getStringPtr(current, &currentLen);
    }

    /* Calculate total length */
    size_t totalLen = currentLen;
    for (int i = 2; i < objc; i++) {
        size_t len;
        host->getStringPtr(objv[i], &len);
        totalLen += len;
    }

    /* Build result */
    void *arena = host->arenaPush(interp->hostCtx);
    char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
    char *p = buf;

    /* Copy current value */
    for (size_t i = 0; i < currentLen; i++) {
        *p++ = currentStr[i];
    }

    /* Append all values */
    for (int i = 2; i < objc; i++) {
        size_t len;
        const char *s = host->getStringPtr(objv[i], &len);
        for (size_t j = 0; j < len; j++) {
            *p++ = s[j];
        }
    }
    *p = '\0';

    TclObj *result = host->newString(buf, totalLen);
    host->arenaPop(interp->hostCtx, arena);

    host->varSet(vars, name, nameLen, host->dup(result));
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * unset Command
 * ======================================================================== */

TclResult tclCmdUnset(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    int nocomplain = 0;
    int argStart = 1;

    /* Parse options */
    while (argStart < objc) {
        size_t len;
        const char *arg = host->getStringPtr(objv[argStart], &len);

        if (len == 11 && tclStrncmp(arg, "-nocomplain", 11) == 0) {
            nocomplain = 1;
            argStart++;
        } else if (len == 2 && tclStrncmp(arg, "--", 2) == 0) {
            argStart++;
            break;
        } else {
            break;
        }
    }

    void *vars = interp->currentFrame->varsHandle;

    /* Unset each variable */
    for (int i = argStart; i < objc; i++) {
        size_t nameLen;
        const char *name = host->getStringPtr(objv[i], &nameLen);

        /* Check if variable exists */
        if (!host->varExists(vars, name, nameLen)) {
            if (interp->currentFrame != interp->globalFrame) {
                if (host->varExists(interp->globalFrame->varsHandle, name, nameLen)) {
                    host->varUnset(interp->globalFrame->varsHandle, name, nameLen);
                    continue;
                }
            }
            if (!nocomplain) {
                tclSetError(interp, "can't unset: no such variable", -1);
                return TCL_ERROR;
            }
        } else {
            host->varUnset(vars, name, nameLen);
        }
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * array Command
 * ======================================================================== */

TclResult tclCmdArray(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"array subcommand arrayName ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* array exists arrayName */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "exists", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"array exists arrayName\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Check if any array elements exist */
        size_t size = host->arraySize(vars, arrName, arrLen);
        if (size == 0 && interp->currentFrame != interp->globalFrame) {
            size = host->arraySize(interp->globalFrame->varsHandle, arrName, arrLen);
        }

        tclSetResult(interp, host->newInt(size > 0 ? 1 : 0));
        return TCL_OK;
    }

    /* array names arrayName ?pattern? */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "names", 5) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"array names arrayName ?pattern?\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        void *vars = interp->currentFrame->varsHandle;
        TclObj *names = host->arrayNames(vars, arrName, arrLen, pattern);
        if (!names && interp->currentFrame != interp->globalFrame) {
            names = host->arrayNames(interp->globalFrame->varsHandle, arrName, arrLen, pattern);
        }

        tclSetResult(interp, names ? names : host->newString("", 0));
        return TCL_OK;
    }

    /* array size arrayName */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "size", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"array size arrayName\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        void *vars = interp->currentFrame->varsHandle;

        size_t size = host->arraySize(vars, arrName, arrLen);
        if (size == 0 && interp->currentFrame != interp->globalFrame) {
            size = host->arraySize(interp->globalFrame->varsHandle, arrName, arrLen);
        }

        tclSetResult(interp, host->newInt((int64_t)size));
        return TCL_OK;
    }

    /* array get arrayName ?pattern? */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "get", 3) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"array get arrayName ?pattern?\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        void *vars = interp->currentFrame->varsHandle;

        /* Get array names */
        TclObj *names = host->arrayNames(vars, arrName, arrLen, pattern);
        if (!names && interp->currentFrame != interp->globalFrame) {
            vars = interp->globalFrame->varsHandle;
            names = host->arrayNames(vars, arrName, arrLen, pattern);
        }

        if (!names) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Parse names and build key-value list */
        TclObj **nameList;
        size_t nameCount;
        if (host->asList(names, &nameList, &nameCount) != 0 || nameCount == 0) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Build result list with key value pairs */
        void *arena = host->arenaPush(interp->hostCtx);
        size_t resultCount = nameCount * 2;
        TclObj **resultElems = host->arenaAlloc(arena, resultCount * sizeof(TclObj*), sizeof(void*));

        for (size_t i = 0; i < nameCount; i++) {
            size_t keyLen;
            const char *key = host->getStringPtr(nameList[i], &keyLen);
            resultElems[i * 2] = nameList[i];
            resultElems[i * 2 + 1] = host->arrayGet(vars, arrName, arrLen, key, keyLen);
        }

        TclObj *result = host->newList(resultElems, resultCount);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* array set arrayName list */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "set", 3) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"array set arrayName list\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);

        /* Parse list */
        TclObj **elems;
        size_t elemCount;
        if (host->asList(objv[3], &elems, &elemCount) != 0) {
            tclSetError(interp, "list must have an even number of elements", -1);
            return TCL_ERROR;
        }

        if (elemCount % 2 != 0) {
            tclSetError(interp, "list must have an even number of elements", -1);
            return TCL_ERROR;
        }

        void *vars = interp->currentFrame->varsHandle;

        /* Set each key-value pair */
        for (size_t i = 0; i < elemCount; i += 2) {
            size_t keyLen;
            const char *key = host->getStringPtr(elems[i], &keyLen);
            host->arraySet(vars, arrName, arrLen, key, keyLen, host->dup(elems[i + 1]));
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* array unset arrayName ?pattern? */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "unset", 5) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"array unset arrayName ?pattern?\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        void *vars = interp->currentFrame->varsHandle;

        if (pattern == NULL) {
            /* Unset entire array - get all names and unset each */
            TclObj *names = host->arrayNames(vars, arrName, arrLen, NULL);
            if (names) {
                TclObj **nameList;
                size_t nameCount;
                if (host->asList(names, &nameList, &nameCount) == 0) {
                    for (size_t i = 0; i < nameCount; i++) {
                        size_t keyLen;
                        const char *key = host->getStringPtr(nameList[i], &keyLen);
                        host->arrayUnset(vars, arrName, arrLen, key, keyLen);
                    }
                }
            }
        } else {
            /* Unset matching elements */
            TclObj *names = host->arrayNames(vars, arrName, arrLen, pattern);
            if (names) {
                TclObj **nameList;
                size_t nameCount;
                if (host->asList(names, &nameList, &nameCount) == 0) {
                    for (size_t i = 0; i < nameCount; i++) {
                        size_t keyLen;
                        const char *key = host->getStringPtr(nameList[i], &keyLen);
                        host->arrayUnset(vars, arrName, arrLen, key, keyLen);
                    }
                }
            }
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    tclSetError(interp, "unknown or ambiguous subcommand", -1);
    return TCL_ERROR;
}

/* ========================================================================
 * info Command
 * ======================================================================== */

TclResult tclCmdInfo(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"info subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* info exists varName */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "exists", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"info exists varName\"", -1);
            return TCL_ERROR;
        }
        size_t nameLen;
        const char *name = host->getStringPtr(objv[2], &nameLen);

        /* Handle :: prefix for global variables */
        int forceGlobal = 0;
        if (nameLen >= 2 && name[0] == ':' && name[1] == ':') {
            name += 2;
            nameLen -= 2;
            forceGlobal = 1;
        }

        void *vars = forceGlobal ? interp->globalFrame->varsHandle : interp->currentFrame->varsHandle;

        int exists = host->varExists(vars, name, nameLen);
        if (!exists && !forceGlobal && interp->currentFrame != interp->globalFrame) {
            exists = host->varExists(interp->globalFrame->varsHandle, name, nameLen);
        }

        tclSetResult(interp, host->newInt(exists ? 1 : 0));
        return TCL_OK;
    }

    tclSetError(interp, "unknown or ambiguous subcommand", -1);
    return TCL_ERROR;
}

/* ========================================================================
 * error Command
 * ======================================================================== */

TclResult tclCmdError(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 4) {
        tclSetError(interp, "wrong # args: should be \"error message ?info? ?code?\"", -1);
        return TCL_ERROR;
    }

    /* Get the error message */
    size_t msgLen;
    const char *msg = host->getStringPtr(objv[1], &msgLen);

    /* Set the error message */
    tclSetError(interp, msg, (int)msgLen);

    /* Handle optional info argument (errorInfo) */
    if (objc >= 3) {
        size_t infoLen;
        const char *info = host->getStringPtr(objv[2], &infoLen);
        if (infoLen > 0) {
            /* Set errorInfo directly instead of letting it accumulate */
            interp->errorInfo = host->newString(info, infoLen);
        }
    }

    /* Handle optional code argument (errorCode) */
    if (objc >= 4) {
        tclSetErrorCode(interp, objv[3]);
    }

    return TCL_ERROR;
}

/* ========================================================================
 * catch Command
 * ======================================================================== */

TclResult tclCmdCatch(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 4) {
        tclSetError(interp, "wrong # args: should be \"catch script ?resultVarName? ?optionsVarName?\"", -1);
        return TCL_ERROR;
    }

    /* Get the script to execute */
    size_t scriptLen;
    const char *script = host->getStringPtr(objv[1], &scriptLen);

    /* Execute the script and capture the result code */
    TclResult code = tclEvalScript(interp, script, scriptLen);

    /* Set global errorInfo and errorCode variables after catching an error */
    if (code == TCL_ERROR) {
        void *globalVars = interp->globalFrame->varsHandle;

        /* Set ::errorInfo */
        if (interp->errorInfo) {
            host->varSet(globalVars, "errorInfo", 9, host->dup(interp->errorInfo));
        } else if (interp->result) {
            host->varSet(globalVars, "errorInfo", 9, host->dup(interp->result));
        }

        /* Set ::errorCode */
        if (interp->errorCode) {
            host->varSet(globalVars, "errorCode", 9, host->dup(interp->errorCode));
        } else {
            host->varSet(globalVars, "errorCode", 9, host->newString("NONE", 4));
        }
    }

    /* Store result in variable if requested */
    if (objc >= 3) {
        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Store the result (or error message) */
        TclObj *resultValue = interp->result ? host->dup(interp->result) : host->newString("", 0);
        host->varSet(vars, varName, varLen, resultValue);
    }

    /* Store options dict in variable if requested */
    if (objc >= 4) {
        size_t optVarLen;
        const char *optVarName = host->getStringPtr(objv[3], &optVarLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Build a simple options dict with -code and -level */
        /* For now, just store -code as a simple value */
        void *arena = host->arenaPush(interp->hostCtx);
        char buf[64];
        int len = 0;
        buf[len++] = '-'; buf[len++] = 'c'; buf[len++] = 'o'; buf[len++] = 'd'; buf[len++] = 'e';
        buf[len++] = ' ';
        if (code == TCL_OK) buf[len++] = '0';
        else if (code == TCL_ERROR) buf[len++] = '1';
        else if (code == TCL_RETURN) buf[len++] = '2';
        else if (code == TCL_BREAK) buf[len++] = '3';
        else if (code == TCL_CONTINUE) buf[len++] = '4';
        buf[len++] = ' ';
        buf[len++] = '-'; buf[len++] = 'l'; buf[len++] = 'e'; buf[len++] = 'v';
        buf[len++] = 'e'; buf[len++] = 'l'; buf[len++] = ' '; buf[len++] = '0';

        TclObj *optValue = host->newString(buf, len);
        host->arenaPop(interp->hostCtx, arena);
        host->varSet(vars, optVarName, optVarLen, optValue);
    }

    /* Return the code as an integer - catch always succeeds */
    tclSetResult(interp, host->newInt((int64_t)code));
    return TCL_OK;
}

/* ========================================================================
 * throw Command
 * ======================================================================== */

TclResult tclCmdThrow(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 3) {
        tclSetError(interp, "wrong # args: should be \"throw type message\"", -1);
        return TCL_ERROR;
    }

    /* Set error code from type */
    tclSetErrorCode(interp, objv[1]);

    /* Set error message */
    size_t msgLen;
    const char *msg = host->getStringPtr(objv[2], &msgLen);
    tclSetError(interp, msg, (int)msgLen);

    return TCL_ERROR;
}

/* ========================================================================
 * try Command
 * ======================================================================== */

TclResult tclCmdTry(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"try body ?handler...? ?finally script?\"", -1);
        return TCL_ERROR;
    }

    /* Execute the body */
    size_t bodyLen;
    const char *body = host->getStringPtr(objv[1], &bodyLen);
    TclResult bodyCode = tclEvalScript(interp, body, bodyLen);
    TclObj *bodyResult = interp->result ? host->dup(interp->result) : host->newString("", 0);

    /* Look for handlers and finally clause */
    int finallyIdx = -1;
    int handlerMatched = 0;
    TclResult handlerCode = bodyCode;
    TclObj *handlerResult = bodyResult;

    int i = 2;
    while (i < objc) {
        size_t kwLen;
        const char *kw = host->getStringPtr(objv[i], &kwLen);

        if (kwLen == 7 && tclStrncmp(kw, "finally", 7) == 0) {
            /* finally clause */
            if (i + 1 >= objc) {
                tclSetError(interp, "wrong # args: finally requires a script", -1);
                return TCL_ERROR;
            }
            finallyIdx = i + 1;
            break;
        }

        if (kwLen == 2 && tclStrncmp(kw, "on", 2) == 0) {
            /* on code varList script */
            if (i + 3 >= objc) {
                tclSetError(interp, "wrong # args: on requires code varList script", -1);
                return TCL_ERROR;
            }

            if (!handlerMatched) {
                /* Parse the code */
                size_t codeLen;
                const char *codeStr = host->getStringPtr(objv[i + 1], &codeLen);
                int targetCode = -1;

                if (codeLen == 2 && tclStrncmp(codeStr, "ok", 2) == 0) targetCode = TCL_OK;
                else if (codeLen == 5 && tclStrncmp(codeStr, "error", 5) == 0) targetCode = TCL_ERROR;
                else if (codeLen == 6 && tclStrncmp(codeStr, "return", 6) == 0) targetCode = TCL_RETURN;
                else if (codeLen == 5 && tclStrncmp(codeStr, "break", 5) == 0) targetCode = TCL_BREAK;
                else if (codeLen == 8 && tclStrncmp(codeStr, "continue", 8) == 0) targetCode = TCL_CONTINUE;
                else {
                    /* Try as integer */
                    int64_t val;
                    if (host->asInt(objv[i + 1], &val) == 0) {
                        targetCode = (int)val;
                    }
                }

                if (targetCode == (int)bodyCode) {
                    handlerMatched = 1;

                    /* Bind variables from varList */
                    TclObj **varNames;
                    size_t varCount;
                    if (host->asList(objv[i + 2], &varNames, &varCount) == 0) {
                        void *vars = interp->currentFrame->varsHandle;
                        if (varCount >= 1) {
                            size_t vlen;
                            const char *vname = host->getStringPtr(varNames[0], &vlen);
                            if (vlen > 0) {
                                host->varSet(vars, vname, vlen, host->dup(bodyResult));
                            }
                        }
                        /* varCount >= 2 would be options dict - skip for now */
                    }

                    /* Execute handler script */
                    size_t scriptLen;
                    const char *script = host->getStringPtr(objv[i + 3], &scriptLen);
                    handlerCode = tclEvalScript(interp, script, scriptLen);
                    handlerResult = interp->result ? host->dup(interp->result) : host->newString("", 0);
                }
            }

            i += 4;
            continue;
        }

        if (kwLen == 4 && tclStrncmp(kw, "trap", 4) == 0) {
            /* trap pattern varList script */
            if (i + 3 >= objc) {
                tclSetError(interp, "wrong # args: trap requires pattern varList script", -1);
                return TCL_ERROR;
            }

            if (!handlerMatched && bodyCode == TCL_ERROR) {
                /* For now, just match any error with trap {} */
                size_t patLen;
                host->getStringPtr(objv[i + 1], &patLen);

                /* Empty pattern matches any error */
                int matches = (patLen == 0);
                /* TODO: Match against errorCode prefix */

                if (matches) {
                    handlerMatched = 1;

                    /* Bind variables from varList */
                    TclObj **varNames;
                    size_t varCount;
                    if (host->asList(objv[i + 2], &varNames, &varCount) == 0) {
                        void *vars = interp->currentFrame->varsHandle;
                        if (varCount >= 1) {
                            size_t vlen;
                            const char *vname = host->getStringPtr(varNames[0], &vlen);
                            if (vlen > 0) {
                                host->varSet(vars, vname, vlen, host->dup(bodyResult));
                            }
                        }
                    }

                    /* Execute handler script */
                    size_t scriptLen;
                    const char *script = host->getStringPtr(objv[i + 3], &scriptLen);
                    handlerCode = tclEvalScript(interp, script, scriptLen);
                    handlerResult = interp->result ? host->dup(interp->result) : host->newString("", 0);
                }
            }

            i += 4;
            continue;
        }

        /* Unknown keyword - could be finally without keyword in older syntax */
        break;
    }

    /* Execute finally clause if present */
    if (finallyIdx >= 0) {
        size_t finallyLen;
        const char *finallyScript = host->getStringPtr(objv[finallyIdx], &finallyLen);
        TclResult finallyCode = tclEvalScript(interp, finallyScript, finallyLen);

        /* If finally raises an error, that takes precedence */
        if (finallyCode == TCL_ERROR) {
            return TCL_ERROR;
        }
    }

    /* Set result and return appropriate code */
    tclSetResult(interp, handlerResult);
    return handlerCode;
}

/* ========================================================================
 * list Command
 * ======================================================================== */

TclResult tclCmdList(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Create a list from all arguments (skip command name) */
    if (objc <= 1) {
        /* Empty list */
        tclSetResult(interp, host->newString("", 0));
    } else {
        TclObj *result = host->newList(objv + 1, objc - 1);
        tclSetResult(interp, result);
    }

    return TCL_OK;
}

/* ========================================================================
 * llength Command
 * ======================================================================== */

TclResult tclCmdLlength(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 2) {
        tclSetError(interp, "wrong # args: should be \"llength list\"", -1);
        return TCL_ERROR;
    }

    size_t len = host->listLength(objv[1]);
    tclSetResult(interp, host->newInt((int64_t)len));
    return TCL_OK;
}

/* ========================================================================
 * lindex Command
 * ======================================================================== */

/* Parse an index value (supports "end", "end-N", and integers) */
static int parseListIndex(const TclHost *host, TclObj *indexObj, size_t listLen, size_t *out) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    /* Check for "end" */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (listLen == 0) {
            *out = 0;
            return -1;  /* Out of bounds */
        }
        if (idxLen == 3) {
            *out = listLen - 1;
            return 0;
        }
        /* end-N or end+N */
        if (idxLen > 4 && idxStr[3] == '-') {
            int64_t offset = 0;
            for (size_t i = 4; i < idxLen; i++) {
                if (idxStr[i] >= '0' && idxStr[i] <= '9') {
                    offset = offset * 10 + (idxStr[i] - '0');
                } else {
                    return -1;
                }
            }
            if (offset > (int64_t)(listLen - 1)) {
                *out = 0;
                return -1;
            }
            *out = listLen - 1 - (size_t)offset;
            return 0;
        }
        return -1;
    }

    /* Try as integer */
    int64_t idx;
    if (host->asInt(indexObj, &idx) != 0) {
        return -1;
    }

    if (idx < 0 || (size_t)idx >= listLen) {
        *out = 0;
        return -1;  /* Out of bounds */
    }

    *out = (size_t)idx;
    return 0;
}

/* Helper to apply a single index to current value */
static TclObj *applyIndex(const TclHost *host, TclObj *current, TclObj *indexObj) {
    size_t idxStrLen;
    host->getStringPtr(indexObj, &idxStrLen);
    if (idxStrLen == 0) {
        return current;  /* Empty index, return as-is */
    }

    size_t listLen = host->listLength(current);
    size_t idx;

    if (parseListIndex(host, indexObj, listLen, &idx) != 0) {
        return NULL;  /* Out of bounds or invalid */
    }

    return host->listIndex(current, idx);
}

TclResult tclCmdLindex(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lindex list ?index ...?\"", -1);
        return TCL_ERROR;
    }

    /* If no index, return the list itself */
    if (objc == 2) {
        tclSetResult(interp, host->dup(objv[1]));
        return TCL_OK;
    }

    /* Start with the list */
    TclObj *current = objv[1];

    /* Process each index argument */
    for (int i = 2; i < objc; i++) {
        size_t idxStrLen;
        host->getStringPtr(objv[i], &idxStrLen);

        /* Empty index - return current value */
        if (idxStrLen == 0) {
            continue;
        }

        /* Check if this index argument is itself a list of indices */
        TclObj **idxElems;
        size_t idxCount;
        if (host->asList(objv[i], &idxElems, &idxCount) == 0 && idxCount > 1) {
            /* It's a list of indices - apply each one */
            for (size_t j = 0; j < idxCount; j++) {
                current = applyIndex(host, current, idxElems[j]);
                if (!current) {
                    tclSetResult(interp, host->newString("", 0));
                    return TCL_OK;
                }
            }
        } else {
            /* Single index */
            current = applyIndex(host, current, objv[i]);
            if (!current) {
                tclSetResult(interp, host->newString("", 0));
                return TCL_OK;
            }
        }
    }

    tclSetResult(interp, host->dup(current));
    return TCL_OK;
}

/* ========================================================================
 * lrange Command
 * ======================================================================== */

TclResult tclCmdLrange(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 4) {
        tclSetError(interp, "wrong # args: should be \"lrange list first last\"", -1);
        return TCL_ERROR;
    }

    size_t listLen = host->listLength(objv[1]);

    /* Parse first index */
    size_t first = 0;
    if (parseListIndex(host, objv[2], listLen, &first) != 0) {
        /* Check if it's negative - treat as 0 */
        int64_t val;
        if (host->asInt(objv[2], &val) == 0 && val < 0) {
            first = 0;
        } else {
            /* Check for end-N that went negative */
            size_t idxLen;
            const char *idxStr = host->getStringPtr(objv[2], &idxLen);
            if (idxLen >= 5 && tclStrncmp(idxStr, "end-", 4) == 0) {
                first = 0;  /* end-N that goes negative -> treat as 0 for first */
            } else if (listLen == 0) {
                tclSetResult(interp, host->newString("", 0));
                return TCL_OK;
            }
        }
    }

    /* Parse last index */
    size_t last = listLen > 0 ? listLen - 1 : 0;
    int lastNegative = 0;
    if (parseListIndex(host, objv[3], listLen, &last) != 0) {
        /* Check if it's past end - treat as end */
        int64_t val;
        if (host->asInt(objv[3], &val) == 0) {
            if (val >= (int64_t)listLen) {
                last = listLen > 0 ? listLen - 1 : 0;
            } else if (val < 0) {
                lastNegative = 1;
            }
        } else {
            /* Check for end-N that went negative */
            size_t idxLen;
            const char *idxStr = host->getStringPtr(objv[3], &idxLen);
            if (idxLen >= 5 && tclStrncmp(idxStr, "end-", 4) == 0) {
                lastNegative = 1;  /* end-N that goes negative for last */
            }
        }
    }

    /* If last is negative or first > last, return empty */
    if (lastNegative || first > last || listLen == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Use host's listRange */
    TclObj *result = host->listRange(objv[1], first, last);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * lappend Command
 * ======================================================================== */

TclResult tclCmdLappend(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lappend varName ?value ...?\"", -1);
        return TCL_ERROR;
    }

    /* Get variable name */
    size_t varLen;
    const char *varName = host->getStringPtr(objv[1], &varLen);
    void *vars = interp->currentFrame->varsHandle;

    /* Get current value or empty list */
    TclObj *current = host->varGet(vars, varName, varLen);
    if (!current) {
        current = host->newString("", 0);
        /* If variable doesn't exist, create it */
        host->varSet(vars, varName, varLen, host->dup(current));
    }

    /* If no values to append, just return current value */
    if (objc == 2) {
        tclSetResult(interp, host->dup(current));
        return TCL_OK;
    }

    /* Append each value */
    TclObj *result = current;
    for (int i = 2; i < objc; i++) {
        result = host->listAppend(result, objv[i]);
    }

    /* Set the variable */
    host->varSet(vars, varName, varLen, result);

    tclSetResult(interp, host->dup(result));
    return TCL_OK;
}

/* ========================================================================
 * join Command
 * ======================================================================== */

TclResult tclCmdJoin(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"join list ?joinString?\"", -1);
        return TCL_ERROR;
    }

    /* Get join string (default is space) */
    const char *joinStr = " ";
    size_t joinLen = 1;
    if (objc == 3) {
        joinStr = host->getStringPtr(objv[2], &joinLen);
    }

    /* Parse list */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(objv[1], &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    /* Empty list returns empty string */
    if (elemCount == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Calculate total length */
    size_t totalLen = 0;
    for (size_t i = 0; i < elemCount; i++) {
        size_t len;
        host->getStringPtr(elems[i], &len);
        totalLen += len;
        if (i > 0) totalLen += joinLen;
    }

    /* Build result using arena */
    void *arena = host->arenaPush(interp->hostCtx);
    char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
    char *p = buf;

    for (size_t i = 0; i < elemCount; i++) {
        if (i > 0) {
            for (size_t j = 0; j < joinLen; j++) {
                *p++ = joinStr[j];
            }
        }
        size_t len;
        const char *s = host->getStringPtr(elems[i], &len);
        for (size_t j = 0; j < len; j++) {
            *p++ = s[j];
        }
    }
    *p = '\0';

    TclObj *result = host->newString(buf, totalLen);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * split Command
 * ======================================================================== */

TclResult tclCmdSplit(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"split string ?splitChars?\"", -1);
        return TCL_ERROR;
    }

    /* Get string to split */
    size_t strLen;
    const char *str = host->getStringPtr(objv[1], &strLen);

    /* Get split characters (default is whitespace) */
    const char *splitChars = " \t\n\r";
    size_t splitLen = 4;
    if (objc == 3) {
        splitChars = host->getStringPtr(objv[2], &splitLen);
    }

    /* Empty string returns empty list */
    if (strLen == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* If splitChars is empty, split into individual characters */
    if (splitLen == 0) {
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **elems = host->arenaAlloc(arena, strLen * sizeof(TclObj*), sizeof(void*));
        for (size_t i = 0; i < strLen; i++) {
            elems[i] = host->newString(&str[i], 1);
        }
        TclObj *result = host->newList(elems, strLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* Count elements first */
    size_t maxElems = strLen + 1;  /* Maximum possible elements */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **elems = host->arenaAlloc(arena, maxElems * sizeof(TclObj*), sizeof(void*));
    size_t elemCount = 0;

    /* Helper to check if char is a split char */
    const char *start = str;
    const char *end = str + strLen;
    const char *p = str;

    while (p <= end) {
        int isSplit = 0;
        if (p < end) {
            for (size_t i = 0; i < splitLen; i++) {
                if (*p == splitChars[i]) {
                    isSplit = 1;
                    break;
                }
            }
        }

        if (isSplit || p == end) {
            /* Add element from start to p */
            elems[elemCount++] = host->newString(start, p - start);
            start = p + 1;
        }
        p++;
    }

    TclObj *result = host->newList(elems, elemCount);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * lsort Command
 * ======================================================================== */

TclResult tclCmdLsort(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lsort ?options? list\"", -1);
        return TCL_ERROR;
    }

    /* Parse options */
    /* flags: 1=decreasing, 2=integer, 4=nocase, 8=unique, 16=dictionary, 32=real */
    int flags = 0;
    int listIdx = objc - 1;  /* List is last argument */

    for (int i = 1; i < objc - 1; i++) {
        size_t optLen;
        const char *opt = host->getStringPtr(objv[i], &optLen);

        if (optLen > 0 && opt[0] == '-') {
            if (optLen == 11 && tclStrncmp(opt, "-decreasing", 11) == 0) {
                flags |= 1;
            } else if (optLen == 11 && tclStrncmp(opt, "-increasing", 11) == 0) {
                flags &= ~1;
            } else if (optLen == 8 && tclStrncmp(opt, "-integer", 8) == 0) {
                flags |= 2;
                flags &= ~(16 | 32);  /* Clear dictionary and real */
            } else if (optLen == 6 && tclStrncmp(opt, "-ascii", 6) == 0) {
                flags &= ~(2 | 16 | 32);  /* Clear integer, dictionary, real */
            } else if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
                flags |= 4;
            } else if (optLen == 7 && tclStrncmp(opt, "-unique", 7) == 0) {
                flags |= 8;
            } else if (optLen == 11 && tclStrncmp(opt, "-dictionary", 11) == 0) {
                flags |= 16;
                flags &= ~(2 | 32);  /* Clear integer and real */
            } else if (optLen == 5 && tclStrncmp(opt, "-real", 5) == 0) {
                flags |= 32;
                flags &= ~(2 | 16);  /* Clear integer and dictionary */
            }
        }
    }

    TclObj *result = host->listSort(objv[listIdx], flags);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * Builtin Table
 * ======================================================================== */

/* Sorted alphabetically for binary search */
static const TclBuiltinEntry builtinTable[] = {
    {"append",   tclCmdAppend},
    {"array",    tclCmdArray},
    {"break",    tclCmdBreak},
    {"catch",    tclCmdCatch},
    {"continue", tclCmdContinue},
    {"error",    tclCmdError},
    {"expr",     tclCmdExpr},
    {"for",      tclCmdFor},
    {"foreach",  tclCmdForeach},
    {"if",       tclCmdIf},
    {"incr",     tclCmdIncr},
    {"info",     tclCmdInfo},
    {"join",     tclCmdJoin},
    {"lappend",  tclCmdLappend},
    {"lindex",   tclCmdLindex},
    {"list",     tclCmdList},
    {"llength",  tclCmdLlength},
    {"lrange",   tclCmdLrange},
    {"lsort",    tclCmdLsort},
    {"proc",     tclCmdProc},
    {"puts",     tclCmdPuts},
    {"return",   tclCmdReturn},
    {"set",      tclCmdSet},
    {"split",    tclCmdSplit},
    {"string",   tclCmdString},
    {"subst",    tclCmdSubst},
    {"throw",    tclCmdThrow},
    {"try",      tclCmdTry},
    {"unset",    tclCmdUnset},
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
