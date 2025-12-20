/*
 * builtin_lassign.c - TCL lassign Command Implementation
 *
 * lassign list ?varName ...? - assigns list elements to variables
 */

#include "internal.h"

/* ========================================================================
 * lassign Command
 * ======================================================================== */

TclResult tclCmdLassign(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lassign list ?varName ...?\"", -1);
        return TCL_ERROR;
    }

    /* Parse the list */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(objv[1], &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    void *vars = interp->currentFrame->varsHandle;

    /* Assign list elements to variables */
    int varCount = objc - 2;  /* Number of variable names provided */

    for (int i = 0; i < varCount; i++) {
        size_t varLen;
        const char *varName = host->getStringPtr(objv[i + 2], &varLen);

        TclObj *value;
        if (i < (int)elemCount) {
            /* Assign the corresponding list element */
            value = host->dup(elems[i]);
        } else {
            /* More variables than elements - assign empty string */
            value = host->newString("", 0);
        }

        host->varSet(vars, varName, varLen, value);
    }

    /* Return remaining elements as a list */
    if (varCount < (int)elemCount) {
        /* More elements than variables - return the rest */
        size_t remainingCount = elemCount - varCount;
        TclObj *result = host->newList(elems + varCount, remainingCount);
        tclSetResult(interp, result);
    } else {
        /* No remaining elements or exactly matched */
        tclSetResult(interp, host->newString("", 0));
    }

    return TCL_OK;
}
