/*
 * builtin_lset.c - TCL lset Command Implementation
 *
 * lset varName ?index ...? newValue
 * Sets element in list variable, returns new value
 */

#include "internal.h"

/* Forward declarations for helper functions */
static int parseListIndex(const TclHost *host, TclObj *indexObj, size_t listLen, size_t *out, int allowAppend);
static TclObj *lsetAtIndex(TclInterp *interp, TclObj *list, TclObj **indices, int idxCount, TclObj *newValue);
static TclObj *buildListWithReplacement(const TclHost *host, TclObj *list, size_t idx, TclObj *newElem);

/* ========================================================================
 * lset Command
 * ======================================================================== */

TclResult tclCmdLset(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"lset listVar ?index? ?index ...? value\"", -1);
        return TCL_ERROR;
    }

    /* Get variable name */
    size_t varLen;
    const char *varName = host->getStringPtr(objv[1], &varLen);
    void *vars = interp->currentFrame->varsHandle;

    /* Get current value */
    TclObj *current = host->varGet(vars, varName, varLen);
    if (!current) {
        /* Variable doesn't exist - create empty list */
        current = host->newString("", 0);
    }

    /* Case 1: No indices - replace entire variable (objc == 3) */
    if (objc == 3) {
        /* lset x {} newValue or lset x newValue */
        /* Check if objv[2] is an empty index */
        size_t arg2Len;
        host->getStringPtr(objv[2], &arg2Len);

        /* This is just "lset varName newValue" - treat newValue as the new list */
        TclObj *newValue = objv[2];
        host->varSet(vars, varName, varLen, host->dup(newValue));
        tclSetResult(interp, host->dup(newValue));
        return TCL_OK;
    }

    /* Case 2: One or more indices */
    /* The last argument is always the new value */
    TclObj *newValue = objv[objc - 1];

    /* Indices are from objv[2] to objv[objc-2] */
    int numIdxArgs = objc - 3;

    /* Check if we have a single index argument that might be a list */
    if (numIdxArgs == 1) {
        /* Check if objv[2] is an empty string */
        size_t idxLen;
        host->getStringPtr(objv[2], &idxLen);
        if (idxLen == 0) {
            /* Empty index - replace entire variable */
            host->varSet(vars, varName, varLen, host->dup(newValue));
            tclSetResult(interp, host->dup(newValue));
            return TCL_OK;
        }

        /* Try to parse as a list of indices */
        TclObj **idxElems;
        size_t idxCount;
        if (host->asList(objv[2], &idxElems, &idxCount) == 0 && idxCount > 1) {
            /* It's a list of indices - use them */
            TclObj *result = lsetAtIndex(interp, current, idxElems, idxCount, newValue);
            if (!result) {
                return TCL_ERROR;
            }
            host->varSet(vars, varName, varLen, result);
            tclSetResult(interp, host->dup(result));
            return TCL_OK;
        }
    }

    /* Normal case: use objv[2] to objv[objc-2] as indices */
    TclObj *result = lsetAtIndex(interp, current, objv + 2, numIdxArgs, newValue);
    if (!result) {
        return TCL_ERROR;
    }

    host->varSet(vars, varName, varLen, result);
    tclSetResult(interp, host->dup(result));
    return TCL_OK;
}

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/* Parse an index value (supports "end", "end-N", and integers) */
static int parseListIndex(const TclHost *host, TclObj *indexObj, size_t listLen, size_t *out, int allowAppend) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    /* Check for "end" */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (listLen == 0) {
            *out = 0;
            return allowAppend ? 0 : -1;  /* Out of bounds unless appending */
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

    if (idx < 0) {
        *out = 0;
        return -1;  /* Out of bounds */
    }

    /* Check if index is at or beyond list length */
    if ((size_t)idx > listLen) {
        *out = 0;
        return -1;  /* Out of bounds */
    }

    if ((size_t)idx == listLen) {
        /* Appending is allowed */
        *out = (size_t)idx;
        return allowAppend ? 0 : -1;
    }

    *out = (size_t)idx;
    return 0;
}

/* Build a new list with element at idx replaced */
static TclObj *buildListWithReplacement(const TclHost *host, TclObj *list, size_t idx, TclObj *newElem) {
    size_t listLen = host->listLength(list);

    /* Handle append case */
    if (idx == listLen) {
        return host->listAppend(list, newElem);
    }

    /* Build new list */
    void *arena = host->arenaPush(NULL);
    TclObj **elems = host->arenaAlloc(arena, listLen * sizeof(TclObj*), sizeof(void*));

    for (size_t i = 0; i < listLen; i++) {
        if (i == idx) {
            elems[i] = newElem;
        } else {
            elems[i] = host->listIndex(list, i);
        }
    }

    TclObj *result = host->newList(elems, listLen);
    host->arenaPop(NULL, arena);
    return result;
}

/* Recursively set element at nested indices */
static TclObj *lsetAtIndex(TclInterp *interp, TclObj *list, TclObj **indices, int idxCount, TclObj *newValue) {
    const TclHost *host = interp->host;

    if (idxCount == 0) {
        /* No more indices - return the new value */
        return host->dup(newValue);
    }

    /* Parse the first index */
    size_t listLen = host->listLength(list);
    size_t idx;
    int allowAppend = (idxCount == 1);  /* Only allow append on last index */

    if (parseListIndex(host, indices[0], listLen, &idx, allowAppend) != 0) {
        /* Index out of range */
        size_t idxStrLen;
        const char *idxStr = host->getStringPtr(indices[0], &idxStrLen);

        /* Build error message */
        void *arena = host->arenaPush(interp->hostCtx);
        char *msg = host->arenaAlloc(arena, 100 + idxStrLen, 1);
        char *p = msg;

        /* Copy "index \"" */
        const char *prefix = "index \"";
        while (*prefix) *p++ = *prefix++;

        /* Copy index string */
        for (size_t i = 0; i < idxStrLen; i++) {
            *p++ = idxStr[i];
        }

        /* Copy "\" out of range" */
        const char *suffix = "\" out of range";
        while (*suffix) *p++ = *suffix++;
        *p = '\0';

        tclSetError(interp, msg, -1);
        host->arenaPop(interp->hostCtx, arena);
        return NULL;
    }

    if (idxCount == 1) {
        /* Last index - replace element */
        return buildListWithReplacement(host, list, idx, newValue);
    }

    /* More indices - recurse into sublist */
    TclObj *sublist = host->listIndex(list, idx);
    if (!sublist) {
        tclSetError(interp, "list index out of range", -1);
        return NULL;
    }

    /* Recursively set in the sublist */
    TclObj *newSublist = lsetAtIndex(interp, sublist, indices + 1, idxCount - 1, newValue);
    if (!newSublist) {
        return NULL;
    }

    /* Build new list with modified sublist */
    return buildListWithReplacement(host, list, idx, newSublist);
}
