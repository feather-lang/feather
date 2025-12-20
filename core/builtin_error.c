/*
 * builtin_error.c - TCL Error Handling Command Implementations
 *
 * Error commands: error, catch, throw, try
 */

#include "internal.h"

/* ========================================================================
 * error Command
 * ======================================================================== */

TclResult tclCmdError(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 4) {
        tclSetError(interp, "wrong # args: should be \"error message ?info? ?code?\"", -1);
        return TCL_ERROR;
    }

    /* Get the error message */
    size_t msgLen;
    const char *msg = host->getStringPtr(objv[1], &msgLen);

    /* Set the error message */
    tclSetError(interp, msg, (int)msgLen);

    /* Handle optional info argument (errorInfo) */
    if (objc >= 3) {
        size_t infoLen;
        const char *info = host->getStringPtr(objv[2], &infoLen);
        if (infoLen > 0) {
            /* Set errorInfo directly instead of letting it accumulate */
            interp->errorInfo = host->newString(info, infoLen);
        }
    }

    /* Handle optional code argument (errorCode) */
    if (objc >= 4) {
        tclSetErrorCode(interp, objv[3]);
    }

    return TCL_ERROR;
}

/* ========================================================================
 * catch Command
 * ======================================================================== */

TclResult tclCmdCatch(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 4) {
        tclSetError(interp, "wrong # args: should be \"catch script ?resultVarName? ?optionsVarName?\"", -1);
        return TCL_ERROR;
    }

    /* Execute the script and capture the result code */
    TclResult code = tclEvalObj(interp, objv[1], 0);

    /* Set global errorInfo and errorCode variables after catching an error */
    if (code == TCL_ERROR) {
        void *globalVars = interp->globalFrame->varsHandle;

        /* Set ::errorInfo */
        if (interp->errorInfo) {
            host->varSet(globalVars, "errorInfo", 9, host->dup(interp->errorInfo));
        } else if (interp->result) {
            host->varSet(globalVars, "errorInfo", 9, host->dup(interp->result));
        }

        /* Set ::errorCode */
        if (interp->errorCode) {
            host->varSet(globalVars, "errorCode", 9, host->dup(interp->errorCode));
        } else {
            host->varSet(globalVars, "errorCode", 9, host->newString("NONE", 4));
        }
    }

    /* Store result in variable if requested */
    if (objc >= 3) {
        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Store the result (or error message) */
        TclObj *resultValue = interp->result ? host->dup(interp->result) : host->newString("", 0);
        host->varSet(vars, varName, varLen, resultValue);
    }

    /* Store options dict in variable if requested */
    if (objc >= 4) {
        size_t optVarLen;
        const char *optVarName = host->getStringPtr(objv[3], &optVarLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Build options dict with -code, -level, and -errorcode (if error) */
        void *arena = host->arenaPush(interp->hostCtx);

        /* Count elements: -code value -level value [-errorcode value] */
        int elemCount = 4;  /* -code, codeVal, -level, levelVal */
        if (code == TCL_ERROR && interp->errorCode) {
            elemCount += 2;  /* -errorcode, errorCodeVal */
        }

        TclObj **elems = host->arenaAlloc(arena, elemCount * sizeof(TclObj*), sizeof(void*));
        int idx = 0;

        /* -code */
        elems[idx++] = host->newString("-code", 5);
        elems[idx++] = host->newInt((int64_t)code);

        /* -level */
        elems[idx++] = host->newString("-level", 6);
        elems[idx++] = host->newInt(0);

        /* -errorcode (only on error) */
        if (code == TCL_ERROR && interp->errorCode) {
            elems[idx++] = host->newString("-errorcode", 10);
            elems[idx++] = host->dup(interp->errorCode);
        }

        TclObj *optValue = host->newList(elems, elemCount);
        host->arenaPop(interp->hostCtx, arena);
        host->varSet(vars, optVarName, optVarLen, optValue);
    }

    /* Return the code as an integer - catch always succeeds */
    tclSetResult(interp, host->newInt((int64_t)code));
    return TCL_OK;
}

/* ========================================================================
 * throw Command
 * ======================================================================== */

TclResult tclCmdThrow(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 3) {
        tclSetError(interp, "wrong # args: should be \"throw type message\"", -1);
        return TCL_ERROR;
    }

    /* Set error code from type */
    tclSetErrorCode(interp, objv[1]);

    /* Set error message */
    size_t msgLen;
    const char *msg = host->getStringPtr(objv[2], &msgLen);
    tclSetError(interp, msg, (int)msgLen);

    return TCL_ERROR;
}

/* ========================================================================
 * try Command
 * ======================================================================== */

TclResult tclCmdTry(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"try body ?handler...? ?finally script?\"", -1);
        return TCL_ERROR;
    }

    /* Execute the body */
    TclResult bodyCode = tclEvalObj(interp, objv[1], 0);
    TclObj *bodyResult = interp->result ? host->dup(interp->result) : host->newString("", 0);

    /* Look for handlers and finally clause */
    int finallyIdx = -1;
    int handlerMatched = 0;
    TclResult handlerCode = bodyCode;
    TclObj *handlerResult = bodyResult;

    int i = 2;
    while (i < objc) {
        size_t kwLen;
        const char *kw = host->getStringPtr(objv[i], &kwLen);

        if (kwLen == 7 && tclStrncmp(kw, "finally", 7) == 0) {
            /* finally clause */
            if (i + 1 >= objc) {
                tclSetError(interp, "wrong # args: finally requires a script", -1);
                return TCL_ERROR;
            }
            finallyIdx = i + 1;
            break;
        }

        if (kwLen == 2 && tclStrncmp(kw, "on", 2) == 0) {
            /* on code varList script */
            if (i + 3 >= objc) {
                tclSetError(interp, "wrong # args: on requires code varList script", -1);
                return TCL_ERROR;
            }

            if (!handlerMatched) {
                /* Parse the code */
                size_t codeLen;
                const char *codeStr = host->getStringPtr(objv[i + 1], &codeLen);
                int targetCode = -1;

                if (codeLen == 2 && tclStrncmp(codeStr, "ok", 2) == 0) targetCode = TCL_OK;
                else if (codeLen == 5 && tclStrncmp(codeStr, "error", 5) == 0) targetCode = TCL_ERROR;
                else if (codeLen == 6 && tclStrncmp(codeStr, "return", 6) == 0) targetCode = TCL_RETURN;
                else if (codeLen == 5 && tclStrncmp(codeStr, "break", 5) == 0) targetCode = TCL_BREAK;
                else if (codeLen == 8 && tclStrncmp(codeStr, "continue", 8) == 0) targetCode = TCL_CONTINUE;
                else {
                    /* Try as integer */
                    int64_t val;
                    if (host->asInt(objv[i + 1], &val) == 0) {
                        targetCode = (int)val;
                    }
                }

                if (targetCode == (int)bodyCode) {
                    handlerMatched = 1;

                    /* Bind variables from varList */
                    TclObj **varNames;
                    size_t varCount;
                    if (host->asList(objv[i + 2], &varNames, &varCount) == 0) {
                        void *vars = interp->currentFrame->varsHandle;
                        if (varCount >= 1) {
                            size_t vlen;
                            const char *vname = host->getStringPtr(varNames[0], &vlen);
                            if (vlen > 0) {
                                host->varSet(vars, vname, vlen, host->dup(bodyResult));
                            }
                        }
                        /* varCount >= 2 would be options dict - skip for now */
                    }

                    /* Execute handler script */
                    handlerCode = tclEvalObj(interp, objv[i + 3], 0);
                    handlerResult = interp->result ? host->dup(interp->result) : host->newString("", 0);
                }
            }

            i += 4;
            continue;
        }

        if (kwLen == 4 && tclStrncmp(kw, "trap", 4) == 0) {
            /* trap pattern varList script */
            if (i + 3 >= objc) {
                tclSetError(interp, "wrong # args: trap requires pattern varList script", -1);
                return TCL_ERROR;
            }

            if (!handlerMatched && bodyCode == TCL_ERROR) {
                /* For now, just match any error with trap {} */
                size_t patLen;
                host->getStringPtr(objv[i + 1], &patLen);

                /* Empty pattern matches any error */
                int matches = (patLen == 0);
                /* TODO: Match against errorCode prefix */

                if (matches) {
                    handlerMatched = 1;

                    /* Bind variables from varList */
                    TclObj **varNames;
                    size_t varCount;
                    if (host->asList(objv[i + 2], &varNames, &varCount) == 0) {
                        void *vars = interp->currentFrame->varsHandle;
                        if (varCount >= 1) {
                            size_t vlen;
                            const char *vname = host->getStringPtr(varNames[0], &vlen);
                            if (vlen > 0) {
                                host->varSet(vars, vname, vlen, host->dup(bodyResult));
                            }
                        }
                    }

                    /* Execute handler script */
                    handlerCode = tclEvalObj(interp, objv[i + 3], 0);
                    handlerResult = interp->result ? host->dup(interp->result) : host->newString("", 0);
                }
            }

            i += 4;
            continue;
        }

        /* Unknown keyword - could be finally without keyword in older syntax */
        break;
    }

    /* Execute finally clause if present */
    if (finallyIdx >= 0) {
        TclResult finallyCode = tclEvalObj(interp, objv[finallyIdx], 0);

        /* If finally raises an error, that takes precedence */
        if (finallyCode == TCL_ERROR) {
            return TCL_ERROR;
        }
    }

    /* Set result and return appropriate code */
    tclSetResult(interp, handlerResult);
    return handlerCode;
}
