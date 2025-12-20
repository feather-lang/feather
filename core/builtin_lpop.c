/*
 * builtin_lpop.c - TCL lpop Command Implementation
 *
 * lpop varName ?index ...? - removes and returns element from list variable
 */

#include "internal.h"

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

/* Build error message for index out of range */
static void setIndexError(TclInterp *interp, TclObj *indexObj) {
    const TclHost *host = interp->host;
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    /* Build error message */
    void *arena = host->arenaPush(interp->hostCtx);
    size_t msgLen = 7 + idxLen + 14;  /* "index \"" + idx + "\" out of range" */
    char *msg = host->arenaAlloc(arena, msgLen + 1, 1);
    char *p = msg;

    const char *prefix = "index \"";
    for (size_t i = 0; i < 7; i++) *p++ = prefix[i];
    for (size_t i = 0; i < idxLen; i++) *p++ = idxStr[i];
    const char *suffix = "\" out of range";
    for (size_t i = 0; i < 14; i++) *p++ = suffix[i];
    *p = '\0';

    tclSetError(interp, msg, msgLen);
    host->arenaPop(interp->hostCtx, arena);
}

/* Recursively pop from nested list structure */
static TclResult lpopRecursive(TclInterp *interp, TclObj *list,
                                TclObj **indices, int indexCount,
                                TclObj **poppedOut, TclObj **newListOut) {
    const TclHost *host = interp->host;

    /* Parse the list */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(list, &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    /* Parse the index */
    size_t idx;
    if (parseListIndex(host, indices[0], elemCount, &idx) != 0) {
        setIndexError(interp, indices[0]);
        return TCL_ERROR;
    }

    /* If this is the final index, pop the element */
    if (indexCount == 1) {
        /* Return the popped element */
        *poppedOut = host->dup(elems[idx]);

        /* Build new list without this element */
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **newElems = host->arenaAlloc(arena, (elemCount - 1) * sizeof(TclObj*), sizeof(void*));

        size_t j = 0;
        for (size_t i = 0; i < elemCount; i++) {
            if (i != idx) {
                newElems[j++] = elems[i];
            }
        }

        *newListOut = host->newList(newElems, elemCount - 1);
        host->arenaPop(interp->hostCtx, arena);
        return TCL_OK;
    }

    /* Recursively pop from nested element */
    TclObj *poppedElem;
    TclObj *newNestedList;
    TclResult res = lpopRecursive(interp, elems[idx], indices + 1, indexCount - 1,
                                   &poppedElem, &newNestedList);
    if (res != TCL_OK) {
        return res;
    }

    *poppedOut = poppedElem;

    /* Build new list with modified element at idx */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **newElems = host->arenaAlloc(arena, elemCount * sizeof(TclObj*), sizeof(void*));

    for (size_t i = 0; i < elemCount; i++) {
        if (i == idx) {
            newElems[i] = newNestedList;
        } else {
            newElems[i] = elems[i];
        }
    }

    *newListOut = host->newList(newElems, elemCount);
    host->arenaPop(interp->hostCtx, arena);
    return TCL_OK;
}

TclResult tclCmdLpop(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lpop listvar ?index?\"", -1);
        return TCL_ERROR;
    }

    /* Get variable name */
    size_t varLen;
    const char *varName = host->getStringPtr(objv[1], &varLen);
    void *vars = interp->currentFrame->varsHandle;

    /* Get current value */
    TclObj *current = host->varGet(vars, varName, varLen);
    if (!current) {
        tclSetError(interp, "can't read variable", -1);
        return TCL_ERROR;
    }

    /* Determine indices - default is "end" */
    TclObj **indices;
    int indexCount;

    if (objc == 2) {
        /* Use default index "end" */
        void *arena = host->arenaPush(interp->hostCtx);
        indices = host->arenaAlloc(arena, sizeof(TclObj*), sizeof(void*));
        indices[0] = host->newString("end", 3);
        indexCount = 1;

        TclObj *poppedElem;
        TclObj *newList;
        TclResult res = lpopRecursive(interp, current, indices, indexCount,
                                      &poppedElem, &newList);
        host->arenaPop(interp->hostCtx, arena);

        if (res != TCL_OK) {
            return res;
        }

        /* Set the variable */
        host->varSet(vars, varName, varLen, newList);

        tclSetResult(interp, poppedElem);
        return TCL_OK;
    } else {
        /* Use provided indices */
        indices = objv + 2;
        indexCount = objc - 2;

        TclObj *poppedElem;
        TclObj *newList;
        TclResult res = lpopRecursive(interp, current, indices, indexCount,
                                      &poppedElem, &newList);

        if (res != TCL_OK) {
            return res;
        }

        /* Set the variable */
        host->varSet(vars, varName, varLen, newList);

        tclSetResult(interp, poppedElem);
        return TCL_OK;
    }
}
