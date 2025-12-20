/*
 * builtin_uplevel.c - TCL uplevel Command Implementation
 *
 * The uplevel command executes a script in a calling scope.
 *
 * Syntax: uplevel ?level? script ?script ...?
 *
 * Level can be:
 *   - An integer N (default 1): relative level (1 = caller, 2 = caller's caller)
 *   - #N: absolute level (0 = global)
 *
 * Multiple scripts are concatenated with spaces.
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

        /* Walk from current frame back to find absolute level */
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

TclResult tclCmdUplevel(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"uplevel ?level? script ?script ...?\"", -1);
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

    /* Need at least one script argument */
    if (argStart >= objc) {
        tclSetError(interp, "wrong # args: should be \"uplevel ?level? script ?script ...?\"", -1);
        return TCL_ERROR;
    }

    /* Build the script to execute */
    TclObj *scriptObj;
    void *arena = NULL;

    if (argStart == objc - 1) {
        /* Single script argument - use directly */
        scriptObj = objv[argStart];
    } else {
        /* Concatenate multiple arguments with spaces */
        arena = host->arenaPush(interp->hostCtx);

        size_t totalLen = 0;
        for (int i = argStart; i < objc; i++) {
            size_t len;
            host->getStringPtr(objv[i], &len);
            totalLen += len;
            if (i > argStart) totalLen++;  /* space separator */
        }

        char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
        char *p = buf;

        for (int i = argStart; i < objc; i++) {
            size_t len;
            const char *s = host->getStringPtr(objv[i], &len);
            if (i > argStart) *p++ = ' ';
            for (size_t j = 0; j < len; j++) {
                *p++ = s[j];
            }
        }
        *p = '\0';

        scriptObj = host->newString(buf, totalLen);
    }

    /* Save current frame and switch to target frame */
    TclFrame *savedFrame = interp->currentFrame;
    interp->currentFrame = targetFrame;

    /* Execute the script in the target frame */
    TclResult result = tclEvalObj(interp, scriptObj, 0);

    /* Restore the original frame */
    interp->currentFrame = savedFrame;

    /* Clean up arena if we used it */
    if (arena) {
        host->arenaPop(interp->hostCtx, arena);
    }

    return result;
}
