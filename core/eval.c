/*
 * eval.c - TCL Evaluation Trampoline
 *
 * Non-recursive command evaluation state machine.
 * Supports suspend/resume for coroutines.
 */

#include "internal.h"

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
 * Eval State Management
 * ======================================================================== */

void tclEvalStateInit(TclEvalState *state, TclInterp *interp,
                      const char *script, size_t len) {
    tclParserInit(&state->parser, interp, script, len);
    state->phase = EVAL_PHASE_PARSE;
    state->substWords = NULL;
    state->substCount = 0;
    state->wordIndex = 0;
    state->currentCmd.words = NULL;
    state->currentCmd.wordCount = 0;
}

void tclEvalStateCleanup(TclEvalState *state, TclInterp *interp) {
    (void)interp;
    tclParserCleanup(&state->parser);
}

/* ========================================================================
 * Trampoline Evaluation
 * ======================================================================== */

TclEvalStatus tclEvalStep(TclInterp *interp, TclEvalState *state) {
    const TclHost *host = interp->host;

    switch (state->phase) {
        case EVAL_PHASE_PARSE: {
            /* Parse next command */
            int result = tclParserNextCommand(&state->parser, &state->currentCmd);

            if (result < 0) {
                /* Error */
                state->phase = EVAL_PHASE_DONE;
                return EVAL_DONE;
            }

            if (result > 0 || state->currentCmd.wordCount == 0) {
                /* EOF or empty command */
                state->phase = EVAL_PHASE_DONE;
                return EVAL_DONE;
            }

            /* Prepare for substitution */
            state->substWords = host->arenaAlloc(state->parser.arena,
                                                  state->currentCmd.wordCount * sizeof(TclObj*),
                                                  sizeof(void*));
            state->substCount = 0;
            state->wordIndex = 0;
            state->phase = EVAL_PHASE_SUBST;
            return EVAL_CONTINUE;
        }

        case EVAL_PHASE_SUBST: {
            /* Substitute each word */
            while (state->wordIndex < state->currentCmd.wordCount) {
                TclWord *word = &state->currentCmd.words[state->wordIndex];
                TclObj *substed = tclSubstWord(interp, word, TCL_SUBST_ALL);

                if (!substed) {
                    /* Substitution error */
                    state->phase = EVAL_PHASE_DONE;
                    interp->resultCode = TCL_ERROR;
                    return EVAL_DONE;
                }

                state->substWords[state->wordIndex] = substed;
                state->wordIndex++;
            }

            state->substCount = state->currentCmd.wordCount;
            state->phase = EVAL_PHASE_LOOKUP;
            return EVAL_CONTINUE;
        }

        case EVAL_PHASE_LOOKUP: {
            /* Look up command */
            if (state->substCount == 0) {
                /* Empty command after substitution - skip */
                state->phase = EVAL_PHASE_PARSE;
                return EVAL_CONTINUE;
            }

            size_t cmdNameLen;
            const char *cmdName = host->getStringPtr(state->substWords[0], &cmdNameLen);

            /* Check builtins first */
            int builtinIdx = tclBuiltinLookup(cmdName, cmdNameLen);
            if (builtinIdx >= 0) {
                state->cmdInfo.type = TCL_CMD_BUILTIN;
                state->cmdInfo.u.builtinId = builtinIdx;
            } else {
                /* Try host command lookup */
                if (host->cmdLookup(interp->hostCtx, cmdName, cmdNameLen, &state->cmdInfo) != 0) {
                    state->cmdInfo.type = TCL_CMD_NOT_FOUND;
                }
            }

            state->phase = EVAL_PHASE_DISPATCH;
            return EVAL_CONTINUE;
        }

        case EVAL_PHASE_DISPATCH: {
            /* Dispatch command */
            TclResult result = TCL_OK;

            switch (state->cmdInfo.type) {
                case TCL_CMD_BUILTIN: {
                    const TclBuiltinEntry *entry = tclBuiltinGet(state->cmdInfo.u.builtinId);
                    if (entry) {
                        result = entry->proc(interp, state->substCount, state->substWords);
                    } else {
                        tclSetError(interp, "internal error: invalid builtin", -1);
                        result = TCL_ERROR;
                    }
                    break;
                }

                case TCL_CMD_PROC:
                    /* TODO: Implement proc calls */
                    tclSetError(interp, "proc not implemented", -1);
                    result = TCL_ERROR;
                    break;

                case TCL_CMD_EXTENSION:
                    result = host->extInvoke(interp, state->cmdInfo.u.extHandle,
                                             state->substCount, state->substWords);
                    break;

                case TCL_CMD_NOT_FOUND:
                default: {
                    size_t nameLen;
                    const char *name = host->getStringPtr(state->substWords[0], &nameLen);

                    /* Build error message */
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, nameLen + 30, 1);
                    char *p = msg;
                    const char *prefix = "invalid command name \"";
                    while (*prefix) *p++ = *prefix++;
                    for (size_t i = 0; i < nameLen; i++) *p++ = name[i];
                    *p++ = '"';
                    *p = '\0';

                    tclSetError(interp, msg, p - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    result = TCL_ERROR;
                    break;
                }
            }

            interp->resultCode = result;
            state->phase = EVAL_PHASE_RESULT;
            return EVAL_CONTINUE;
        }

        case EVAL_PHASE_RESULT: {
            /* Handle result code */
            if (interp->resultCode == TCL_ERROR) {
                state->phase = EVAL_PHASE_DONE;
                return EVAL_DONE;
            }

            if (interp->resultCode == TCL_RETURN ||
                interp->resultCode == TCL_BREAK ||
                interp->resultCode == TCL_CONTINUE) {
                state->phase = EVAL_PHASE_DONE;
                return EVAL_DONE;
            }

            /* Continue to next command */
            state->phase = EVAL_PHASE_PARSE;
            return EVAL_CONTINUE;
        }

        case EVAL_PHASE_DONE:
        default:
            return EVAL_DONE;
    }
}

/* ========================================================================
 * High-Level Eval Functions
 * ======================================================================== */

TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len) {
    TclEvalState state;
    tclEvalStateInit(&state, interp, script, len);

    TclEvalStatus status;
    while ((status = tclEvalStep(interp, &state)) == EVAL_CONTINUE) {
        /* Continue */
    }

    tclEvalStateCleanup(&state, interp);
    return interp->resultCode;
}

TclResult tclEvalBracketed(TclInterp *interp, const char *cmd, size_t len) {
    /* Save current result */
    TclObj *savedResult = interp->result;
    TclResult savedCode = interp->resultCode;

    /* Evaluate bracketed command */
    TclResult result = tclEvalScript(interp, cmd, len);

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

TclResult tclEval(TclInterp *interp, TclObj *script) {
    size_t len;
    const char *str = interp->host->getStringPtr(script, &len);
    return tclEvalScript(interp, str, len);
}

TclResult tclEvalStr(TclInterp *interp, const char *script, size_t len) {
    if (len == (size_t)-1) {
        len = tclStrlen(script);
    }
    return tclEvalScript(interp, script, len);
}
