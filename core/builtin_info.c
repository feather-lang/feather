/*
 * builtin_info.c - TCL Info Command Implementation
 */

#include "internal.h"





/* ========================================================================
 * info Command
 * ======================================================================== */

/* Helper: look up procedure and get argList/body, returns 0 on success */
static int infoProcLookup(TclInterp *interp, TclObj *nameObj, TclObj **argListOut, TclObj **bodyOut) {
    const TclHost *host = interp->host;
    size_t nameLen;
    const char *name = host->getStringPtr(nameObj, &nameLen);

    TclCmdInfo info;
    host->cmdLookup(interp->hostCtx, name, nameLen, &info);
    if (info.type != TCL_CMD_PROC) {
        return -1;
    }
    if (host->procGetDef(info.u.procHandle, argListOut, bodyOut) != 0) {
        return -1;
    }
    return 0;
}

TclResult tclCmdInfo(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"info subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* info exists varName */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "exists", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"info exists varName\"", -1);
            return TCL_ERROR;
        }
        size_t nameLen;
        const char *name = host->getStringPtr(objv[2], &nameLen);

        /* Handle :: prefix for global variables */
        int forceGlobal = 0;
        if (nameLen >= 2 && name[0] == ':' && name[1] == ':') {
            name += 2;
            nameLen -= 2;
            forceGlobal = 1;
        }

        void *vars = forceGlobal ? interp->globalFrame->varsHandle : interp->currentFrame->varsHandle;

        /* Check for scalar variable */
        int exists = host->varExists(vars, name, nameLen);

        /* Also check for array (without subscript) */
        if (!exists) {
            size_t arrayCount = host->arraySize(vars, name, nameLen);
            exists = (arrayCount > 0);
        }

        /* Check global frame if not found and not already forced global */
        if (!exists && !forceGlobal && interp->currentFrame != interp->globalFrame) {
            exists = host->varExists(interp->globalFrame->varsHandle, name, nameLen);
            if (!exists) {
                size_t arrayCount = host->arraySize(interp->globalFrame->varsHandle, name, nameLen);
                exists = (arrayCount > 0);
            }
        }

        tclSetResult(interp, host->newInt(exists ? 1 : 0));
        return TCL_OK;
    }

    /* info args procName */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "args", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"info args procname\"", -1);
            return TCL_ERROR;
        }

        TclObj *argList, *body;
        if (infoProcLookup(interp, objv[2], &argList, &body) != 0) {
            /* Build error message: "procname" isn't a procedure */
            size_t nameLen;
            const char *name = host->getStringPtr(objv[2], &nameLen);
            void *arena = host->arenaPush(interp->hostCtx);
            char *msg = host->arenaAlloc(arena, nameLen + 30, 1);
            char *p = msg;
            *p++ = '"';
            for (size_t i = 0; i < nameLen; i++) *p++ = name[i];
            *p++ = '"';
            const char *suffix = " isn't a procedure";
            while (*suffix) *p++ = *suffix++;
            *p = '\0';
            tclSetError(interp, msg, p - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Parse argList and extract just the argument names */
        TclObj **args;
        size_t argCount;
        if (host->asList(argList, &args, &argCount) != 0) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Build result list with just argument names */
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **names = host->arenaAlloc(arena, argCount * sizeof(TclObj*), sizeof(void*));

        for (size_t i = 0; i < argCount; i++) {
            /* Each arg might be a list {name default} or just a name */
            TclObj **argParts;
            size_t partCount;
            if (host->asList(args[i], &argParts, &partCount) == 0 && partCount >= 1) {
                names[i] = argParts[0];  /* First element is the name */
            } else {
                names[i] = args[i];  /* Use whole thing as name */
            }
        }

        TclObj *result = host->newList(names, argCount);
        host->arenaPop(interp->hostCtx, arena);

        /* Convert list to space-separated string for output */
        size_t resultLen;
        const char *resultStr = host->getStringPtr(result, &resultLen);
        tclSetResult(interp, host->newString(resultStr, resultLen));
        return TCL_OK;
    }

    /* info body procName */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "body", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"info body procname\"", -1);
            return TCL_ERROR;
        }

        TclObj *argList, *body;
        if (infoProcLookup(interp, objv[2], &argList, &body) != 0) {
            /* Build error message: "procname" isn't a procedure */
            size_t nameLen;
            const char *name = host->getStringPtr(objv[2], &nameLen);
            void *arena = host->arenaPush(interp->hostCtx);
            char *msg = host->arenaAlloc(arena, nameLen + 30, 1);
            char *p = msg;
            *p++ = '"';
            for (size_t i = 0; i < nameLen; i++) *p++ = name[i];
            *p++ = '"';
            const char *suffix = " isn't a procedure";
            while (*suffix) *p++ = *suffix++;
            *p = '\0';
            tclSetError(interp, msg, p - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        tclSetResult(interp, host->dup(body));
        return TCL_OK;
    }

    /* info commands ?pattern? */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "commands", 8) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info commands ?pattern?\"", -1);
            return TCL_ERROR;
        }

        const char *pattern = NULL;
        if (objc == 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }

        TclObj *cmdList = host->cmdList(interp->hostCtx, pattern);
        tclSetResult(interp, cmdList ? cmdList : host->newString("", 0));
        return TCL_OK;
    }

    /* info complete string */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "complete", 8) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"info complete command\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        /* Check if the string is a complete TCL command */
        /* A command is complete if all braces, brackets, and quotes are balanced */
        int braceDepth = 0;
        int bracketDepth = 0;
        int inQuote = 0;
        int complete = 1;

        const char *p = str;
        const char *end = str + strLen;

        while (p < end) {
            if (*p == '\\' && p + 1 < end) {
                p += 2;  /* Skip escaped character */
                continue;
            }

            if (inQuote) {
                if (*p == '"') {
                    inQuote = 0;
                }
            } else if (braceDepth > 0) {
                if (*p == '{') {
                    braceDepth++;
                } else if (*p == '}') {
                    braceDepth--;
                }
            } else {
                if (*p == '"') {
                    inQuote = 1;
                } else if (*p == '{') {
                    braceDepth++;
                } else if (*p == '[') {
                    bracketDepth++;
                } else if (*p == ']') {
                    if (bracketDepth > 0) {
                        bracketDepth--;
                    }
                }
            }
            p++;
        }

        /* Complete if all are balanced */
        complete = (braceDepth == 0 && bracketDepth == 0 && !inQuote);

        tclSetResult(interp, host->newInt(complete ? 1 : 0));
        return TCL_OK;
    }

    /* info default procName arg varName */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "default", 7) == 0) {
        if (objc != 5) {
            tclSetError(interp, "wrong # args: should be \"info default procname arg varname\"", -1);
            return TCL_ERROR;
        }

        TclObj *argList, *body;
        if (infoProcLookup(interp, objv[2], &argList, &body) != 0) {
            /* Build error message: "procname" isn't a procedure */
            size_t nameLen;
            const char *name = host->getStringPtr(objv[2], &nameLen);
            void *arena = host->arenaPush(interp->hostCtx);
            char *msg = host->arenaAlloc(arena, nameLen + 30, 1);
            char *p = msg;
            *p++ = '"';
            for (size_t i = 0; i < nameLen; i++) *p++ = name[i];
            *p++ = '"';
            const char *suffix = " isn't a procedure";
            while (*suffix) *p++ = *suffix++;
            *p = '\0';
            tclSetError(interp, msg, p - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Get the argument name we're looking for */
        size_t targetArgLen;
        const char *targetArg = host->getStringPtr(objv[3], &targetArgLen);

        /* Get the variable name to store the default */
        size_t varNameLen;
        const char *varName = host->getStringPtr(objv[4], &varNameLen);

        /* Parse argList */
        TclObj **args;
        size_t argCount;
        if (host->asList(argList, &args, &argCount) != 0) {
            /* Build error message */
            void *arena = host->arenaPush(interp->hostCtx);
            size_t procNameLen;
            const char *procName = host->getStringPtr(objv[2], &procNameLen);
            char *msg = host->arenaAlloc(arena, procNameLen + targetArgLen + 60, 1);
            char *mp = msg;
            const char *pre = "procedure \"";
            while (*pre) *mp++ = *pre++;
            for (size_t i = 0; i < procNameLen; i++) *mp++ = procName[i];
            const char *mid = "\" doesn't have an argument \"";
            while (*mid) *mp++ = *mid++;
            for (size_t i = 0; i < targetArgLen; i++) *mp++ = targetArg[i];
            *mp++ = '"';
            *mp = '\0';
            tclSetError(interp, msg, mp - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Find the argument */
        int found = 0;
        int hasDefault = 0;
        TclObj *defaultValue = NULL;

        for (size_t i = 0; i < argCount; i++) {
            TclObj **argParts;
            size_t partCount;
            const char *argName;
            size_t argNameLen;

            if (host->asList(args[i], &argParts, &partCount) == 0 && partCount >= 1) {
                argName = host->getStringPtr(argParts[0], &argNameLen);
                if (argNameLen == targetArgLen && tclStrncmp(argName, targetArg, targetArgLen) == 0) {
                    found = 1;
                    if (partCount >= 2) {
                        hasDefault = 1;
                        defaultValue = argParts[1];
                    }
                    break;
                }
            } else {
                argName = host->getStringPtr(args[i], &argNameLen);
                if (argNameLen == targetArgLen && tclStrncmp(argName, targetArg, targetArgLen) == 0) {
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            /* Build error message */
            void *arena = host->arenaPush(interp->hostCtx);
            size_t procNameLen;
            const char *procName = host->getStringPtr(objv[2], &procNameLen);
            char *msg = host->arenaAlloc(arena, procNameLen + targetArgLen + 60, 1);
            char *mp = msg;
            const char *pre = "procedure \"";
            while (*pre) *mp++ = *pre++;
            for (size_t i = 0; i < procNameLen; i++) *mp++ = procName[i];
            const char *mid = "\" doesn't have an argument \"";
            while (*mid) *mp++ = *mid++;
            for (size_t i = 0; i < targetArgLen; i++) *mp++ = targetArg[i];
            *mp++ = '"';
            *mp = '\0';
            tclSetError(interp, msg, mp - msg);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Set the variable with the default value if there is one */
        if (hasDefault) {
            void *vars = interp->currentFrame->varsHandle;
            host->varSet(vars, varName, varNameLen, host->dup(defaultValue));
            tclSetResult(interp, host->newInt(1));
        } else {
            tclSetResult(interp, host->newInt(0));
        }
        return TCL_OK;
    }

    /* info globals ?pattern? */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "globals", 7) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info globals ?pattern?\"", -1);
            return TCL_ERROR;
        }

        const char *pattern = NULL;
        if (objc == 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }

        TclObj *names = host->varNames(interp->globalFrame->varsHandle, pattern);
        tclSetResult(interp, names ? names : host->newString("", 0));
        return TCL_OK;
    }

    /* info level ?number? */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "level", 5) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info level ?number?\"", -1);
            return TCL_ERROR;
        }

        if (objc == 2) {
            /* Return current level */
            tclSetResult(interp, host->newInt((int64_t)interp->currentFrame->level));
            return TCL_OK;
        }

        /* Get information about a specific level */
        int64_t levelNum;
        if (host->asInt(objv[2], &levelNum) != 0) {
            tclSetError(interp, "expected integer but got non-integer value", -1);
            return TCL_ERROR;
        }

        /* Handle zero and negative levels as relative (0 = current, -1 = caller) */
        /* Positive levels are absolute (1 = most recent proc, 2 = its caller) */
        int targetLevel;
        if (levelNum <= 0) {
            targetLevel = (int)interp->currentFrame->level + (int)levelNum;
        } else {
            targetLevel = (int)levelNum;
        }

        if (targetLevel < 0 || targetLevel > (int)interp->currentFrame->level) {
            tclSetError(interp, "bad level", -1);
            return TCL_ERROR;
        }

        /* Find the frame at the target level */
        TclFrame *frame = interp->currentFrame;
        while (frame && (int)frame->level > targetLevel) {
            frame = frame->parent;
        }

        if (!frame || (int)frame->level != targetLevel) {
            tclSetError(interp, "bad level", -1);
            return TCL_ERROR;
        }

        /* Return the full command invocation as a list at that level */
        if (frame->invocationObjs && frame->invocationCount > 0) {
            TclObj *invocList = host->newList(frame->invocationObjs, frame->invocationCount);
            tclSetResult(interp, invocList);
        } else if (frame->procName) {
            tclSetResult(interp, host->newString(frame->procName, tclStrlen(frame->procName)));
        } else {
            tclSetResult(interp, host->newString("", 0));
        }
        return TCL_OK;
    }

    /* info locals ?pattern? */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "locals", 6) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info locals ?pattern?\"", -1);
            return TCL_ERROR;
        }

        /* At global level, return empty */
        if (interp->currentFrame == interp->globalFrame) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        const char *pattern = NULL;
        if (objc == 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }

        /* Use varNamesLocal to exclude linked variables (from global/upvar) */
        TclObj *names = host->varNamesLocal(interp->currentFrame->varsHandle, pattern);
        tclSetResult(interp, names ? names : host->newString("", 0));
        return TCL_OK;
    }

    /* info procs ?pattern? */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "procs", 5) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info procs ?pattern?\"", -1);
            return TCL_ERROR;
        }

        const char *pattern = NULL;
        if (objc == 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }

        /* Get all commands, then filter for procs */
        TclObj *allCmds = host->cmdList(interp->hostCtx, pattern);
        if (!allCmds) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        TclObj **cmds;
        size_t cmdCount;
        if (host->asList(allCmds, &cmds, &cmdCount) != 0 || cmdCount == 0) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Filter for procs only */
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **procs = host->arenaAlloc(arena, cmdCount * sizeof(TclObj*), sizeof(void*));
        size_t procCount = 0;

        for (size_t i = 0; i < cmdCount; i++) {
            size_t nameLen;
            const char *name = host->getStringPtr(cmds[i], &nameLen);
            TclCmdInfo info;
            host->cmdLookup(interp->hostCtx, name, nameLen, &info);
            if (info.type == TCL_CMD_PROC) {
                procs[procCount++] = cmds[i];
            }
        }

        TclObj *result = host->newList(procs, procCount);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* info vars ?pattern? */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "vars", 4) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info vars ?pattern?\"", -1);
            return TCL_ERROR;
        }

        const char *pattern = NULL;
        if (objc == 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }

        TclObj *names = host->varNames(interp->currentFrame->varsHandle, pattern);
        tclSetResult(interp, names ? names : host->newString("", 0));
        return TCL_OK;
    }

    /* info patchlevel */
    if (subcmdLen == 10 && tclStrncmp(subcmd, "patchlevel", 10) == 0) {
        if (objc != 2) {
            tclSetError(interp, "wrong # args: should be \"info patchlevel\"", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newString("9.0.2", 5));
        return TCL_OK;
    }

    /* info tclversion */
    if (subcmdLen == 10 && tclStrncmp(subcmd, "tclversion", 10) == 0) {
        if (objc != 2) {
            tclSetError(interp, "wrong # args: should be \"info tclversion\"", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newString("9.0", 3));
        return TCL_OK;
    }

    /* info script ?filename? */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "script", 6) == 0) {
        if (objc > 3) {
            tclSetError(interp, "wrong # args: should be \"info script ?filename?\"", -1);
            return TCL_ERROR;
        }

        /* Get the current script path */
        const char *currentScript = interp->scriptFile ? interp->scriptFile : "";

        if (objc == 3) {
            /* Set new script path, return old one */
            size_t newPathLen;
            const char *newPath = host->getStringPtr(objv[2], &newPathLen);

            /* Return the old script path */
            tclSetResult(interp, host->newString(currentScript, tclStrlen(currentScript)));

            /* Set the new script path - need to copy the string since objv might be freed */
            /* The host should provide a way to store this, but for now we use the arena */
            if (newPathLen > 0) {
                void *arena = host->arenaPush(interp->hostCtx);
                char *newScriptPath = host->arenaAlloc(arena, newPathLen + 1, 1);
                for (size_t i = 0; i < newPathLen; i++) {
                    newScriptPath[i] = newPath[i];
                }
                newScriptPath[newPathLen] = '\0';
                interp->scriptFile = newScriptPath;
                /* Note: arena is intentionally not popped - the string needs to persist */
            } else {
                interp->scriptFile = NULL;
            }
        } else {
            /* Just return current script path */
            tclSetResult(interp, host->newString(currentScript, tclStrlen(currentScript)));
        }
        return TCL_OK;
    }

    /* Use static error message - subcmd name is embedded as literal */
    /* This matches what string/dict commands do */
    (void)subcmd;  /* Unused for now - using literal "unknown" */
    (void)subcmdLen;
    tclSetError(interp, "unknown or ambiguous subcommand \"unknown\": must be args, body, class, cmdcount, cmdtype, commands, complete, constant, consts, coroutine, default, errorstack, exists, frame, functions, globals, hostname, level, library, loaded, locals, nameofexecutable, object, patchlevel, procs, script, sharedlibextension, tclversion, or vars", -1);
    return TCL_ERROR;
}
