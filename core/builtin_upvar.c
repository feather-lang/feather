/*
 * builtin_upvar.c - TCL upvar Command Implementation
 *
 * The upvar command creates a link between a local variable and a variable
 * in a calling scope.
 *
 * Syntax: upvar ?level? otherVar myVar ?otherVar myVar ...?
 *
 * Level can be:
 *   - An integer N (default 1): relative level (1 = caller, 2 = caller's caller)
 *   - #N: absolute level (0 = global)
 */

#include "internal.h"

/* Helper to find the target frame based on level specification */
static TclFrame *findTargetFrame(TclInterp *interp, const char *levelStr, size_t levelLen, int *error) {
    *error = 0;

    /* Check for absolute level (#N) */
    if (levelLen > 0 && levelStr[0] == '#') {
        /* Parse absolute level */
        int64_t absLevel = 0;
        for (size_t i = 1; i < levelLen; i++) {
            char c = levelStr[i];
            if (c >= '0' && c <= '9') {
                absLevel = absLevel * 10 + (c - '0');
            } else {
                *error = 1;
                return NULL;
            }
        }

        /* Level 0 is global, higher levels are closer to current */
        if (absLevel == 0) {
            return interp->globalFrame;
        }

        /* Walk from global frame forward to find the target */
        /* absLevel N means N frames up from global */
        TclFrame *frame = interp->currentFrame;
        uint32_t currentLevel = frame->level;

        if ((uint32_t)absLevel > currentLevel) {
            *error = 1;
            return NULL;
        }

        /* Find frame at absolute level */
        while (frame && frame->level > (uint32_t)absLevel) {
            frame = frame->parent;
        }

        return frame;
    }

    /* Parse relative level (integer) */
    int64_t relLevel = 0;
    int neg = 0;
    size_t i = 0;

    if (levelLen > 0 && levelStr[0] == '-') {
        neg = 1;
        i = 1;
    }

    for (; i < levelLen; i++) {
        char c = levelStr[i];
        if (c >= '0' && c <= '9') {
            relLevel = relLevel * 10 + (c - '0');
        } else {
            *error = 1;
            return NULL;
        }
    }

    if (neg) {
        *error = 1;  /* Negative relative levels not allowed */
        return NULL;
    }

    /* Walk up the call stack relLevel times */
    TclFrame *frame = interp->currentFrame;
    for (int64_t j = 0; j < relLevel; j++) {
        if (!frame->parent) {
            *error = 1;
            return NULL;
        }
        frame = frame->parent;
    }

    return frame;
}

TclResult tclCmdUpvar(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"upvar ?level? otherVar localVar ?otherVar localVar ...?\"", -1);
        return TCL_ERROR;
    }

    /* Check if first arg is a level specification */
    int argStart = 1;
    TclFrame *targetFrame = NULL;

    size_t firstLen;
    const char *firstArg = host->getStringPtr(objv[1], &firstLen);

    /* Check if it looks like a level spec */
    int isLevel = 0;
    if (firstLen > 0) {
        if (firstArg[0] == '#') {
            isLevel = 1;
        } else if (firstArg[0] >= '0' && firstArg[0] <= '9') {
            isLevel = 1;
        } else if (firstArg[0] == '-' && firstLen > 1 && firstArg[1] >= '0' && firstArg[1] <= '9') {
            isLevel = 1;
        }
    }

    if (isLevel) {
        int error;
        targetFrame = findTargetFrame(interp, firstArg, firstLen, &error);
        if (error) {
            tclSetError(interp, "bad level", -1);
            return TCL_ERROR;
        }
        argStart = 2;
    } else {
        /* Default level is 1 (caller) */
        if (interp->currentFrame->parent) {
            targetFrame = interp->currentFrame->parent;
        } else {
            targetFrame = interp->globalFrame;
        }
    }

    /* Remaining args must be pairs of otherVar myVar */
    int remaining = objc - argStart;
    if (remaining < 2 || remaining % 2 != 0) {
        tclSetError(interp, "wrong # args: should be \"upvar ?level? otherVar localVar ?otherVar localVar ...?\"", -1);
        return TCL_ERROR;
    }

    void *localVars = interp->currentFrame->varsHandle;
    void *targetVars = targetFrame->varsHandle;

    /* Create links for each pair */
    for (int i = argStart; i < objc; i += 2) {
        size_t otherLen, localLen;
        const char *otherName = host->getStringPtr(objv[i], &otherLen);
        const char *localName = host->getStringPtr(objv[i + 1], &localLen);

        host->varLink(localVars, localName, localLen, targetVars, otherName, otherLen);
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}
