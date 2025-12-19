/*
 * builtin_break.c - TCL break and continue Command Implementations
 */

#include "internal.h"

/* ========================================================================
 * break Command
 * ======================================================================== */

TclResult tclCmdBreak(TclInterp *interp, int objc, TclObj **objv) {
    (void)objv;

    if (objc != 1) {
        tclSetError(interp, "wrong # args: should be \"break\"", -1);
        return TCL_ERROR;
    }

    tclSetResult(interp, interp->host->newString("", 0));
    return TCL_BREAK;
}

/* ========================================================================
 * continue Command
 * ======================================================================== */

TclResult tclCmdContinue(TclInterp *interp, int objc, TclObj **objv) {
    (void)objv;

    if (objc != 1) {
        tclSetError(interp, "wrong # args: should be \"continue\"", -1);
        return TCL_ERROR;
    }

    tclSetResult(interp, interp->host->newString("", 0));
    return TCL_CONTINUE;
}
