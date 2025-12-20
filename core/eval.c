/*
 * eval.c - TCL Evaluator
 *
 * Evaluates scripts by parsing to AST and walking the tree structure.
 * Uses an explicit state stack for non-recursive evaluation,
 * enabling coroutine suspend/resume by saving the stack state.
 */

#include "internal.h"
#include "ast.h"

/* ========================================================================
 * Interpreter Creation and Destruction
 * ======================================================================== */

TclInterp *tclInterpNew(const TclHost *host, void *hostCtx) {
    /* Allocate interpreter struct using host arena temporarily */
    void *arena = host->arenaPush(hostCtx);
    TclInterp *interp = host->arenaAlloc(arena, sizeof(TclInterp), sizeof(void*));
    if (!interp) {
        host->arenaPop(hostCtx, arena);
        return NULL;
    }

    /* Copy to permanent storage (we need interp to outlive arena) */
    /* For now, use a simple static pool approach */
    static TclInterp interpPool[8];
    static int interpCount = 0;
    if (interpCount >= 8) {
        host->arenaPop(hostCtx, arena);
        return NULL;
    }

    host->arenaPop(hostCtx, arena);
    interp = &interpPool[interpCount++];

    /* Initialize */
    interp->host = host;
    interp->hostCtx = hostCtx;

    /* Create root arena for interpreter-lifetime objects (coroutines, continuations).
     * This arena is NOT popped until tclInterpFree. */
    interp->rootArena = host->arenaPush(hostCtx);
    interp->result = NULL;
    interp->resultCode = TCL_OK;
    interp->errorInfo = NULL;
    interp->errorCode = NULL;
    interp->errorLine = 0;
    interp->returnCode = TCL_OK;
    interp->returnLevel = 1;
    interp->currentCoro = NULL;
    interp->scriptFile = NULL;
    interp->scriptLine = 0;
    interp->parent = NULL;
    interp->isSafe = 0;

    /* Create global frame */
    interp->globalFrame = host->frameAlloc(hostCtx);
    if (!interp->globalFrame) {
        return NULL;
    }
    interp->globalFrame->parent = NULL;
    interp->globalFrame->level = 0;
    interp->globalFrame->flags = TCL_FRAME_GLOBAL;
    interp->globalFrame->procName = NULL;
    interp->globalFrame->invocationObjs = NULL;
    interp->globalFrame->invocationCount = 0;

    interp->currentFrame = interp->globalFrame;

    return interp;
}

void tclInterpFree(TclInterp *interp) {
    if (!interp) return;

    const TclHost *host = interp->host;
    void *ctx = interp->hostCtx;

    /* Free global frame */
    if (interp->globalFrame) {
        host->frameFree(ctx, interp->globalFrame);
    }

    /* Pop root arena (frees coroutines, continuations, cached ASTs) */
    if (interp->rootArena) {
        host->arenaPop(ctx, interp->rootArena);
        interp->rootArena = NULL;
    }

    /* Note: interp itself is from static pool, not freed */
}

/* ========================================================================
 * Result and Error Handling
 * ======================================================================== */

TclObj *tclGetResult(TclInterp *interp) {
    return interp->result;
}

void tclSetResult(TclInterp *interp, TclObj *result) {
    interp->result = result;
    interp->resultCode = TCL_OK;
}

void tclSetResultStr(TclInterp *interp, const char *s, size_t len) {
    if (len == (size_t)-1) {
        len = tclStrlen(s);
    }
    interp->result = interp->host->newString(s, len);
    interp->resultCode = TCL_OK;
}

void tclSetError(TclInterp *interp, const char *msg, size_t len) {
    if (len == (size_t)-1) {
        len = tclStrlen(msg);
    }
    interp->result = interp->host->newString(msg, len);
    interp->resultCode = TCL_ERROR;
}

void tclSetErrorCode(TclInterp *interp, TclObj *code) {
    interp->errorCode = code;
}

void tclAddErrorInfo(TclInterp *interp, const char *info, size_t len) {
    if (len == (size_t)-1) {
        len = tclStrlen(info);
    }

    const TclHost *host = interp->host;

    if (!interp->errorInfo) {
        interp->errorInfo = host->newString(info, len);
    } else {
        /* Append to existing error info */
        size_t existingLen;
        const char *existing = host->getStringPtr(interp->errorInfo, &existingLen);

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, existingLen + len + 1, 1);

        for (size_t i = 0; i < existingLen; i++) {
            buf[i] = existing[i];
        }
        for (size_t i = 0; i < len; i++) {
            buf[existingLen + i] = info[i];
        }
        buf[existingLen + len] = '\0';

        interp->errorInfo = host->newString(buf, existingLen + len);
        host->arenaPop(interp->hostCtx, arena);
    }
}

/* ========================================================================
 * Evaluation State
 *
 * The evaluator maintains an explicit stack of states instead of using
 * C recursion. This allows the evaluation to be suspended (for yield)
 * and resumed later by saving/restoring the state stack.
 * ======================================================================== */

/* EvalPhase is now defined in internal.h for use by continuation types */

typedef struct EvalFrame {
    EvalPhase phase;
    TclAstNode *node;           /* Current node being evaluated */
    int index;                  /* Current child index */
    TclObj **args;              /* Accumulated arguments (for commands) */
    int argCount;               /* Number of accumulated arguments */
    int argCapacity;            /* Capacity of args array */
    TclObj *result;             /* Partial result for words */
    struct EvalFrame *parent;
} EvalFrame;

/* Maximum stack depth */
enum { MAX_EVAL_DEPTH = 256 };

typedef struct EvalState {
    EvalFrame *top;             /* Top of evaluation stack */
    void *arena;                /* Arena for allocation */
    int suspended;              /* Was evaluation suspended (yield)? */
    TclObj *yieldValue;         /* Value passed to yield */
} EvalState;

/* ========================================================================
 * State Management
 * ======================================================================== */

static EvalFrame *pushFrame(TclInterp *interp, EvalState *state, EvalPhase phase, TclAstNode *node) {
    const TclHost *host = interp->host;

    EvalFrame *frame = host->arenaAlloc(state->arena, sizeof(EvalFrame), sizeof(void*));
    if (!frame) return NULL;

    frame->phase = phase;
    frame->node = node;
    frame->index = 0;
    frame->args = NULL;
    frame->argCount = 0;
    frame->argCapacity = 0;
    frame->result = NULL;
    frame->parent = state->top;

    state->top = frame;
    return frame;
}

static void popFrame(EvalState *state) {
    if (state->top) {
        state->top = state->top->parent;
    }
}

/* Add argument to command frame */
static int addArg(TclInterp *interp, EvalState *state, EvalFrame *frame, TclObj *arg) {
    const TclHost *host = interp->host;

    if (frame->argCount >= frame->argCapacity) {
        int newCap = frame->argCapacity == 0 ? 8 : frame->argCapacity * 2;
        TclObj **newArgs = host->arenaAlloc(state->arena, newCap * sizeof(TclObj*), sizeof(void*));
        if (!newArgs) return -1;

        for (int i = 0; i < frame->argCount; i++) {
            newArgs[i] = frame->args[i];
        }
        frame->args = newArgs;
        frame->argCapacity = newCap;
    }

    frame->args[frame->argCount++] = arg;
    return 0;
}

/* Concatenate result for word building */
static int appendResult(TclInterp *interp, EvalState *state, EvalFrame *frame, TclObj *part) {
    const TclHost *host = interp->host;

    if (!frame->result) {
        frame->result = part;
        return 0;
    }

    /* Concatenate strings */
    size_t len1, len2;
    const char *s1 = host->getStringPtr(frame->result, &len1);
    const char *s2 = host->getStringPtr(part, &len2);

    char *buf = host->arenaAlloc(state->arena, len1 + len2 + 1, 1);
    if (!buf) return -1;

    for (size_t i = 0; i < len1; i++) buf[i] = s1[i];
    for (size_t i = 0; i < len2; i++) buf[len1 + i] = s2[i];
    buf[len1 + len2] = '\0';

    frame->result = host->newString(buf, len1 + len2);
    return 0;
}

/* ========================================================================
 * Continuation Save/Restore (for coroutine yield/resume)
 *
 * These functions save and restore the evaluation state across yields.
 * State is saved to TclContinuation (allocated from rootArena) on yield,
 * and restored on resume. This enables true CPS - no re-execution.
 * ======================================================================== */

/* Save a single EvalFrame to a TclContFrame (persistent) */
static TclContFrame *saveContFrame(TclInterp *interp, EvalFrame *evalFrame) {
    const TclHost *host = interp->host;

    TclContFrame *cont = host->arenaAlloc(interp->rootArena,
                                          sizeof(TclContFrame), sizeof(void*));
    if (!cont) return NULL;

    cont->phase = evalFrame->phase;
    cont->node = evalFrame->node;
    cont->index = evalFrame->index;
    cont->argCount = evalFrame->argCount;
    cont->argCapacity = evalFrame->argCapacity;
    cont->result = evalFrame->result ? host->dup(evalFrame->result) : NULL;
    cont->parent = NULL;

    /* Copy args array to rootArena and dup each object */
    if (evalFrame->argCount > 0) {
        cont->args = host->arenaAlloc(interp->rootArena,
                                      evalFrame->argCount * sizeof(TclObj*),
                                      sizeof(void*));
        if (!cont->args) return NULL;

        for (int i = 0; i < evalFrame->argCount; i++) {
            cont->args[i] = host->dup(evalFrame->args[i]);
        }
    } else {
        cont->args = NULL;
    }

    return cont;
}

/* Save entire EvalState stack to TclContinuation */
TclContinuation *tclContSave(TclInterp *interp, EvalState *state, TclAstNode *ast) {
    const TclHost *host = interp->host;

    TclContinuation *cont = host->arenaAlloc(interp->rootArena,
                                             sizeof(TclContinuation), sizeof(void*));
    if (!cont) return NULL;

    cont->ast = ast;
    cont->execFrame = interp->currentFrame;

    /* Walk eval stack from top to bottom, building continuation chain */
    EvalFrame *frame = state->top;
    TclContFrame *prevCont = NULL;
    TclContFrame *topCont = NULL;

    while (frame) {
        TclContFrame *contFrame = saveContFrame(interp, frame);
        if (!contFrame) return NULL;

        if (!topCont) {
            topCont = contFrame;
        }
        if (prevCont) {
            prevCont->parent = contFrame;
        }
        prevCont = contFrame;
        frame = frame->parent;
    }

    cont->top = topCont;
    return cont;
}

/* Restore EvalState stack from TclContinuation */
static EvalFrame *restoreFromContinuation(TclInterp *interp, EvalState *state,
                                          TclContinuation *cont) {
    const TclHost *host = interp->host;

    /* Walk continuation chain and rebuild EvalFrame stack.
     * We need to reverse the order (continuation is top-to-bottom,
     * but we build stack by pushing, which would reverse it).
     *
     * Strategy: collect frames in array first, then push in reverse order.
     */
    int frameCount = 0;
    TclContFrame *cf = cont->top;
    while (cf) {
        frameCount++;
        cf = cf->parent;
    }

    if (frameCount == 0) return NULL;

    /* Allocate temp array for frames */
    TclContFrame **frames = host->arenaAlloc(state->arena,
                                             frameCount * sizeof(TclContFrame*),
                                             sizeof(void*));
    if (!frames) return NULL;

    cf = cont->top;
    for (int i = 0; i < frameCount; i++) {
        frames[i] = cf;
        cf = cf->parent;
    }

    /* Push frames in reverse order (bottom of stack first) */
    for (int i = frameCount - 1; i >= 0; i--) {
        TclContFrame *contFrame = frames[i];

        EvalFrame *evalFrame = pushFrame(interp, state, contFrame->phase, contFrame->node);
        if (!evalFrame) return NULL;

        evalFrame->index = contFrame->index;
        evalFrame->result = contFrame->result;

        /* Restore args */
        if (contFrame->argCount > 0) {
            evalFrame->args = host->arenaAlloc(state->arena,
                                               contFrame->argCount * sizeof(TclObj*),
                                               sizeof(void*));
            if (!evalFrame->args) return NULL;

            for (int j = 0; j < contFrame->argCount; j++) {
                evalFrame->args[j] = contFrame->args[j];
            }
            evalFrame->argCount = contFrame->argCount;
            evalFrame->argCapacity = contFrame->argCount;
        }
    }

    return state->top;
}

/* ========================================================================
 * Variable Lookup
 * ======================================================================== */

static TclObj *lookupVar(TclInterp *interp, const char *name, size_t len) {
    const TclHost *host = interp->host;

    /* Try current frame first */
    TclObj *val = host->varGet(interp->currentFrame->varsHandle, name, len);
    if (val) return val;

    /* Try global frame if different */
    if (interp->currentFrame != interp->globalFrame) {
        val = host->varGet(interp->globalFrame->varsHandle, name, len);
    }

    return val;
}

/* ========================================================================
 * Command Dispatch
 * ======================================================================== */

static TclResult dispatchCommand(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    size_t cmdLen;
    const char *cmdName = host->getStringPtr(objv[0], &cmdLen);

    /* Check builtins first */
    int builtinIdx = tclBuiltinLookup(cmdName, cmdLen);
    if (builtinIdx >= 0) {
        const TclBuiltinEntry *entry = tclBuiltinGet(builtinIdx);
        if (entry) {
            return entry->proc(interp, objc, objv);
        }
    }

    /* Try host command lookup (procs, extensions) */
    TclCmdInfo cmdInfo;
    if (host->cmdLookup(interp->hostCtx, cmdName, cmdLen, &cmdInfo) == 0) {
        switch (cmdInfo.type) {
            case TCL_CMD_PROC: {
                TclObj *argList = NULL;
                TclObj *body = NULL;
                if (host->procGetDef(cmdInfo.u.procHandle, &argList, &body) != 0) {
                    tclSetError(interp, "proc definition not found", -1);
                    return TCL_ERROR;
                }

                /* Parse argument specification */
                TclObj **argSpecs = NULL;
                size_t argCount = 0;
                if (host->asList(argList, &argSpecs, &argCount) != 0) {
                    tclSetError(interp, "invalid argument list", -1);
                    return TCL_ERROR;
                }

                /* Create proc frame */
                TclFrame *procFrame = host->frameAlloc(interp->hostCtx);
                if (!procFrame) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
                procFrame->parent = interp->currentFrame;
                procFrame->level = interp->currentFrame->level + 1;
                procFrame->flags = TCL_FRAME_PROC;
                procFrame->procName = cmdName;
                procFrame->invocationObjs = objv;
                procFrame->invocationCount = objc;

                /* Check for 'args' parameter */
                int hasArgs = 0;
                if (argCount > 0) {
                    size_t lastLen;
                    const char *lastName = host->getStringPtr(argSpecs[argCount - 1], &lastLen);
                    if (lastLen == 4 && tclStrncmp(lastName, "args", 4) == 0) {
                        hasArgs = 1;
                    }
                }

                int requiredArgs = (int)argCount - (hasArgs ? 1 : 0);
                int actualArgs = objc - 1;

                /* Count args with defaults */
                int minArgs = 0;
                for (size_t i = 0; i < (size_t)requiredArgs; i++) {
                    if (host->listLength(argSpecs[i]) < 2) {
                        minArgs++;
                    }
                }

                if (actualArgs < minArgs || (!hasArgs && actualArgs > requiredArgs)) {
                    tclSetError(interp, "wrong # args", -1);
                    host->frameFree(interp->hostCtx, procFrame);
                    return TCL_ERROR;
                }

                /* Bind arguments */
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
                            value = objv[i + 1];
                        } else {
                            value = host->listIndex(argSpec, 1);
                        }
                    } else {
                        argName = host->getStringPtr(argSpec, &argNameLen);
                        if ((int)i < actualArgs) {
                            value = objv[i + 1];
                        }
                    }

                    if (value) {
                        host->varSet(procFrame->varsHandle, argName, argNameLen, host->dup(value));
                    }
                }

                if (hasArgs) {
                    int argsStart = requiredArgs;
                    int argsCount = actualArgs - argsStart;
                    if (argsCount < 0) argsCount = 0;

                    TclObj *argsList;
                    if (argsCount > 0) {
                        argsList = host->newList(&objv[argsStart + 1], argsCount);
                    } else {
                        argsList = host->newString("", 0);
                    }
                    host->varSet(procFrame->varsHandle, "args", 4, argsList);
                }

                /* Execute body */
                TclFrame *savedFrame = interp->currentFrame;
                interp->currentFrame = procFrame;

                TclResult result = tclEvalObj(interp, body, 0);

                if (result == TCL_RETURN) {
                    result = TCL_OK;
                }

                interp->currentFrame = savedFrame;
                host->frameFree(interp->hostCtx, procFrame);
                return result;
            }

            case TCL_CMD_EXTENSION:
                return host->extInvoke(interp, cmdInfo.u.extHandle, objc, objv);

            default:
                break;
        }
    }

    /* Check coroutines */
    TclCoroutine *coro = tclCoroLookup(cmdName, cmdLen);
    if (coro) {
        return tclCoroInvoke(interp, coro, objc, objv);
    }

    /* Command not found */
    void *arena = host->arenaPush(interp->hostCtx);
    char *msg = host->arenaAlloc(arena, cmdLen + 30, 1);
    char *p = msg;
    const char *prefix = "invalid command name \"";
    while (*prefix) *p++ = *prefix++;
    for (size_t i = 0; i < cmdLen; i++) *p++ = cmdName[i];
    *p++ = '"';
    *p = '\0';
    tclSetError(interp, msg, p - msg);
    host->arenaPop(interp->hostCtx, arena);
    return TCL_ERROR;
}

/* ========================================================================
 * Evaluation Step
 *
 * Processes one step of evaluation. Returns:
 * - TCL_OK: Evaluation complete
 * - TCL_ERROR: Error occurred
 * - TCL_CONTINUE: More work to do (internal use)
 * ======================================================================== */

static TclResult evalStep(TclInterp *interp, EvalState *state) {
    const TclHost *host = interp->host;

    if (!state->top) {
        return TCL_OK; /* Stack empty, done */
    }

    EvalFrame *frame = state->top;

    switch (frame->phase) {
        case EVAL_PHASE_SCRIPT: {
            TclAstNode *script = frame->node;

            if (frame->index >= script->u.script.count) {
                /* Script done */
                popFrame(state);
                return TCL_CONTINUE;
            }

            /* Push frame for next command */
            TclAstNode *cmd = script->u.script.cmds[frame->index++];
            if (!pushFrame(interp, state, EVAL_PHASE_COMMAND, cmd)) {
                tclSetError(interp, "out of memory", -1);
                return TCL_ERROR;
            }
            return TCL_CONTINUE;
        }

        case EVAL_PHASE_COMMAND: {
            TclAstNode *cmd = frame->node;

            if (frame->index >= cmd->u.command.count) {
                /* All words evaluated, dispatch command */
                TclResult result = dispatchCommand(interp, frame->argCount, frame->args);

                /* Handle control flow */
                if (result == TCL_ERROR || result == TCL_RETURN ||
                    result == TCL_BREAK || result == TCL_CONTINUE) {
                    return result;
                }

                /* Check for yield */
                if (tclCoroYieldPending()) {
                    state->suspended = 1;
                    return TCL_OK;
                }

                popFrame(state);
                return TCL_CONTINUE;
            }

            /* Push frame for next word */
            TclAstNode *wordNode = cmd->u.command.words[frame->index++];

            /* Handle expand nodes */
            if (wordNode->type == TCL_NODE_EXPAND) {
                /* Push word evaluation for inner node */
                if (!pushFrame(interp, state, EVAL_PHASE_WORD, wordNode->u.expand.word)) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
                /* Mark that we need to expand the result */
                state->top->phase = EVAL_PHASE_WORD;
                /* TODO: Handle expansion after word evaluation */
            } else if (wordNode->type == TCL_NODE_LITERAL) {
                /* Literal - just add as argument */
                TclObj *arg = host->newString(wordNode->u.literal.value, wordNode->u.literal.len);
                if (addArg(interp, state, frame, arg) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            } else if (wordNode->type == TCL_NODE_VAR_SIMPLE) {
                /* Variable reference */
                TclObj *val = lookupVar(interp, wordNode->u.varSimple.name, wordNode->u.varSimple.len);
                if (!val) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, wordNode->u.varSimple.len + 30, 1);
                    char *p = msg;
                    const char *prefix = "can't read \"";
                    while (*prefix) *p++ = *prefix++;
                    for (size_t i = 0; i < wordNode->u.varSimple.len; i++) *p++ = wordNode->u.varSimple.name[i];
                    const char *suffix = "\": no such variable";
                    while (*suffix) *p++ = *suffix++;
                    *p = '\0';
                    tclSetError(interp, msg, p - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    return TCL_ERROR;
                }
                if (addArg(interp, state, frame, val) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            } else {
                /* Complex word - push word frame */
                if (!pushFrame(interp, state, EVAL_PHASE_WORD, wordNode)) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            }
            return TCL_CONTINUE;
        }

        case EVAL_PHASE_WORD: {
            TclAstNode *word = frame->node;

            if (word->type == TCL_NODE_LITERAL) {
                /* Simple literal */
                frame->result = host->newString(word->u.literal.value, word->u.literal.len);
                goto word_done;
            }

            if (word->type == TCL_NODE_BACKSLASH) {
                /* Backslash escape */
                frame->result = host->newString(word->u.backslash.value, word->u.backslash.len);
                goto word_done;
            }

            if (word->type == TCL_NODE_VAR_SIMPLE) {
                TclObj *val = lookupVar(interp, word->u.varSimple.name, word->u.varSimple.len);
                if (!val) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, word->u.varSimple.len + 30, 1);
                    char *p = msg;
                    const char *prefix = "can't read \"";
                    while (*prefix) *p++ = *prefix++;
                    for (size_t i = 0; i < word->u.varSimple.len; i++) *p++ = word->u.varSimple.name[i];
                    const char *suffix = "\": no such variable";
                    while (*suffix) *p++ = *suffix++;
                    *p = '\0';
                    tclSetError(interp, msg, p - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    return TCL_ERROR;
                }
                frame->result = val;
                goto word_done;
            }

            if (word->type == TCL_NODE_VAR_ARRAY) {
                /* Array reference - first evaluate index */
                if (frame->index == 0) {
                    frame->index = 1;
                    if (word->u.varArray.index) {
                        if (!pushFrame(interp, state, EVAL_PHASE_WORD, word->u.varArray.index)) {
                            tclSetError(interp, "out of memory", -1);
                            return TCL_ERROR;
                        }
                        return TCL_CONTINUE;
                    }
                }

                /* Index evaluated, look up array element */
                const char *arrName = word->u.varArray.name;
                size_t arrLen = word->u.varArray.nameLen;
                size_t indexLen;
                const char *indexStr = "";
                if (frame->result) {
                    indexStr = host->getStringPtr(frame->result, &indexLen);
                } else {
                    indexLen = 0;
                }

                /* Build array(index) key */
                char *key = host->arenaAlloc(state->arena, arrLen + indexLen + 3, 1);
                char *p = key;
                for (size_t i = 0; i < arrLen; i++) *p++ = arrName[i];
                *p++ = '(';
                for (size_t i = 0; i < indexLen; i++) *p++ = indexStr[i];
                *p++ = ')';
                *p = '\0';

                TclObj *val = lookupVar(interp, key, p - key);
                if (!val) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, (p - key) + 30, 1);
                    char *mp = msg;
                    const char *prefix = "can't read \"";
                    while (*prefix) *mp++ = *prefix++;
                    for (char *k = key; k < p; k++) *mp++ = *k;
                    const char *suffix = "\": no such variable";
                    while (*suffix) *mp++ = *suffix++;
                    *mp = '\0';
                    tclSetError(interp, msg, mp - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    return TCL_ERROR;
                }
                frame->result = val;
                goto word_done;
            }

            if (word->type == TCL_NODE_CMD_SUBST) {
                /* Command substitution */
                if (frame->index == 0) {
                    frame->index = 1;
                    if (word->u.cmdSubst.script) {
                        if (!pushFrame(interp, state, EVAL_PHASE_SCRIPT, word->u.cmdSubst.script)) {
                            tclSetError(interp, "out of memory", -1);
                            return TCL_ERROR;
                        }
                        return TCL_CONTINUE;
                    }
                }
                /* Script evaluated, result is in interp->result */
                frame->result = interp->result ? interp->result : host->newString("", 0);
                goto word_done;
            }

            if (word->type == TCL_NODE_WORD) {
                /* Composite word - evaluate parts */
                if (frame->index >= word->u.word.count) {
                    /* All parts done */
                    if (!frame->result) {
                        frame->result = host->newString("", 0);
                    }
                    goto word_done;
                }

                TclAstNode *part = word->u.word.parts[frame->index++];

                /* Handle simple parts inline */
                if (part->type == TCL_NODE_LITERAL) {
                    TclObj *lit = host->newString(part->u.literal.value, part->u.literal.len);
                    if (appendResult(interp, state, frame, lit) != 0) {
                        tclSetError(interp, "out of memory", -1);
                        return TCL_ERROR;
                    }
                    return TCL_CONTINUE;
                }

                if (part->type == TCL_NODE_BACKSLASH) {
                    TclObj *bs = host->newString(part->u.backslash.value, part->u.backslash.len);
                    if (appendResult(interp, state, frame, bs) != 0) {
                        tclSetError(interp, "out of memory", -1);
                        return TCL_ERROR;
                    }
                    return TCL_CONTINUE;
                }

                /* Complex part - push frame */
                if (!pushFrame(interp, state, EVAL_PHASE_WORD, part)) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
                return TCL_CONTINUE;
            }

            /* Unknown node type */
            tclSetError(interp, "internal error: unknown node type", -1);
            return TCL_ERROR;

        word_done:
            /* Word evaluation complete */
            popFrame(state);

            /* If parent is a command, add as argument */
            if (state->top && state->top->phase == EVAL_PHASE_COMMAND) {
                if (addArg(interp, state, state->top, frame->result) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            }
            /* If parent is a word, append to result */
            else if (state->top && state->top->phase == EVAL_PHASE_WORD) {
                if (appendResult(interp, state, state->top, frame->result) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            }
            return TCL_CONTINUE;
        }

        default:
            tclSetError(interp, "internal error: invalid eval phase", -1);
            return TCL_ERROR;
    }
}

/* ========================================================================
 * High-Level Eval Functions
 * ======================================================================== */

static TclResult evalAstWithContinuation(TclInterp *interp, TclAstNode *ast,
                                         TclContinuation *resume,
                                         TclContinuation **contOut,
                                         int popFrameOnResume) {
    const TclHost *host = interp->host;

    if (!ast && !resume) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Initialize state */
    void *arena = host->arenaPush(interp->hostCtx);
    EvalState state;
    state.top = NULL;
    state.arena = arena;
    state.suspended = 0;
    state.yieldValue = NULL;

    if (resume) {
        /* Restore state from continuation */
        if (!restoreFromContinuation(interp, &state, resume)) {
            host->arenaPop(interp->hostCtx, arena);
            tclSetError(interp, "failed to restore continuation", -1);
            return TCL_ERROR;
        }
        /* Use AST from continuation */
        ast = resume->ast;
        /* Restore execution frame */
        interp->currentFrame = resume->execFrame;

        /* Pop the command frame if requested.
         * For inner resumes (from while/for body), we want to skip the yield command
         * and continue with the next command. For outer resumes, we don't pop so
         * the loop command gets re-dispatched. */
        if (popFrameOnResume && state.top && state.top->phase == EVAL_PHASE_COMMAND) {
            popFrame(&state);
        }
    } else {
        /* Fresh evaluation - push initial frame */
        if (ast->type == TCL_NODE_SCRIPT) {
            if (!pushFrame(interp, &state, EVAL_PHASE_SCRIPT, ast)) {
                host->arenaPop(interp->hostCtx, arena);
                tclSetError(interp, "out of memory", -1);
                return TCL_ERROR;
            }
        } else {
            /* Single node - wrap in evaluation */
            if (!pushFrame(interp, &state, EVAL_PHASE_WORD, ast)) {
                host->arenaPop(interp->hostCtx, arena);
                tclSetError(interp, "out of memory", -1);
                return TCL_ERROR;
            }
        }
        /* Set default result */
        tclSetResult(interp, host->newString("", 0));
    }

    /* Run evaluation loop */
    TclResult result;
    while (1) {
        result = evalStep(interp, &state);

        if (result == TCL_CONTINUE) {
            continue;
        }

        if (state.suspended) {
            /* Yield occurred - save continuation for resume */
            if (contOut) {
                *contOut = tclContSave(interp, &state, ast);
            }
            break;
        }

        break;
    }

    host->arenaPop(interp->hostCtx, arena);
    return result == TCL_CONTINUE ? TCL_OK : result;
}

/* Original evalAst for backward compatibility */
static TclResult evalAst(TclInterp *interp, TclAstNode *ast) {
    return evalAstWithContinuation(interp, ast, NULL, NULL, 0);
}

TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len, int flags) {
    const TclHost *host = interp->host;

    if (len == 0 || !script) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Handle TCL_EVAL_GLOBAL flag */
    TclFrame *savedFrame = NULL;
    if (flags & TCL_EVAL_GLOBAL) {
        savedFrame = interp->currentFrame;
        interp->currentFrame = interp->globalFrame;
    }

    /* Parse script to AST.
     * If in coroutine, use rootArena so AST persists across yields. */
    TclResult result;
    int inCoroutine = (tclCoroGetCurrent() != NULL);

    if (inCoroutine) {
        /* In coroutine - parse to persistent arena */
        TclAstNode *ast = tclAstParse(interp, interp->rootArena, script, len);
        if (!ast) {
            if (savedFrame) {
                interp->currentFrame = savedFrame;
            }
            tclSetError(interp, "parse error", -1);
            return TCL_ERROR;
        }

        TclContinuation *contOut = NULL;
        result = evalAstWithContinuation(interp, ast, NULL, &contOut, 0);
        if (contOut) {
            /* Yield occurred - save inner continuation for resume */
            tclCoroSetInnerContinuation(contOut);
        }
    } else {
        /* Not in coroutine - use temporary arena */
        void *arena = host->arenaPush(interp->hostCtx);
        TclAstNode *ast = tclAstParse(interp, arena, script, len);

        if (!ast) {
            host->arenaPop(interp->hostCtx, arena);
            if (savedFrame) {
                interp->currentFrame = savedFrame;
            }
            tclSetError(interp, "parse error", -1);
            return TCL_ERROR;
        }

        result = evalAst(interp, ast);
        host->arenaPop(interp->hostCtx, arena);
    }

    /* Restore frame if we switched to global */
    if (savedFrame) {
        interp->currentFrame = savedFrame;
    }

    return result;
}

TclResult tclEvalObjv(TclInterp *interp, int objc, TclObj **objv, int flags) {
    /* Handle TCL_EVAL_GLOBAL flag */
    TclFrame *savedFrame = NULL;
    if (flags & TCL_EVAL_GLOBAL) {
        savedFrame = interp->currentFrame;
        interp->currentFrame = interp->globalFrame;
    }

    TclResult result = dispatchCommand(interp, objc, objv);

    /* Restore frame if we switched to global */
    if (savedFrame) {
        interp->currentFrame = savedFrame;
    }

    return result;
}

TclResult tclEvalBracketed(TclInterp *interp, const char *cmd, size_t len) {
    /* Save current result */
    TclObj *savedResult = interp->result;
    TclResult savedCode = interp->resultCode;

    /* Evaluate bracketed command in current scope */
    TclResult result = tclEvalScript(interp, cmd, len, 0);

    if (result != TCL_OK) {
        /* Keep error result */
        return result;
    }

    /* Command succeeded - result is in interp->result */
    /* Restore saved code but keep new result */
    (void)savedResult;
    (void)savedCode;

    return TCL_OK;
}

/* ========================================================================
 * Public API (from tclc.h)
 * ======================================================================== */

TclResult tclEvalObj(TclInterp *interp, TclObj *script, int flags) {
    size_t len;
    const char *str = interp->host->getStringPtr(script, &len);
    return tclEvalScript(interp, str, len, flags);
}

/* ========================================================================
 * Continuation-based Evaluation (for coroutines)
 *
 * These functions allow coroutine code to evaluate with continuation
 * save/restore support for true CPS semantics.
 * ======================================================================== */

/* Parse script to AST and allocate in rootArena for persistence across yields */
TclAstNode *tclParseToRootArena(TclInterp *interp, const char *script, size_t len) {
    return tclAstParse(interp, interp->rootArena, script, len);
}

/* Evaluate AST with continuation support.
 * - If resume is non-NULL, continues from saved continuation
 * - If contOut is non-NULL and yield occurs, saves continuation there
 * - popFrameOnResume: if true and resuming, pop the command frame to skip
 *   the yield command and continue with the next command
 * Returns TCL_OK on completion or yield, TCL_ERROR on error.
 */
TclResult tclEvalAstCont(TclInterp *interp, TclAstNode *ast,
                         TclContinuation *resume, TclContinuation **contOut,
                         int popFrameOnResume) {
    return evalAstWithContinuation(interp, ast, resume, contOut, popFrameOnResume);
}
