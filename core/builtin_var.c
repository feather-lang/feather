/*
 * builtin_var.c - TCL Variable Command Implementations
 *
 * Variable commands: set, incr, append, unset
 */

#include "internal.h"

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

    /* Handle :: prefix for global variables */
    int forceGlobal = 0;
    if (nameLen >= 2 && name[0] == ':' && name[1] == ':') {
        name += 2;
        nameLen -= 2;
        forceGlobal = 1;
    }

    void *vars = forceGlobal ? interp->globalFrame->varsHandle : interp->currentFrame->varsHandle;

    if (objc == 3) {
        /* Setting a value */
        TclObj *value = host->dup(objv[2]);
        host->varSet(vars, name, nameLen, value);
        tclSetResult(interp, host->dup(objv[2]));
    } else {
        /* Getting a value */
        TclObj *value = host->varGet(vars, name, nameLen);

        /* Try global frame if not found and not already forced global */
        if (!value && !forceGlobal && interp->currentFrame != interp->globalFrame) {
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
