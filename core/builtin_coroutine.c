/*
 * builtin_coroutine.c - TCL Coroutine Command Implementations
 *
 * Coroutine commands: coroutine, yield, yieldto
 *
 * Coroutines allow suspending and resuming execution. When a coroutine
 * yields, it saves its state and returns to the caller. When resumed,
 * it continues from where it left off.
 */

#include "internal.h"
#include <string.h>

/* ========================================================================
 * Coroutine State
 * ======================================================================== */

typedef struct TclCoroutine {
    char       *name;           /* Coroutine name (fully qualified) */
    size_t      nameLen;        /* Length of name */
    TclFrame   *savedFrame;     /* Saved frame for coroutine */
    TclFrame   *baseFrame;      /* Frame where coroutine was created */
    int         running;        /* Currently executing */
    int         done;           /* Finished (returned or errored) */
    TclObj     *result;         /* Last result/yield value */
    TclInterp  *interp;         /* Interpreter */

    /* For initial invocation */
    TclObj    **cmdObjs;        /* Command + args to execute */
    int         cmdObjc;        /* Count of command objects */
    int         started;        /* Has first execution started? */

    /* For true CPS: continuation-based suspend/resume */
    TclAstNode *cachedAst;              /* Parsed AST (allocated in rootArena) */
    TclContinuation *continuation;      /* Saved continuation on yield */
    TclObj     *resumeValue;            /* Value to return from yield on resume */
} TclCoroutine;

/* Maximum number of active coroutines */
#define MAX_COROUTINES 64
static TclCoroutine *gCoroutines[MAX_COROUTINES];
static int gCoroutineCount = 0;

/* Current coroutine being executed (for yield to find) */
static TclCoroutine *gCurrentCoroutine = NULL;

/* Global yield flag - set by yield, checked by eval loop */
static int gYieldPending = 0;

/* Inner continuation from nested eval (body of while/for/foreach) */
static TclContinuation *gInnerContinuation = NULL;

/* Check if yield is pending (called by eval loop) */
int tclCoroYieldPending(void) {
    return gYieldPending;
}

/* Clear yield pending flag */
void tclCoroClearYield(void) {
    gYieldPending = 0;
}

/* Get/set inner continuation for nested evals */
TclContinuation *tclCoroGetInnerContinuation(void) {
    return gInnerContinuation;
}

void tclCoroSetInnerContinuation(TclContinuation *cont) {
    gInnerContinuation = cont;
}

/* Get current coroutine */
TclCoroutine *tclCoroGetCurrent(void) {
    return gCurrentCoroutine;
}

/* ========================================================================
 * Coroutine Management
 * ======================================================================== */

static TclCoroutine *coroFind(const char *name, size_t len) {
    for (int i = 0; i < gCoroutineCount; i++) {
        if (gCoroutines[i] && gCoroutines[i]->nameLen == len &&
            tclStrncmp(gCoroutines[i]->name, name, len) == 0) {
            return gCoroutines[i];
        }
    }
    return NULL;
}

static TclCoroutine *coroFindByFullName(const char *fullName, size_t len) {
    /* Try exact match first */
    TclCoroutine *c = coroFind(fullName, len);
    if (c) return c;

    /* Try with :: prefix */
    if (len >= 2 && fullName[0] == ':' && fullName[1] == ':') {
        return coroFind(fullName + 2, len - 2);
    }

    /* Try adding :: prefix */
    for (int i = 0; i < gCoroutineCount; i++) {
        if (gCoroutines[i] && gCoroutines[i]->nameLen == len + 2 &&
            gCoroutines[i]->name[0] == ':' && gCoroutines[i]->name[1] == ':' &&
            tclStrncmp(gCoroutines[i]->name + 2, fullName, len) == 0) {
            return gCoroutines[i];
        }
    }
    return NULL;
}

static void coroFree(TclCoroutine *coro) {
    if (!coro) return;

    /* Remove from global list */
    for (int i = 0; i < gCoroutineCount; i++) {
        if (gCoroutines[i] == coro) {
            gCoroutines[i] = gCoroutines[gCoroutineCount - 1];
            gCoroutines[gCoroutineCount - 1] = NULL;
            gCoroutineCount--;
            break;
        }
    }

    /* Note: we don't free the name/cmdObjs here as they're arena-allocated
     * or managed by the interpreter. The coroutine struct itself is also
     * arena-allocated in a real implementation. For now we leak. */
}

static TclCoroutine *coroCreate(TclInterp *interp, const char *name, size_t nameLen) {
    if (gCoroutineCount >= MAX_COROUTINES) {
        return NULL;
    }

    const TclHost *host = interp->host;

    /* Allocate from rootArena - persists until interpreter is freed */
    TclCoroutine *coro = host->arenaAlloc(interp->rootArena, sizeof(TclCoroutine), sizeof(void*));
    if (!coro) {
        return NULL;
    }

    /* Build fully qualified name (also in rootArena) */
    char *fullName;
    size_t fullLen;
    if (nameLen >= 2 && name[0] == ':' && name[1] == ':') {
        fullName = host->arenaStrdup(interp->rootArena, name, nameLen);
        fullLen = nameLen;
    } else {
        fullName = host->arenaAlloc(interp->rootArena, nameLen + 3, 1);
        fullName[0] = ':';
        fullName[1] = ':';
        for (size_t i = 0; i < nameLen; i++) {
            fullName[i + 2] = name[i];
        }
        fullName[nameLen + 2] = '\0';
        fullLen = nameLen + 2;
    }

    coro->name = fullName;
    coro->nameLen = fullLen;
    coro->savedFrame = NULL;
    coro->baseFrame = interp->currentFrame;
    coro->running = 0;
    coro->done = 0;
    coro->result = NULL;
    coro->interp = interp;
    coro->cmdObjs = NULL;
    coro->cmdObjc = 0;
    coro->started = 0;

    /* CPS fields */
    coro->cachedAst = NULL;
    coro->continuation = NULL;
    coro->resumeValue = NULL;

    gCoroutines[gCoroutineCount++] = coro;

    return coro;
}

/* ========================================================================
 * Coroutine Execution (CPS-based)
 *
 * Uses continuation-based evaluation for true suspend/resume semantics.
 * On yield, the entire evaluation state is saved to a TclContinuation.
 * On resume, the continuation is restored and execution continues from
 * exactly where it left off - no re-execution of prior code.
 * ======================================================================== */

/* Execute the coroutine's command (first invocation or resume) */
static TclResult coroExecute(TclCoroutine *coro, TclObj *resumeValue) {
    TclInterp *interp = coro->interp;
    const TclHost *host = interp->host;

    if (coro->done) {
        tclSetError(interp, "invalid command name", -1);
        return TCL_ERROR;
    }

    if (coro->running) {
        tclSetError(interp, "coroutine is already running", -1);
        return TCL_ERROR;
    }

    coro->running = 1;
    coro->resumeValue = resumeValue;
    TclCoroutine *prevCoro = gCurrentCoroutine;
    gCurrentCoroutine = coro;
    gYieldPending = 0;

    TclResult result;
    TclFrame *savedFrame = interp->currentFrame;

    if (!coro->started) {
        /* First invocation - set up the coroutine */
        coro->started = 1;

        if (coro->cmdObjc < 1) {
            coro->running = 0;
            gCurrentCoroutine = prevCoro;
            tclSetError(interp, "no command to execute", -1);
            return TCL_ERROR;
        }

        /* Create the coroutine frame */
        TclFrame *coroFrame = host->frameAlloc(interp->hostCtx);
        if (!coroFrame) {
            coro->running = 0;
            gCurrentCoroutine = prevCoro;
            tclSetError(interp, "out of memory", -1);
            return TCL_ERROR;
        }
        coroFrame->parent = interp->globalFrame;
        coroFrame->level = 1;
        coroFrame->flags = TCL_FRAME_PROC | TCL_FRAME_COROUTINE;
        coroFrame->procName = coro->name;
        coroFrame->invocationObjs = coro->cmdObjs;
        coroFrame->invocationCount = coro->cmdObjc;

        coro->savedFrame = coroFrame;

        /* Look up the command */
        size_t cmdLen;
        const char *cmdName = host->getStringPtr(coro->cmdObjs[0], &cmdLen);

        int builtinIdx = tclBuiltinLookup(cmdName, cmdLen);
        TclCmdInfo cmdInfo;

        if (builtinIdx >= 0) {
            cmdInfo.type = TCL_CMD_BUILTIN;
            cmdInfo.u.builtinId = builtinIdx;
        } else if (host->cmdLookup(interp->hostCtx, cmdName, cmdLen, &cmdInfo) != 0) {
            cmdInfo.type = TCL_CMD_NOT_FOUND;
        }

        if (cmdInfo.type == TCL_CMD_PROC) {
            /* Get proc definition */
            TclObj *argList = NULL;
            TclObj *body = NULL;
            if (host->procGetDef(cmdInfo.u.procHandle, &argList, &body) != 0) {
                coro->running = 0;
                gCurrentCoroutine = prevCoro;
                host->frameFree(interp->hostCtx, coroFrame);
                tclSetError(interp, "proc definition not found", -1);
                return TCL_ERROR;
            }

            /* Parse and bind arguments */
            TclObj **argSpecs = NULL;
            size_t argCount = 0;
            host->asList(argList, &argSpecs, &argCount);

            int actualArgs = coro->cmdObjc - 1;
            int hasArgs = 0;

            if (argCount > 0) {
                size_t lastArgLen;
                const char *lastArg = host->getStringPtr(argSpecs[argCount - 1], &lastArgLen);
                if (lastArgLen == 4 && tclStrncmp(lastArg, "args", 4) == 0) {
                    hasArgs = 1;
                }
            }

            int requiredArgs = (int)argCount - (hasArgs ? 1 : 0);

            /* Bind arguments to frame */
            interp->currentFrame = coroFrame;
            for (size_t i = 0; i < (size_t)requiredArgs; i++) {
                TclObj *argSpec = argSpecs[i];
                size_t argNameLen;
                const char *argName;
                TclObj *value = NULL;

                size_t listLen = host->listLength(argSpec);
                if (listLen >= 2) {
                    TclObj *nameObj = host->listIndex(argSpec, 0);
                    argName = host->getStringPtr(nameObj, &argNameLen);
                    if ((int)i < actualArgs) {
                        value = coro->cmdObjs[i + 1];
                    } else {
                        value = host->listIndex(argSpec, 1);
                    }
                } else {
                    argName = host->getStringPtr(argSpec, &argNameLen);
                    if ((int)i < actualArgs) {
                        value = coro->cmdObjs[i + 1];
                    }
                }

                if (value) {
                    host->varSet(coroFrame->varsHandle, argName, argNameLen, host->dup(value));
                }
            }

            if (hasArgs) {
                int argsStart = requiredArgs;
                int argsCount = actualArgs - argsStart;
                if (argsCount < 0) argsCount = 0;

                TclObj *argsList;
                if (argsCount > 0) {
                    argsList = host->newList(&coro->cmdObjs[argsStart + 1], argsCount);
                } else {
                    argsList = host->newString("", 0);
                }
                host->varSet(coroFrame->varsHandle, "args", 4, argsList);
            }

            /* Parse body to AST and cache in rootArena for persistence */
            size_t bodyLen;
            const char *bodyStr = host->getStringPtr(body, &bodyLen);
            coro->cachedAst = tclParseToRootArena(interp, bodyStr, bodyLen);
            if (!coro->cachedAst) {
                coro->running = 0;
                gCurrentCoroutine = prevCoro;
                host->frameFree(interp->hostCtx, coroFrame);
                tclSetError(interp, "failed to parse proc body", -1);
                return TCL_ERROR;
            }

            /* Execute with continuation support */
            TclContinuation *contOut = NULL;
            result = tclEvalAstCont(interp, coro->cachedAst, NULL, &contOut, 0);

            /* Save continuation if yield occurred */
            if (contOut) {
                coro->continuation = contOut;
            }

        } else if (cmdInfo.type == TCL_CMD_BUILTIN) {
            interp->currentFrame = coroFrame;
            const TclBuiltinEntry *entry = tclBuiltinGet(cmdInfo.u.builtinId);
            if (entry) {
                result = entry->proc(interp, coro->cmdObjc, coro->cmdObjs);
            } else {
                result = TCL_ERROR;
                tclSetError(interp, "invalid builtin", -1);
            }
        } else {
            coro->running = 0;
            gCurrentCoroutine = prevCoro;
            host->frameFree(interp->hostCtx, coroFrame);
            tclSetError(interp, "invalid command name", -1);
            return TCL_ERROR;
        }
    } else {
        /* Resume from saved continuation - true CPS, no re-execution */
        interp->currentFrame = coro->savedFrame;

        /* Set the resume value as result (for yield to return) */
        tclSetResult(interp, resumeValue ? resumeValue : host->newString("", 0));

        if (coro->continuation) {
            /* Resume from saved continuation.
             * If there's an inner continuation (yield was inside while/for body),
             * don't pop frame - the loop command will handle the inner resume.
             * Otherwise, pop the yield command frame to continue with next command. */
            int popFrame = (tclCoroGetInnerContinuation() == NULL);
            TclContinuation *contOut = NULL;
            result = tclEvalAstCont(interp, NULL, coro->continuation, &contOut, popFrame);

            /* Update continuation (new one if yielded again, NULL if done) */
            coro->continuation = contOut;
        } else {
            result = TCL_OK;
        }
    }

    interp->currentFrame = savedFrame;

    /* Check if we yielded or finished */
    if (gYieldPending) {
        /* Yielded - continuation was saved above, keep coroutine alive */
        gYieldPending = 0;
        coro->running = 0;
        gCurrentCoroutine = prevCoro;
        tclSetResult(interp, coro->result);
        return TCL_OK;
    }

    /* Finished - clean up */
    coro->done = 1;
    coro->running = 0;
    gCurrentCoroutine = prevCoro;

    if (result == TCL_RETURN) {
        result = TCL_OK;
    }
    if (result == TCL_OK) {
        coro->result = interp->result;
    }

    /* Free the coroutine frame */
    if (coro->savedFrame) {
        host->frameFree(interp->hostCtx, coro->savedFrame);
        coro->savedFrame = NULL;
    }

    tclSetResult(interp, coro->result ? coro->result : host->newString("", 0));
    return result;
}

/* ========================================================================
 * coroutine Command
 *
 * coroutine name command ?arg ...?
 *
 * Creates a new coroutine with the given name and starts executing
 * the command. Returns the first yielded value or the final result.
 * ======================================================================== */

TclResult tclCmdCoroutine(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"coroutine name command ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *name = host->getStringPtr(objv[1], &nameLen);

    /* Check if a coroutine with this name already exists */
    if (coroFindByFullName(name, nameLen)) {
        tclSetError(interp, "command already exists", -1);
        return TCL_ERROR;
    }

    /* Create the coroutine */
    TclCoroutine *coro = coroCreate(interp, name, nameLen);
    if (!coro) {
        tclSetError(interp, "cannot create coroutine", -1);
        return TCL_ERROR;
    }

    /* Store the command to execute */
    coro->cmdObjs = &objv[2];
    coro->cmdObjc = objc - 2;

    /* Execute the coroutine (starts the command) */
    TclResult result = coroExecute(coro, NULL);

    if (result != TCL_OK && !coro->done) {
        /* Error during initial execution */
        coroFree(coro);
        return result;
    }

    /* Return the first yield value or final result */
    if (coro->result) {
        tclSetResult(interp, coro->result);
    } else {
        tclSetResult(interp, host->newString("", 0));
    }

    /* If coroutine finished immediately, clean up */
    if (coro->done) {
        coroFree(coro);
    }

    return TCL_OK;
}

/* ========================================================================
 * yield Command
 *
 * yield ?value?
 *
 * Suspends the current coroutine and returns value to the caller.
 * When resumed, returns the value passed by the caller.
 * ======================================================================== */

TclResult tclCmdYield(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc > 2) {
        tclSetError(interp, "wrong # args: should be \"yield ?value?\"", -1);
        return TCL_ERROR;
    }

    /* Check if we're in a coroutine */
    if (!gCurrentCoroutine) {
        tclSetError(interp, "yield can only be called inside a coroutine", -1);
        return TCL_ERROR;
    }

    TclCoroutine *coro = gCurrentCoroutine;

    /* Get the yield value */
    TclObj *value;
    if (objc == 2) {
        value = objv[1];
    } else {
        value = host->newString("", 0);
    }

    /* Signal yield - continuation saving is handled by tclEvalAstCont */
    coro->result = value;
    gYieldPending = 1;

    /* Result will be replaced with resume value when continued */
    tclSetResult(interp, value);

    return TCL_OK;
}

/* ========================================================================
 * yieldto Command
 *
 * yieldto command ?arg ...?
 *
 * Suspends the current coroutine and calls command. The return value
 * of command becomes the result returned to the coroutine caller.
 * When the coroutine is resumed, the arguments passed become yield's
 * return value.
 * ======================================================================== */

TclResult tclCmdYieldto(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"yieldto command ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    /* Check if we're in a coroutine */
    if (!gCurrentCoroutine) {
        tclSetError(interp, "yieldto can only be called inside a coroutine", -1);
        return TCL_ERROR;
    }

    /* Execute the command */
    size_t cmdLen;
    const char *cmdName = host->getStringPtr(objv[1], &cmdLen);

    /* Look up command */
    int builtinIdx = tclBuiltinLookup(cmdName, cmdLen);
    TclCmdInfo cmdInfo;

    if (builtinIdx >= 0) {
        cmdInfo.type = TCL_CMD_BUILTIN;
        cmdInfo.u.builtinId = builtinIdx;
    } else if (host->cmdLookup(interp->hostCtx, cmdName, cmdLen, &cmdInfo) != 0) {
        cmdInfo.type = TCL_CMD_NOT_FOUND;
    }

    TclResult result;

    switch (cmdInfo.type) {
        case TCL_CMD_BUILTIN: {
            const TclBuiltinEntry *entry = tclBuiltinGet(cmdInfo.u.builtinId);
            if (entry) {
                result = entry->proc(interp, objc - 1, &objv[1]);
            } else {
                tclSetError(interp, "invalid builtin", -1);
                result = TCL_ERROR;
            }
            break;
        }
        case TCL_CMD_PROC: {
            TclObj *argList = NULL;
            TclObj *body = NULL;
            if (host->procGetDef(cmdInfo.u.procHandle, &argList, &body) != 0) {
                tclSetError(interp, "proc definition not found", -1);
                result = TCL_ERROR;
                break;
            }
            result = tclEvalObj(interp, body, 0);
            break;
        }
        case TCL_CMD_EXTENSION:
            result = host->extInvoke(interp, cmdInfo.u.extHandle, objc - 1, &objv[1]);
            break;
        default:
            tclSetError(interp, "invalid command name", -1);
            result = TCL_ERROR;
            break;
    }

    if (result != TCL_OK) {
        return result;
    }

    TclCoroutine *coro = gCurrentCoroutine;

    /* Signal yield - continuation saving is handled by tclEvalAstCont */
    coro->result = interp->result;
    gYieldPending = 1;

    return TCL_OK;
}

/* ========================================================================
 * Coroutine Invocation (when calling the coroutine by name)
 *
 * This is called when the coroutine command is invoked to resume it.
 * ======================================================================== */

TclResult tclCoroInvoke(TclInterp *interp, TclCoroutine *coro, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (coro->done) {
        /* Build error message */
        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, coro->nameLen + 30, 1);
        char *p = msg;
        const char *prefix = "invalid command name \"";
        while (*prefix) *p++ = *prefix++;
        /* Use short name (without ::) */
        const char *shortName = coro->name;
        size_t shortLen = coro->nameLen;
        if (shortLen >= 2 && shortName[0] == ':' && shortName[1] == ':') {
            shortName += 2;
            shortLen -= 2;
        }
        for (size_t i = 0; i < shortLen; i++) *p++ = shortName[i];
        *p++ = '"';
        *p = '\0';
        tclSetError(interp, msg, p - msg);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    /* Get resume value (arguments passed to coroutine) */
    TclObj *resumeValue = NULL;
    if (objc > 1) {
        /* Concatenate all arguments into a single value */
        if (objc == 2) {
            resumeValue = objv[1];
        } else {
            /* Create a list of all args */
            resumeValue = host->newList(&objv[1], objc - 1);
        }
    }

    /* Resume the coroutine */
    return coroExecute(coro, resumeValue);
}

/* ========================================================================
 * Lookup function for coroutine commands
 * ======================================================================== */

TclCoroutine *tclCoroLookup(const char *name, size_t len) {
    return coroFindByFullName(name, len);
}

/* ========================================================================
 * Get current coroutine name (for info coroutine)
 * ======================================================================== */

const char *tclCoroCurrentName(size_t *lenOut) {
    if (!gCurrentCoroutine) {
        if (lenOut) *lenOut = 0;
        return "";
    }
    if (lenOut) *lenOut = gCurrentCoroutine->nameLen;
    return gCurrentCoroutine->name;
}
