/*
 * builtin_linsert.c - TCL linsert Command Implementation
 *
 * linsert command: insert elements into a list at a specified index
 */

#include "internal.h"

/* Parse an index value for linsert (supports "end", "end-N", and integers) */
static int parseInsertIndex(const TclHost *host, TclObj *indexObj, size_t listLen, size_t *out) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    /* Check for "end" */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (idxLen == 3) {
            /* "end" means insert after last element (at length position) */
            *out = listLen;
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
            /* end-N: insert before (length - N)th element */
            if (offset > (int64_t)listLen) {
                *out = 0;  /* Goes before start, insert at beginning */
            } else {
                *out = listLen - (size_t)offset;
            }
            return 0;
        }
        return -1;
    }

    /* Try as integer */
    int64_t idx;
    if (host->asInt(indexObj, &idx) != 0) {
        return -1;
    }

    /* Negative or zero index: insert at beginning */
    if (idx <= 0) {
        *out = 0;
        return 0;
    }

    /* Index >= length: insert at end */
    if ((size_t)idx >= listLen) {
        *out = listLen;
        return 0;
    }

    *out = (size_t)idx;
    return 0;
}

TclResult tclCmdLinsert(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"linsert list index ?element ...?\"", -1);
        return TCL_ERROR;
    }

    /* Get the list */
    TclObj *listObj = objv[1];
    size_t listLen = host->listLength(listObj);

    /* Parse the index */
    size_t insertIdx;
    if (parseInsertIndex(host, objv[2], listLen, &insertIdx) != 0) {
        tclSetError(interp, "bad index: must be integer or end?[+-]integer?", -1);
        return TCL_ERROR;
    }

    /* If no elements to insert, return the original list */
    if (objc == 3) {
        tclSetResult(interp, host->dup(listObj));
        return TCL_OK;
    }

    /* Calculate new list size */
    size_t numInsert = objc - 3;
    size_t newLen = listLen + numInsert;

    /* Build the new list using arena */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **newElems = host->arenaAlloc(arena, newLen * sizeof(TclObj*), sizeof(void*));

    /* Copy elements before insertion point */
    for (size_t i = 0; i < insertIdx; i++) {
        newElems[i] = host->listIndex(listObj, i);
    }

    /* Insert new elements */
    for (size_t i = 0; i < numInsert; i++) {
        newElems[insertIdx + i] = objv[3 + i];
    }

    /* Copy elements after insertion point */
    for (size_t i = insertIdx; i < listLen; i++) {
        newElems[i + numInsert] = host->listIndex(listObj, i);
    }

    /* Create result list */
    TclObj *result = host->newList(newElems, newLen);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}
