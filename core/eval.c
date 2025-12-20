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
 * Legacy Eval State Management (kept for API compatibility)
 *
 * These functions are no longer used internally since evaluation now
 * goes through the tree-walking evaluator. They are kept as stubs in
 * case external code references them.
 * ======================================================================== */

void tclEvalStateInit(TclEvalState *state, TclInterp *interp,
                      const char *script, size_t len) {
    (void)state;
    (void)interp;
    (void)script;
    (void)len;
}

void tclEvalStateCleanup(TclEvalState *state, TclInterp *interp) {
    (void)state;
    (void)interp;
}

TclEvalStatus tclEvalStep(TclInterp *interp, TclEvalState *state) {
    (void)interp;
    (void)state;
    return EVAL_DONE;
}

/* ========================================================================
 * High-Level Eval Functions
 *
 * Uses the tree-walking evaluator (tree_eval.c) instead of the old
 * text-based trampoline. The tree evaluator parses the script into an AST
 * and evaluates by walking the tree, enabling proper coroutine support.
 * ======================================================================== */

TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len) {
    return tclTreeEvalStr(interp, script, len);
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
