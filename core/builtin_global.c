/*
 * builtin_global.c - TCL global Command Implementation
 *
 * The global command makes global variables accessible in the current scope.
 * Syntax: global varName ?varName ...?
 */

#include "internal.h"

TclResult tclCmdGlobal(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* global with no args is valid - just returns empty string */
    if (objc < 2) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* If already at global scope, nothing to do */
    if (interp->currentFrame == interp->globalFrame) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    void *localVars = interp->currentFrame->varsHandle;
    void *globalVars = interp->globalFrame->varsHandle;

    /* Link each variable name to the global scope */
    for (int i = 1; i < objc; i++) {
        size_t nameLen;
        const char *name = host->getStringPtr(objv[i], &nameLen);

        /* Create a link from local to global with the same name */
        host->varLink(localVars, name, nameLen, globalVars, name, nameLen);
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}
