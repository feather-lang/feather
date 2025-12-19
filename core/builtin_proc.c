/*
 * builtin_proc.c - TCL Procedure Command Implementations
 *
 * Procedure commands: proc, return
 */

#include "internal.h"

/* ========================================================================
 * proc Command
 * ======================================================================== */

TclResult tclCmdProc(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 4) {
        tclSetError(interp, "wrong # args: should be \"proc name args body\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *name = host->getStringPtr(objv[1], &nameLen);

    /* Register the procedure with the host */
    void *handle = host->procRegister(interp->hostCtx, name, nameLen, objv[2], objv[3]);
    if (!handle) {
        tclSetError(interp, "failed to register procedure", -1);
        return TCL_ERROR;
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * return Command
 * ======================================================================== */

TclResult tclCmdReturn(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Default return value is empty string */
    TclObj *result = host->newString("", 0);
    int code = TCL_OK;
    int level = 1;

    /* Parse options: return ?-code code? ?-level level? ?result? */
    int i = 1;
    while (i < objc) {
        size_t len;
        const char *arg = host->getStringPtr(objv[i], &len);

        if (len >= 1 && arg[0] == '-') {
            if (len == 5 && tclStrncmp(arg, "-code", 5) == 0) {
                if (i + 1 >= objc) {
                    tclSetError(interp, "wrong # args: should be \"-code code\"", -1);
                    return TCL_ERROR;
                }
                i++;
                size_t codeLen;
                const char *codeStr = host->getStringPtr(objv[i], &codeLen);

                /* Parse code value */
                if (codeLen == 2 && tclStrncmp(codeStr, "ok", 2) == 0) {
                    code = TCL_OK;
                } else if (codeLen == 5 && tclStrncmp(codeStr, "error", 5) == 0) {
                    code = TCL_ERROR;
                } else if (codeLen == 6 && tclStrncmp(codeStr, "return", 6) == 0) {
                    code = TCL_RETURN;
                } else if (codeLen == 5 && tclStrncmp(codeStr, "break", 5) == 0) {
                    code = TCL_BREAK;
                } else if (codeLen == 8 && tclStrncmp(codeStr, "continue", 8) == 0) {
                    code = TCL_CONTINUE;
                } else {
                    /* Try as integer */
                    int64_t val;
                    if (host->asInt(objv[i], &val) == 0) {
                        code = (int)val;
                    } else {
                        tclSetError(interp, "bad completion code", -1);
                        return TCL_ERROR;
                    }
                }
                i++;
            } else if (len == 6 && tclStrncmp(arg, "-level", 6) == 0) {
                if (i + 1 >= objc) {
                    tclSetError(interp, "wrong # args: should be \"-level level\"", -1);
                    return TCL_ERROR;
                }
                i++;
                int64_t val;
                if (host->asInt(objv[i], &val) != 0 || val < 0) {
                    tclSetError(interp, "bad level", -1);
                    return TCL_ERROR;
                }
                level = (int)val;
                i++;
            } else {
                /* Unknown option - treat as result value */
                result = objv[i];
                i++;
            }
        } else {
            /* Result value */
            result = objv[i];
            i++;
        }
    }

    /* Set interpreter state for return */
    interp->result = result;
    interp->returnCode = code;
    interp->returnLevel = level;

    return TCL_RETURN;
}
