/*
 * builtin_list.c - TCL List Command Implementations
 *
 * List commands: list, llength, lindex, lrange, lappend, join, split, lsort
 */

#include "internal.h"

/* ========================================================================
 * list Command
 * ======================================================================== */

TclResult tclCmdList(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Create a list from all arguments (skip command name) */
    if (objc <= 1) {
        /* Empty list */
        tclSetResult(interp, host->newString("", 0));
    } else {
        TclObj *result = host->newList(objv + 1, objc - 1);
        tclSetResult(interp, result);
    }

    return TCL_OK;
}

/* ========================================================================
 * llength Command
 * ======================================================================== */

TclResult tclCmdLlength(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 2) {
        tclSetError(interp, "wrong # args: should be \"llength list\"", -1);
        return TCL_ERROR;
    }

    size_t len = host->listLength(objv[1]);
    tclSetResult(interp, host->newInt((int64_t)len));
    return TCL_OK;
}

/* ========================================================================
 * lindex Command
 * ======================================================================== */

/* Parse an index value (supports "end", "end-N", and integers) */
static int parseListIndex(const TclHost *host, TclObj *indexObj, size_t listLen, size_t *out) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    /* Check for "end" */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (listLen == 0) {
            *out = 0;
            return -1;  /* Out of bounds */
        }
        if (idxLen == 3) {
            *out = listLen - 1;
            return 0;
        }
        /* end-N or end+N */
        if (idxLen > 4 && idxStr[3] == '-') {
            int64_t offset = 0;
            for (size_t i = 4; i < idxLen; i++) {
                if (idxStr[i] >= '0' && idxStr[i] <= '9') {
                    offset = offset * 10 + (idxStr[i] - '0');
                } else {
                    return -1;
                }
            }
            if (offset > (int64_t)(listLen - 1)) {
                *out = 0;
                return -1;
            }
            *out = listLen - 1 - (size_t)offset;
            return 0;
        }
        return -1;
    }

    /* Try as integer */
    int64_t idx;
    if (host->asInt(indexObj, &idx) != 0) {
        return -1;
    }

    if (idx < 0 || (size_t)idx >= listLen) {
        *out = 0;
        return -1;  /* Out of bounds */
    }

    *out = (size_t)idx;
    return 0;
}

/* Helper to apply a single index to current value */
static TclObj *applyIndex(const TclHost *host, TclObj *current, TclObj *indexObj) {
    size_t idxStrLen;
    host->getStringPtr(indexObj, &idxStrLen);
    if (idxStrLen == 0) {
        return current;  /* Empty index, return as-is */
    }

    size_t listLen = host->listLength(current);
    size_t idx;

    if (parseListIndex(host, indexObj, listLen, &idx) != 0) {
        return NULL;  /* Out of bounds or invalid */
    }

    return host->listIndex(current, idx);
}

TclResult tclCmdLindex(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lindex list ?index ...?\"", -1);
        return TCL_ERROR;
    }

    /* If no index, return the list itself */
    if (objc == 2) {
        tclSetResult(interp, host->dup(objv[1]));
        return TCL_OK;
    }

    /* Start with the list */
    TclObj *current = objv[1];

    /* Process each index argument */
    for (int i = 2; i < objc; i++) {
        size_t idxStrLen;
        host->getStringPtr(objv[i], &idxStrLen);

        /* Empty index - return current value */
        if (idxStrLen == 0) {
            continue;
        }

        /* Check if this index argument is itself a list of indices */
        TclObj **idxElems;
        size_t idxCount;
        if (host->asList(objv[i], &idxElems, &idxCount) == 0 && idxCount > 1) {
            /* It's a list of indices - apply each one */
            for (size_t j = 0; j < idxCount; j++) {
                current = applyIndex(host, current, idxElems[j]);
                if (!current) {
                    tclSetResult(interp, host->newString("", 0));
                    return TCL_OK;
                }
            }
        } else {
            /* Single index */
            current = applyIndex(host, current, objv[i]);
            if (!current) {
                tclSetResult(interp, host->newString("", 0));
                return TCL_OK;
            }
        }
    }

    tclSetResult(interp, host->dup(current));
    return TCL_OK;
}

/* ========================================================================
 * lrange Command
 * ======================================================================== */

TclResult tclCmdLrange(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc != 4) {
        tclSetError(interp, "wrong # args: should be \"lrange list first last\"", -1);
        return TCL_ERROR;
    }

    size_t listLen = host->listLength(objv[1]);

    /* Parse first index */
    size_t first = 0;
    if (parseListIndex(host, objv[2], listLen, &first) != 0) {
        /* Check if it's negative - treat as 0 */
        int64_t val;
        if (host->asInt(objv[2], &val) == 0 && val < 0) {
            first = 0;
        } else {
            /* Check for end-N that went negative */
            size_t idxLen;
            const char *idxStr = host->getStringPtr(objv[2], &idxLen);
            if (idxLen >= 5 && tclStrncmp(idxStr, "end-", 4) == 0) {
                first = 0;  /* end-N that goes negative -> treat as 0 for first */
            } else if (listLen == 0) {
                tclSetResult(interp, host->newString("", 0));
                return TCL_OK;
            }
        }
    }

    /* Parse last index */
    size_t last = listLen > 0 ? listLen - 1 : 0;
    int lastNegative = 0;
    if (parseListIndex(host, objv[3], listLen, &last) != 0) {
        /* Check if it's past end - treat as end */
        int64_t val;
        if (host->asInt(objv[3], &val) == 0) {
            if (val >= (int64_t)listLen) {
                last = listLen > 0 ? listLen - 1 : 0;
            } else if (val < 0) {
                lastNegative = 1;
            }
        } else {
            /* Check for end-N that went negative */
            size_t idxLen;
            const char *idxStr = host->getStringPtr(objv[3], &idxLen);
            if (idxLen >= 5 && tclStrncmp(idxStr, "end-", 4) == 0) {
                lastNegative = 1;  /* end-N that goes negative for last */
            }
        }
    }

    /* If last is negative or first > last, return empty */
    if (lastNegative || first > last || listLen == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Use host's listRange */
    TclObj *result = host->listRange(objv[1], first, last);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * lappend Command
 * ======================================================================== */

TclResult tclCmdLappend(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lappend varName ?value ...?\"", -1);
        return TCL_ERROR;
    }

    /* Get variable name */
    size_t varLen;
    const char *varName = host->getStringPtr(objv[1], &varLen);
    void *vars = interp->currentFrame->varsHandle;

    /* Get current value or empty list */
    TclObj *current = host->varGet(vars, varName, varLen);
    if (!current) {
        current = host->newString("", 0);
        /* If variable doesn't exist, create it */
        host->varSet(vars, varName, varLen, host->dup(current));
    }

    /* If no values to append, just return current value */
    if (objc == 2) {
        tclSetResult(interp, host->dup(current));
        return TCL_OK;
    }

    /* Append each value */
    TclObj *result = current;
    for (int i = 2; i < objc; i++) {
        result = host->listAppend(result, objv[i]);
    }

    /* Set the variable */
    host->varSet(vars, varName, varLen, result);

    tclSetResult(interp, host->dup(result));
    return TCL_OK;
}

/* ========================================================================
 * join Command
 * ======================================================================== */

TclResult tclCmdJoin(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"join list ?joinString?\"", -1);
        return TCL_ERROR;
    }

    /* Get join string (default is space) */
    const char *joinStr = " ";
    size_t joinLen = 1;
    if (objc == 3) {
        joinStr = host->getStringPtr(objv[2], &joinLen);
    }

    /* Parse list */
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

    /* Calculate total length */
    size_t totalLen = 0;
    for (size_t i = 0; i < elemCount; i++) {
        size_t len;
        host->getStringPtr(elems[i], &len);
        totalLen += len;
        if (i > 0) totalLen += joinLen;
    }

    /* Build result using arena */
    void *arena = host->arenaPush(interp->hostCtx);
    char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
    char *p = buf;

    for (size_t i = 0; i < elemCount; i++) {
        if (i > 0) {
            for (size_t j = 0; j < joinLen; j++) {
                *p++ = joinStr[j];
            }
        }
        size_t len;
        const char *s = host->getStringPtr(elems[i], &len);
        for (size_t j = 0; j < len; j++) {
            *p++ = s[j];
        }
    }
    *p = '\0';

    TclObj *result = host->newString(buf, totalLen);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * split Command
 * ======================================================================== */

TclResult tclCmdSplit(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"split string ?splitChars?\"", -1);
        return TCL_ERROR;
    }

    /* Get string to split */
    size_t strLen;
    const char *str = host->getStringPtr(objv[1], &strLen);

    /* Get split characters (default is whitespace) */
    const char *splitChars = " \t\n\r";
    size_t splitLen = 4;
    if (objc == 3) {
        splitChars = host->getStringPtr(objv[2], &splitLen);
    }

    /* Empty string returns empty list */
    if (strLen == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* If splitChars is empty, split into individual characters */
    if (splitLen == 0) {
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **elems = host->arenaAlloc(arena, strLen * sizeof(TclObj*), sizeof(void*));
        for (size_t i = 0; i < strLen; i++) {
            elems[i] = host->newString(&str[i], 1);
        }
        TclObj *result = host->newList(elems, strLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* Count elements first */
    size_t maxElems = strLen + 1;  /* Maximum possible elements */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **elems = host->arenaAlloc(arena, maxElems * sizeof(TclObj*), sizeof(void*));
    size_t elemCount = 0;

    /* Helper to check if char is a split char */
    const char *start = str;
    const char *end = str + strLen;
    const char *p = str;

    while (p <= end) {
        int isSplit = 0;
        if (p < end) {
            for (size_t i = 0; i < splitLen; i++) {
                if (*p == splitChars[i]) {
                    isSplit = 1;
                    break;
                }
            }
        }

        if (isSplit || p == end) {
            /* Add element from start to p */
            elems[elemCount++] = host->newString(start, p - start);
            start = p + 1;
        }
        p++;
    }

    TclObj *result = host->newList(elems, elemCount);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * lsort Command
 * ======================================================================== */

TclResult tclCmdLsort(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lsort ?options? list\"", -1);
        return TCL_ERROR;
    }

    /* Parse options */
    /* flags: 1=decreasing, 2=integer, 4=nocase, 8=unique, 16=dictionary, 32=real */
    int flags = 0;
    int listIdx = objc - 1;  /* List is last argument */

    for (int i = 1; i < objc - 1; i++) {
        size_t optLen;
        const char *opt = host->getStringPtr(objv[i], &optLen);

        if (optLen > 0 && opt[0] == '-') {
            if (optLen == 11 && tclStrncmp(opt, "-decreasing", 11) == 0) {
                flags |= 1;
            } else if (optLen == 11 && tclStrncmp(opt, "-increasing", 11) == 0) {
                flags &= ~1;
            } else if (optLen == 8 && tclStrncmp(opt, "-integer", 8) == 0) {
                flags |= 2;
                flags &= ~(16 | 32);  /* Clear dictionary and real */
            } else if (optLen == 6 && tclStrncmp(opt, "-ascii", 6) == 0) {
                flags &= ~(2 | 16 | 32);  /* Clear integer, dictionary, real */
            } else if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
                flags |= 4;
            } else if (optLen == 7 && tclStrncmp(opt, "-unique", 7) == 0) {
                flags |= 8;
            } else if (optLen == 11 && tclStrncmp(opt, "-dictionary", 11) == 0) {
                flags |= 16;
                flags &= ~(2 | 32);  /* Clear integer and real */
            } else if (optLen == 5 && tclStrncmp(opt, "-real", 5) == 0) {
                flags |= 32;
                flags &= ~(2 | 16);  /* Clear integer and dictionary */
            }
        }
    }

    TclObj *result = host->listSort(objv[listIdx], flags);
    tclSetResult(interp, result);
    return TCL_OK;
}
