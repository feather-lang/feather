/*
 * builtin_proc.c - TCL Procedure Command Implementations
 *
 * Procedure commands: proc, return, apply
 */

#include "internal.h"

/* Forward declaration for script evaluation */
TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len);

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
 * rename Command
 * ======================================================================== */

TclResult tclCmdRename(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 3) {
        tclSetError(interp, "wrong # args: should be \"rename oldName newName\"", -1);
        return TCL_ERROR;
    }

    size_t oldLen, newLen;
    const char *oldName = host->getStringPtr(objv[1], &oldLen);
    const char *newName = host->getStringPtr(objv[2], &newLen);

    /* If newName is empty, delete the command */
    if (newLen == 0) {
        int result = host->cmdDelete(interp->hostCtx, oldName, oldLen);
        if (result != 0) {
            /* Build error message: can't delete "name": command doesn't exist */
            const char *prefix = "can't delete \"";
            const char *suffix = "\": command doesn't exist";
            size_t msgLen = 14 + oldLen + 24;
            char *msg = host->arenaAlloc(host->arenaPush(interp->hostCtx), msgLen + 1, 1);
            char *p = msg;
            for (size_t i = 0; i < 14; i++) *p++ = prefix[i];
            for (size_t i = 0; i < oldLen; i++) *p++ = oldName[i];
            for (size_t i = 0; i < 24; i++) *p++ = suffix[i];
            *p = '\0';
            tclSetError(interp, msg, msgLen);
            return TCL_ERROR;
        }
    } else {
        int result = host->cmdRename(interp->hostCtx, oldName, oldLen, newName, newLen);
        if (result < 0) {
            /* Command doesn't exist */
            const char *prefix = "can't rename \"";
            const char *suffix = "\": command doesn't exist";
            size_t msgLen = 14 + oldLen + 24;
            char *msg = host->arenaAlloc(host->arenaPush(interp->hostCtx), msgLen + 1, 1);
            char *p = msg;
            for (size_t i = 0; i < 14; i++) *p++ = prefix[i];
            for (size_t i = 0; i < oldLen; i++) *p++ = oldName[i];
            for (size_t i = 0; i < 24; i++) *p++ = suffix[i];
            *p = '\0';
            tclSetError(interp, msg, msgLen);
            return TCL_ERROR;
        } else if (result > 0) {
            /* Target command already exists */
            const char *prefix = "can't rename to \"";
            const char *suffix = "\": command already exists";
            size_t msgLen = 17 + newLen + 25;
            char *msg = host->arenaAlloc(host->arenaPush(interp->hostCtx), msgLen + 1, 1);
            char *p = msg;
            for (size_t i = 0; i < 17; i++) *p++ = prefix[i];
            for (size_t i = 0; i < newLen; i++) *p++ = newName[i];
            for (size_t i = 0; i < 25; i++) *p++ = suffix[i];
            *p = '\0';
            tclSetError(interp, msg, msgLen);
            return TCL_ERROR;
        }
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

/* ========================================================================
 * apply Command
 * ======================================================================== */

TclResult tclCmdApply(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"apply lambdaExpr ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    /* Parse the lambda expression - must be 2 or 3 element list */
    TclObj **lambdaParts = NULL;
    size_t lambdaCount = 0;
    if (host->asList(objv[1], &lambdaParts, &lambdaCount) != 0) {
        tclSetError(interp, "can't interpret lambda as a lambda expression", -1);
        return TCL_ERROR;
    }

    if (lambdaCount < 2 || lambdaCount > 3) {
        /* Build error message: can't interpret "..." as a lambda expression */
        size_t funcLen;
        const char *funcStr = host->getStringPtr(objv[1], &funcLen);
        const char *prefix = "can't interpret \"";
        const char *suffix = "\" as a lambda expression";
        size_t msgLen = 17 + funcLen + 24;
        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, msgLen + 1, 1);
        char *p = msg;
        for (size_t i = 0; i < 17; i++) *p++ = prefix[i];
        for (size_t i = 0; i < funcLen; i++) *p++ = funcStr[i];
        for (size_t i = 0; i < 24; i++) *p++ = suffix[i];
        *p = '\0';
        tclSetError(interp, msg, msgLen);
        return TCL_ERROR;
    }

    TclObj *argList = lambdaParts[0];
    TclObj *body = lambdaParts[1];
    /* Note: lambdaParts[2] would be namespace, currently ignored */

    /* Parse argument specification */
    TclObj **argSpecs = NULL;
    size_t argCount = 0;
    if (host->asList(argList, &argSpecs, &argCount) != 0) {
        tclSetError(interp, "invalid argument list", -1);
        return TCL_ERROR;
    }

    /* Create new frame for lambda execution */
    TclFrame *lambdaFrame = host->frameAlloc(interp->hostCtx);
    if (!lambdaFrame) {
        tclSetError(interp, "out of memory", -1);
        return TCL_ERROR;
    }
    lambdaFrame->parent = interp->currentFrame;
    lambdaFrame->level = interp->currentFrame->level + 1;
    lambdaFrame->flags = TCL_FRAME_PROC;
    lambdaFrame->procName = "apply";

    /* Store invocation for info level */
    lambdaFrame->invocationObjs = objv;
    lambdaFrame->invocationCount = objc;

    /* Calculate actual arguments (excluding 'apply' and lambda) */
    int actualArgs = objc - 2;
    int hasArgs = 0;

    /* Check if last param is 'args' for varargs */
    if (argCount > 0) {
        size_t lastArgLen;
        const char *lastArg = host->getStringPtr(argSpecs[argCount - 1], &lastArgLen);
        if (lastArgLen == 4 && tclStrncmp(lastArg, "args", 4) == 0) {
            hasArgs = 1;
        }
    }

    /* Check argument count */
    int requiredArgs = (int)argCount - (hasArgs ? 1 : 0);

    /* Handle default arguments - count how many have defaults */
    int minArgs = 0;
    for (size_t i = 0; i < (size_t)requiredArgs; i++) {
        size_t listLen = host->listLength(argSpecs[i]);
        if (listLen < 2) {
            minArgs++;  /* Required argument */
        }
    }

    if (actualArgs < minArgs || (!hasArgs && actualArgs > requiredArgs)) {
        /* Build error message: wrong # args: should be "apply lambdaExpr arg1 arg2..." */
        void *arena = host->arenaPush(interp->hostCtx);

        /* Calculate total message length */
        const char *prefix = "wrong # args: should be \"apply lambdaExpr";
        size_t msgLen = 41;  /* strlen(prefix) */

        /* Add space and each argument name */
        for (size_t i = 0; i < argCount; i++) {
            TclObj *argSpec = argSpecs[i];
            size_t listLen = host->listLength(argSpec);
            size_t argNameLen;

            if (listLen >= 2) {
                TclObj *nameObj = host->listIndex(argSpec, 0);
                host->getStringPtr(nameObj, &argNameLen);
            } else {
                host->getStringPtr(argSpec, &argNameLen);
            }
            msgLen += 1 + argNameLen;  /* space + name */
        }
        msgLen += 1;  /* closing quote */

        char *msg = host->arenaAlloc(arena, msgLen + 1, 1);
        char *p = msg;

        /* Copy prefix */
        for (size_t i = 0; i < 41; i++) *p++ = prefix[i];

        /* Copy each argument name */
        for (size_t i = 0; i < argCount; i++) {
            TclObj *argSpec = argSpecs[i];
            size_t listLen = host->listLength(argSpec);
            const char *argName;
            size_t argNameLen;

            if (listLen >= 2) {
                TclObj *nameObj = host->listIndex(argSpec, 0);
                argName = host->getStringPtr(nameObj, &argNameLen);
            } else {
                argName = host->getStringPtr(argSpec, &argNameLen);
            }

            *p++ = ' ';
            for (size_t j = 0; j < argNameLen; j++) *p++ = argName[j];
        }

        *p++ = '"';
        *p = '\0';

        tclSetError(interp, msg, msgLen);
        host->frameFree(interp->hostCtx, lambdaFrame);
        return TCL_ERROR;
    }

    /* Bind each argument */
    for (size_t i = 0; i < (size_t)requiredArgs; i++) {
        TclObj *argSpec = argSpecs[i];
        size_t argNameLen;
        const char *argName;
        TclObj *value = NULL;

        /* Check if arg has a default */
        size_t listLen = host->listLength(argSpec);
        if (listLen >= 2) {
            /* Has default: {name default} */
            TclObj *nameObj = host->listIndex(argSpec, 0);
            argName = host->getStringPtr(nameObj, &argNameLen);

            if ((int)i < actualArgs) {
                value = objv[i + 2];  /* +2 to skip 'apply' and lambda */
            } else {
                value = host->listIndex(argSpec, 1);
            }
        } else {
            /* Simple argument name */
            argName = host->getStringPtr(argSpec, &argNameLen);
            if ((int)i < actualArgs) {
                value = objv[i + 2];  /* +2 to skip 'apply' and lambda */
            }
        }

        if (value) {
            host->varSet(lambdaFrame->varsHandle, argName, argNameLen, host->dup(value));
        }
    }

    /* Bind 'args' if present */
    if (hasArgs) {
        int argsStart = requiredArgs;
        int argsCount = actualArgs - argsStart;
        if (argsCount < 0) argsCount = 0;

        TclObj *argsList;
        if (argsCount > 0) {
            argsList = host->newList(&objv[argsStart + 2], argsCount);
        } else {
            argsList = host->newString("", 0);
        }
        host->varSet(lambdaFrame->varsHandle, "args", 4, argsList);
    }

    /* Switch to lambda frame and execute body */
    TclFrame *savedFrame = interp->currentFrame;
    interp->currentFrame = lambdaFrame;

    size_t bodyLen;
    const char *bodyStr = host->getStringPtr(body, &bodyLen);
    TclResult result = tclEvalScript(interp, bodyStr, bodyLen);

    /* Handle return */
    if (result == TCL_RETURN) {
        result = TCL_OK;
    }

    /* Restore frame and cleanup */
    interp->currentFrame = savedFrame;
    host->frameFree(interp->hostCtx, lambdaFrame);

    return result;
}
