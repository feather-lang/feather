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
 * Builtin Table
 * ======================================================================== */

/* Sorted alphabetically for binary search */
static const TclBuiltinEntry builtinTable[] = {
    {"expr",   tclCmdExpr},
    {"puts",   tclCmdPuts},
    {"set",    tclCmdSet},
    {"string", tclCmdString},
    {"subst",  tclCmdSubst},
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
