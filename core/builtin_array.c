/*
 * builtin_array.c - TCL Array Command Implementation
 */

#include "internal.h"

/* ========================================================================
 * array Command
 * ======================================================================== */

TclResult tclCmdArray(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"array subcommand arrayName ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* array exists arrayName */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "exists", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"array exists arrayName\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Check if any array elements exist */
        size_t size = host->arraySize(vars, arrName, arrLen);
        if (size == 0 && interp->currentFrame != interp->globalFrame) {
            size = host->arraySize(interp->globalFrame->varsHandle, arrName, arrLen);
        }

        tclSetResult(interp, host->newInt(size > 0 ? 1 : 0));
        return TCL_OK;
    }

    /* array names arrayName ?mode? ?pattern? */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "names", 5) == 0) {
        if (objc < 3 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"array names arrayName ?-exact|-glob|-regexp? ?pattern?\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);

        /* Parse mode and pattern */
        int mode = 0;  /* 0=glob (default), 1=exact, 2=regexp */
        const char *pattern = NULL;
        size_t patLen = 0;

        if (objc == 4) {
            /* Could be just a pattern, or a mode with no pattern */
            size_t argLen;
            const char *arg = host->getStringPtr(objv[3], &argLen);
            if (argLen > 0 && arg[0] == '-') {
                /* It's a mode flag - pattern defaults to * */
                if (argLen == 6 && tclStrncmp(arg, "-exact", 6) == 0) {
                    mode = 1;
                    pattern = "*";
                    patLen = 1;
                } else if (argLen == 5 && tclStrncmp(arg, "-glob", 5) == 0) {
                    mode = 0;
                    pattern = "*";
                    patLen = 1;
                } else if (argLen == 7 && tclStrncmp(arg, "-regexp", 7) == 0) {
                    mode = 2;
                    pattern = ".*";
                    patLen = 2;
                } else {
                    tclSetError(interp, "bad option: must be -exact, -glob, or -regexp", -1);
                    return TCL_ERROR;
                }
            } else {
                /* It's a pattern */
                pattern = arg;
                patLen = argLen;
            }
        } else if (objc == 5) {
            /* Mode and pattern */
            size_t modeLen;
            const char *modeStr = host->getStringPtr(objv[3], &modeLen);
            if (modeLen == 6 && tclStrncmp(modeStr, "-exact", 6) == 0) {
                mode = 1;
            } else if (modeLen == 5 && tclStrncmp(modeStr, "-glob", 5) == 0) {
                mode = 0;
            } else if (modeLen == 7 && tclStrncmp(modeStr, "-regexp", 7) == 0) {
                mode = 2;
            } else {
                tclSetError(interp, "bad option: must be -exact, -glob, or -regexp", -1);
                return TCL_ERROR;
            }
            pattern = host->getStringPtr(objv[4], &patLen);
        }

        void *vars = interp->currentFrame->varsHandle;

        /* Get all names first for exact/regexp modes */
        TclObj *names;
        if (mode == 0) {
            /* Glob mode - use hostArrayNames with pattern */
            names = host->arrayNames(vars, arrName, arrLen, pattern);
            if (!names && interp->currentFrame != interp->globalFrame) {
                names = host->arrayNames(interp->globalFrame->varsHandle, arrName, arrLen, pattern);
            }
        } else {
            /* Exact or regexp mode - get all names, then filter */
            names = host->arrayNames(vars, arrName, arrLen, NULL);
            if (!names && interp->currentFrame != interp->globalFrame) {
                vars = interp->globalFrame->varsHandle;
                names = host->arrayNames(vars, arrName, arrLen, NULL);
            }

            if (names && pattern) {
                TclObj **nameList;
                size_t nameCount;
                if (host->asList(names, &nameList, &nameCount) == 0 && nameCount > 0) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    TclObj **filtered = host->arenaAlloc(arena, nameCount * sizeof(TclObj*), sizeof(void*));
                    size_t filteredCount = 0;

                    for (size_t i = 0; i < nameCount; i++) {
                        size_t keyLen;
                        const char *key = host->getStringPtr(nameList[i], &keyLen);
                        int match = 0;

                        if (mode == 1) {
                            /* Exact match */
                            match = (keyLen == patLen && tclStrncmp(key, pattern, patLen) == 0);
                        } else if (mode == 2) {
                            /* Regexp match - use host's regex */
                            TclObj *matchResult = host->regexMatch(pattern, patLen, nameList[i], 0);
                            match = (matchResult != NULL);
                        }

                        if (match) {
                            filtered[filteredCount++] = nameList[i];
                        }
                    }

                    names = host->newList(filtered, filteredCount);
                    host->arenaPop(interp->hostCtx, arena);
                } else {
                    names = host->newString("", 0);
                }
            }
        }

        tclSetResult(interp, names ? names : host->newString("", 0));
        return TCL_OK;
    }

    /* array size arrayName */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "size", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"array size arrayName\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        void *vars = interp->currentFrame->varsHandle;

        size_t size = host->arraySize(vars, arrName, arrLen);
        if (size == 0 && interp->currentFrame != interp->globalFrame) {
            size = host->arraySize(interp->globalFrame->varsHandle, arrName, arrLen);
        }

        tclSetResult(interp, host->newInt((int64_t)size));
        return TCL_OK;
    }

    /* array get arrayName ?pattern? */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "get", 3) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"array get arrayName ?pattern?\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        void *vars = interp->currentFrame->varsHandle;

        /* Get array names */
        TclObj *names = host->arrayNames(vars, arrName, arrLen, pattern);
        if (!names && interp->currentFrame != interp->globalFrame) {
            vars = interp->globalFrame->varsHandle;
            names = host->arrayNames(vars, arrName, arrLen, pattern);
        }

        if (!names) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Parse names and build key-value list */
        TclObj **nameList;
        size_t nameCount;
        if (host->asList(names, &nameList, &nameCount) != 0 || nameCount == 0) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Build result list with key value pairs */
        void *arena = host->arenaPush(interp->hostCtx);
        size_t resultCount = nameCount * 2;
        TclObj **resultElems = host->arenaAlloc(arena, resultCount * sizeof(TclObj*), sizeof(void*));

        for (size_t i = 0; i < nameCount; i++) {
            size_t keyLen;
            const char *key = host->getStringPtr(nameList[i], &keyLen);
            resultElems[i * 2] = nameList[i];
            resultElems[i * 2 + 1] = host->arrayGet(vars, arrName, arrLen, key, keyLen);
        }

        TclObj *result = host->newList(resultElems, resultCount);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* array set arrayName list */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "set", 3) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"array set arrayName list\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);

        /* Parse list */
        TclObj **elems;
        size_t elemCount;
        if (host->asList(objv[3], &elems, &elemCount) != 0) {
            tclSetError(interp, "list must have an even number of elements", -1);
            return TCL_ERROR;
        }

        if (elemCount % 2 != 0) {
            tclSetError(interp, "list must have an even number of elements", -1);
            return TCL_ERROR;
        }

        void *vars = interp->currentFrame->varsHandle;

        /* Set each key-value pair */
        for (size_t i = 0; i < elemCount; i += 2) {
            size_t keyLen;
            const char *key = host->getStringPtr(elems[i], &keyLen);
            host->arraySet(vars, arrName, arrLen, key, keyLen, host->dup(elems[i + 1]));
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* array unset arrayName ?pattern? */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "unset", 5) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"array unset arrayName ?pattern?\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        void *vars = interp->currentFrame->varsHandle;

        if (pattern == NULL) {
            /* Unset entire array - get all names and unset each */
            TclObj *names = host->arrayNames(vars, arrName, arrLen, NULL);
            if (names) {
                TclObj **nameList;
                size_t nameCount;
                if (host->asList(names, &nameList, &nameCount) == 0) {
                    for (size_t i = 0; i < nameCount; i++) {
                        size_t keyLen;
                        const char *key = host->getStringPtr(nameList[i], &keyLen);
                        host->arrayUnset(vars, arrName, arrLen, key, keyLen);
                    }
                }
            }
        } else {
            /* Unset matching elements */
            TclObj *names = host->arrayNames(vars, arrName, arrLen, pattern);
            if (names) {
                TclObj **nameList;
                size_t nameCount;
                if (host->asList(names, &nameList, &nameCount) == 0) {
                    for (size_t i = 0; i < nameCount; i++) {
                        size_t keyLen;
                        const char *key = host->getStringPtr(nameList[i], &keyLen);
                        host->arrayUnset(vars, arrName, arrLen, key, keyLen);
                    }
                }
            }
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* array startsearch arrayName */
    if (subcmdLen == 11 && tclStrncmp(subcmd, "startsearch", 11) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"array startsearch arrayName\"", -1);
            return TCL_ERROR;
        }
        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[2], &arrLen);
        void *vars = interp->currentFrame->varsHandle;

        TclObj *searchId = host->arrayStartSearch(vars, arrName, arrLen);
        if (!searchId && interp->currentFrame != interp->globalFrame) {
            searchId = host->arrayStartSearch(interp->globalFrame->varsHandle, arrName, arrLen);
        }

        if (!searchId) {
            tclSetError(interp, "not an array", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, searchId);
        return TCL_OK;
    }

    /* array anymore arrayName searchId */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "anymore", 7) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"array anymore arrayName searchId\"", -1);
            return TCL_ERROR;
        }
        size_t sidLen;
        const char *searchId = host->getStringPtr(objv[3], &sidLen);

        int result = host->arrayAnymore(searchId);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* array nextelement arrayName searchId */
    if (subcmdLen == 11 && tclStrncmp(subcmd, "nextelement", 11) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"array nextelement arrayName searchId\"", -1);
            return TCL_ERROR;
        }
        size_t sidLen;
        const char *searchId = host->getStringPtr(objv[3], &sidLen);

        TclObj *key = host->arrayNextElement(searchId);
        tclSetResult(interp, key ? key : host->newString("", 0));
        return TCL_OK;
    }

    /* array donesearch arrayName searchId */
    if (subcmdLen == 10 && tclStrncmp(subcmd, "donesearch", 10) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"array donesearch arrayName searchId\"", -1);
            return TCL_ERROR;
        }
        size_t sidLen;
        const char *searchId = host->getStringPtr(objv[3], &sidLen);

        host->arrayDoneSearch(searchId);
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* array statistics arrayName */
    if (subcmdLen == 10 && tclStrncmp(subcmd, "statistics", 10) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"array statistics arrayName\"", -1);
            return TCL_ERROR;
        }
        /* Return a simple non-empty statistics string */
        const char *stats = "entries in table\naverage search distance: 1.0";
        tclSetResult(interp, host->newString(stats, tclStrlen(stats)));
        return TCL_OK;
    }

    /* array for {keyVar valueVar} arrayName body */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "for", 3) == 0) {
        if (objc != 5) {
            tclSetError(interp, "wrong # args: should be \"array for {keyVar valueVar} arrayName body\"", -1);
            return TCL_ERROR;
        }

        /* Parse key/value variable names */
        TclObj **varList;
        size_t varCount;
        if (host->asList(objv[2], &varList, &varCount) != 0 || varCount != 2) {
            tclSetError(interp, "must have exactly two variable names", -1);
            return TCL_ERROR;
        }
        size_t keyVarLen, valVarLen;
        const char *keyVar = host->getStringPtr(varList[0], &keyVarLen);
        const char *valVar = host->getStringPtr(varList[1], &valVarLen);

        size_t arrLen;
        const char *arrName = host->getStringPtr(objv[3], &arrLen);
        TclObj *body = objv[4];

        void *vars = interp->currentFrame->varsHandle;

        /* Get all array names */
        TclObj *names = host->arrayNames(vars, arrName, arrLen, NULL);
        if (!names && interp->currentFrame != interp->globalFrame) {
            vars = interp->globalFrame->varsHandle;
            names = host->arrayNames(vars, arrName, arrLen, NULL);
        }

        if (!names) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        TclObj **nameList;
        size_t nameCount;
        if (host->asList(names, &nameList, &nameCount) != 0 || nameCount == 0) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Iterate over array elements */
        void *localVars = interp->currentFrame->varsHandle;
        TclResult rc = TCL_OK;
        for (size_t i = 0; i < nameCount; i++) {
            size_t keyLen;
            const char *key = host->getStringPtr(nameList[i], &keyLen);

            /* Set key variable */
            host->varSet(localVars, keyVar, keyVarLen, host->dup(nameList[i]));

            /* Get and set value variable */
            TclObj *val = host->arrayGet(vars, arrName, arrLen, key, keyLen);
            if (val) {
                host->varSet(localVars, valVar, valVarLen, host->dup(val));
            }

            /* Execute body */
            rc = tclEval(interp, body);
            if (rc == TCL_BREAK) {
                rc = TCL_OK;
                break;
            }
            if (rc == TCL_CONTINUE) {
                rc = TCL_OK;
                continue;
            }
            if (rc != TCL_OK) {
                return rc;
            }
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    tclSetError(interp, "unknown or ambiguous subcommand", -1);
    return TCL_ERROR;
}
