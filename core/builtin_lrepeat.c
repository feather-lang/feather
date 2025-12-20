/*
 * builtin_lrepeat.c - TCL lrepeat Command Implementation
 *
 * lrepeat command: creates a list by repeating elements count times
 */

#include "internal.h"

/* ========================================================================
 * lrepeat Command
 * ======================================================================== */

TclResult tclCmdLrepeat(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Check arguments */
    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lrepeat count ?value ...?\"", -1);
        return TCL_ERROR;
    }

    /* Parse count */
    int64_t count;
    if (host->asInt(objv[1], &count) != 0) {
        /* If not an integer, create error message */
        size_t countLen;
        const char *countStr = host->getStringPtr(objv[1], &countLen);

        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, 100 + countLen, 1);
        char *p = msg;

        /* Build error: bad count "VALUE": must be integer >= 0 */
        const char *prefix = "bad count \"";
        for (const char *s = prefix; *s; s++) *p++ = *s;
        for (size_t i = 0; i < countLen; i++) *p++ = countStr[i];
        const char *suffix = "\": must be integer >= 0";
        for (const char *s = suffix; *s; s++) *p++ = *s;
        *p = '\0';

        tclSetError(interp, msg, p - msg);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    /* Check count is non-negative */
    if (count < 0) {
        size_t countLen;
        const char *countStr = host->getStringPtr(objv[1], &countLen);

        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, 100 + countLen, 1);
        char *p = msg;

        /* Build error: bad count "VALUE": must be integer >= 0 */
        const char *prefix = "bad count \"";
        for (const char *s = prefix; *s; s++) *p++ = *s;
        for (size_t i = 0; i < countLen; i++) *p++ = countStr[i];
        const char *suffix = "\": must be integer >= 0";
        for (const char *s = suffix; *s; s++) *p++ = *s;
        *p = '\0';

        tclSetError(interp, msg, p - msg);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_ERROR;
    }

    /* If count is 0 or no elements, return empty list */
    if (count == 0 || objc == 2) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Calculate total number of elements in result list */
    int elemCount = objc - 2;  /* Number of elements to repeat */
    size_t totalElems = (size_t)count * elemCount;

    /* Use arena for temporary array */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **resultElems = host->arenaAlloc(arena, totalElems * sizeof(TclObj*), sizeof(void*));

    /* Fill the result array by repeating the input elements */
    size_t idx = 0;
    for (int64_t i = 0; i < count; i++) {
        for (int j = 2; j < objc; j++) {
            resultElems[idx++] = objv[j];
        }
    }

    /* Create the result list */
    TclObj *result = host->newList(resultElems, totalElems);
    host->arenaPop(interp->hostCtx, arena);

    tclSetResult(interp, result);
    return TCL_OK;
}
