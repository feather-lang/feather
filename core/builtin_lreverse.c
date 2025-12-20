/*
 * builtin_lreverse.c - TCL lreverse Command Implementation
 *
 * lreverse command: reverses the order of elements in a list
 */

#include "internal.h"

/* ========================================================================
 * lreverse Command
 * ======================================================================== */

TclResult tclCmdLreverse(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 2) {
        tclSetError(interp, "wrong # args: should be \"lreverse list\"", -1);
        return TCL_ERROR;
    }

    /* Parse the list */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(objv[1], &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    /* Empty list returns empty string */
    if (elemCount == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Create reversed array using arena */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **reversed = host->arenaAlloc(arena, elemCount * sizeof(TclObj*), sizeof(void*));

    /* Reverse the elements */
    for (size_t i = 0; i < elemCount; i++) {
        reversed[i] = elems[elemCount - 1 - i];
    }

    /* Create result list */
    TclObj *result = host->newList(reversed, elemCount);
    host->arenaPop(interp->hostCtx, arena);

    tclSetResult(interp, result);
    return TCL_OK;
}
