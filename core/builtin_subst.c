/*
 * builtin_subst.c - TCL Subst Command Implementation
 */

#include "internal.h"

/* ========================================================================
 * subst Command
 * ======================================================================== */

TclResult tclCmdSubst(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"subst ?-nobackslashes? ?-nocommands? ?-novariables? string\"", -1);
        return TCL_ERROR;
    }

    int flags = TCL_SUBST_ALL;
    int stringIdx = 1;

    /* Parse flags */
    for (int i = 1; i < objc - 1; i++) {
        size_t len;
        const char *arg = host->getStringPtr(objv[i], &len);

        if (len == 14 && tclStrncmp(arg, "-nobackslashes", 14) == 0) {
            flags &= ~TCL_SUBST_BACKSLASH;
            stringIdx = i + 1;
        } else if (len == 11 && tclStrncmp(arg, "-nocommands", 11) == 0) {
            flags &= ~TCL_SUBST_COMMANDS;
            stringIdx = i + 1;
        } else if (len == 12 && tclStrncmp(arg, "-novariables", 12) == 0) {
            flags &= ~TCL_SUBST_VARIABLES;
            stringIdx = i + 1;
        }
    }

    /* Get the string */
    size_t strLen;
    const char *str = host->getStringPtr(objv[stringIdx], &strLen);

    /* Perform substitution */
    TclObj *result = tclSubstString(interp, str, strLen, flags);
    if (!result) {
        return TCL_ERROR;
    }

    tclSetResult(interp, result);
    return TCL_OK;
}

