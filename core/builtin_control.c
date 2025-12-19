/*
 * builtin_control.c - TCL Control Flow Command Implementations
 *
 * Control flow commands: if, while, for, foreach
 * Also includes the evalExprBool helper function.
 */

#include "internal.h"

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
