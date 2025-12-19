/*
 * builtin_event.c - Event Loop Commands
 *
 * Implements: after, vwait, update, fileevent
 */

#include "internal.h"

/* ========================================================================
 * after Command
 *
 * after ms                       - Sleep for ms milliseconds
 * after ms script...             - Schedule script to run after ms
 * after cancel id                - Cancel by ID
 * after cancel script...         - Cancel by script
 * after idle script...           - Schedule script for idle
 * after info ?id?                - Get info about handlers
 * ======================================================================== */

TclResult tclCmdAfter(TclInterp *interp, int objc, TclObj **objv) {
    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"after option ?arg ...?\"", 0);
        return TCL_ERROR;
    }

    const TclHost *host = interp->host;
    size_t len;
    const char *arg = host->getStringPtr(objv[1], &len);

    /* Check for subcommands */
    if (len == 6 && tclStrncmp(arg, "cancel", 6) == 0) {
        /* after cancel id | after cancel script... */
        if (objc < 3) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        if (objc == 3) {
            /* after cancel id */
            size_t idLen;
            const char *idStr = host->getStringPtr(objv[2], &idLen);

            /* Check if it looks like an ID (after#N) */
            if (idLen > 6 && tclStrncmp(idStr, "after#", 6) == 0) {
                host->afterCancel(interp->hostCtx, (TclTimerToken)objv[2]);
            } else {
                /* Try cancel by script */
                host->afterCancel(interp->hostCtx, (TclTimerToken)objv[2]);
            }
        } else {
            /* after cancel script script... - concatenate and cancel */
            /* Build concatenated script */
            size_t totalLen = 0;
            for (int i = 2; i < objc; i++) {
                size_t partLen;
                host->getStringPtr(objv[i], &partLen);
                totalLen += partLen + 1;  /* +1 for space */
            }

            void *arena = host->arenaPush(interp->hostCtx);
            char *script = host->arenaAlloc(arena, totalLen + 1, 1);
            char *p = script;

            for (int i = 2; i < objc; i++) {
                size_t partLen;
                const char *part = host->getStringPtr(objv[i], &partLen);
                for (size_t j = 0; j < partLen; j++) {
                    *p++ = part[j];
                }
                if (i < objc - 1) *p++ = ' ';
            }
            *p = '\0';

            TclObj *scriptObj = host->newString(script, p - script);
            host->afterCancel(interp->hostCtx, (TclTimerToken)scriptObj);
            host->arenaPop(interp->hostCtx, arena);
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    if (len == 4 && tclStrncmp(arg, "idle", 4) == 0) {
        /* after idle script... */
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"after idle script ?script ...?\"", 0);
            return TCL_ERROR;
        }

        /* Build concatenated script */
        TclObj *script;
        if (objc == 3) {
            script = objv[2];
        } else {
            size_t totalLen = 0;
            for (int i = 2; i < objc; i++) {
                size_t partLen;
                host->getStringPtr(objv[i], &partLen);
                totalLen += partLen + 1;
            }

            void *arena = host->arenaPush(interp->hostCtx);
            char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
            char *p = buf;

            for (int i = 2; i < objc; i++) {
                size_t partLen;
                const char *part = host->getStringPtr(objv[i], &partLen);
                for (size_t j = 0; j < partLen; j++) {
                    *p++ = part[j];
                }
                if (i < objc - 1) *p++ = ' ';
            }
            *p = '\0';

            script = host->newString(buf, p - buf);
            host->arenaPop(interp->hostCtx, arena);
        }

        TclTimerToken token = host->afterIdle(interp->hostCtx, script);
        if (token) {
            tclSetResult(interp, (TclObj *)token);
        } else {
            tclSetError(interp, "couldn't create idle handler", 0);
            return TCL_ERROR;
        }
        return TCL_OK;
    }

    if (len == 4 && tclStrncmp(arg, "info", 4) == 0) {
        /* after info ?id? */
        TclObj *info = host->afterInfo(interp->hostCtx, objc > 2 ? (TclTimerToken)objv[2] : NULL);
        if (info) {
            tclSetResult(interp, info);
        } else if (objc > 2) {
            /* Invalid ID */
            size_t idLen;
            const char *idStr = host->getStringPtr(objv[2], &idLen);

            /* Build error message */
            void *arena = host->arenaPush(interp->hostCtx);
            char *msg = host->arenaAlloc(arena, 64 + idLen, 1);
            char *p = msg;
            const char *prefix = "event \"";
            while (*prefix) *p++ = *prefix++;
            for (size_t i = 0; i < idLen; i++) *p++ = idStr[i];
            const char *suffix = "\" doesn't exist";
            while (*suffix) *p++ = *suffix++;
            *p = '\0';

            tclSetError(interp, msg, p - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        } else {
            tclSetResult(interp, host->newString("", 0));
        }
        return TCL_OK;
    }

    /* Try to parse as integer (after ms or after ms script...) */
    int64_t ms;
    if (host->asInt(objv[1], &ms) != 0) {
        /* Not an integer, not a valid subcommand */
        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, 128 + len, 1);
        char *p = msg;
        const char *prefix = "bad argument \"";
        while (*prefix) *p++ = *prefix++;
        for (size_t i = 0; i < len; i++) *p++ = arg[i];
        const char *suffix = "\": must be cancel, idle, info, or an integer";
        while (*suffix) *p++ = *suffix++;
        *p = '\0';

        tclSetError(interp, msg, p - msg);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    /* Negative treated as 0 */
    if (ms < 0) ms = 0;

    if (objc == 2) {
        /* after ms - blocking sleep */
        host->afterMs(interp->hostCtx, (int)ms, NULL);
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* after ms script... - schedule timer */
    TclObj *script;
    if (objc == 3) {
        script = objv[2];
    } else {
        size_t totalLen = 0;
        for (int i = 2; i < objc; i++) {
            size_t partLen;
            host->getStringPtr(objv[i], &partLen);
            totalLen += partLen + 1;
        }

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
        char *p = buf;

        for (int i = 2; i < objc; i++) {
            size_t partLen;
            const char *part = host->getStringPtr(objv[i], &partLen);
            for (size_t j = 0; j < partLen; j++) {
                *p++ = part[j];
            }
            if (i < objc - 1) *p++ = ' ';
        }
        *p = '\0';

        script = host->newString(buf, p - buf);
        host->arenaPop(interp->hostCtx, arena);
    }

    TclTimerToken token = host->afterMs(interp->hostCtx, (int)ms, script);
    if (token) {
        tclSetResult(interp, (TclObj *)token);
    } else {
        /* Blocking sleep was performed */
        tclSetResult(interp, host->newString("", 0));
    }
    return TCL_OK;
}

/* ========================================================================
 * vwait Command
 *
 * vwait varName - Wait for variable to be written
 * ======================================================================== */

/* Global flag for vwait - set by trace callback */
static volatile int gVwaitFlag = 0;

/* Trace callback for vwait */
static void vwaitTraceCallback(void *clientData, const char *name, int op) {
    (void)clientData;
    (void)name;
    (void)op;
    gVwaitFlag = 1;
}

TclResult tclCmdVwait(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        /* TCL 9 allows vwait with no args but it returns immediately */
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    size_t varLen;
    const char *varName = host->getStringPtr(objv[1], &varLen);

    /* Get global frame for the variable */
    void *globalVars = interp->globalFrame->varsHandle;

    /* Reset the flag */
    gVwaitFlag = 0;

    /* Add trace on the variable */
    host->traceVarAdd(globalVars, varName, varLen, TCL_TRACE_WRITE,
                      vwaitTraceCallback, NULL);

    /* Process events until variable is set */
    while (!gVwaitFlag) {
        int result = host->doOneEvent(interp->hostCtx, TCL_EVENT_ALL);
        if (result == 0) {
            /* No events available - this shouldn't happen in a proper event loop */
            break;
        }
    }

    /* Remove trace */
    host->traceVarRemove(globalVars, varName, varLen, vwaitTraceCallback, NULL);

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * update Command
 *
 * update              - Process all pending events
 * update idletasks    - Process only idle callbacks
 * ======================================================================== */

TclResult tclCmdUpdate(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;
    int flags = TCL_EVENT_ALL | TCL_EVENT_NOWAIT;

    if (objc > 2) {
        tclSetError(interp, "wrong # args: should be \"update ?idletasks?\"", 0);
        return TCL_ERROR;
    }

    if (objc == 2) {
        size_t len;
        const char *opt = host->getStringPtr(objv[1], &len);
        if (len == 9 && tclStrncmp(opt, "idletasks", 9) == 0) {
            flags = TCL_EVENT_IDLE | TCL_EVENT_NOWAIT;
        } else {
            void *arena = host->arenaPush(interp->hostCtx);
            char *msg = host->arenaAlloc(arena, 64 + len, 1);
            char *p = msg;
            const char *prefix = "bad option \"";
            while (*prefix) *p++ = *prefix++;
            for (size_t i = 0; i < len; i++) *p++ = opt[i];
            const char *suffix = "\": must be idletasks";
            while (*suffix) *p++ = *suffix++;
            *p = '\0';

            tclSetError(interp, msg, p - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }
    }

    /* Process events until none pending */
    while (host->doOneEvent(interp->hostCtx, flags)) {
        /* Keep processing */
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * fileevent Command
 *
 * fileevent channel readable ?script?
 * fileevent channel writable ?script?
 * ======================================================================== */

TclResult tclCmdFileevent(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3 || objc > 4) {
        tclSetError(interp, "wrong # args: should be \"fileevent channel event ?script?\"", 0);
        return TCL_ERROR;
    }

    /* Get channel */
    size_t chanLen;
    const char *chanName = host->getStringPtr(objv[1], &chanLen);

    /* Look up channel by name */
    TclChannel *chan = NULL;
    TclObj *chanNames = host->chanNames(interp->hostCtx, NULL);
    if (chanNames) {
        size_t count = host->listLength(chanNames);
        for (size_t i = 0; i < count; i++) {
            TclObj *name = host->listIndex(chanNames, i);
            size_t nameLen;
            const char *nameStr = host->getStringPtr(name, &nameLen);
            if (nameLen == chanLen) {
                int match = 1;
                for (size_t j = 0; j < nameLen && match; j++) {
                    if (nameStr[j] != chanName[j]) match = 0;
                }
                if (match) {
                    /* Found it - need to get actual channel */
                    /* This is a bit awkward - we need the channel lookup function */
                    /* For now, we'll use a different approach */
                    break;
                }
            }
        }
    }

    /* Use external channel lookup */
    extern TclChannel *hostChanLookup(void *ctx, const char *name);
    chan = hostChanLookup(interp->hostCtx, chanName);

    if (!chan) {
        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, 64 + chanLen, 1);
        char *p = msg;
        const char *prefix = "can not find channel named \"";
        while (*prefix) *p++ = *prefix++;
        for (size_t i = 0; i < chanLen; i++) *p++ = chanName[i];
        *p++ = '"';
        *p = '\0';

        tclSetError(interp, msg, p - msg);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    /* Get event type */
    size_t eventLen;
    const char *event = host->getStringPtr(objv[2], &eventLen);
    int mask = 0;

    if (eventLen == 8 && tclStrncmp(event, "readable", 8) == 0) {
        mask = TCL_READABLE;
    } else if (eventLen == 8 && tclStrncmp(event, "writable", 8) == 0) {
        mask = TCL_WRITABLE;
    } else {
        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, 64 + eventLen, 1);
        char *p = msg;
        const char *prefix = "bad event name \"";
        while (*prefix) *p++ = *prefix++;
        for (size_t i = 0; i < eventLen; i++) *p++ = event[i];
        const char *suffix = "\": must be readable or writable";
        while (*suffix) *p++ = *suffix++;
        *p = '\0';

        tclSetError(interp, msg, p - msg);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    if (objc == 4) {
        /* Set handler */
        host->fileeventSet(interp->hostCtx, chan, mask, objv[3]);
        tclSetResult(interp, host->newString("", 0));
    } else {
        /* Get handler */
        TclObj *script = host->fileeventGet(interp->hostCtx, chan, mask);
        if (script) {
            tclSetResult(interp, script);
        } else {
            tclSetResult(interp, host->newString("", 0));
        }
    }

    return TCL_OK;
}
