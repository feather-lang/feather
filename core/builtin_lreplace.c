/*
 * builtin_lreplace.c - TCL lreplace Command Implementation
 *
 * lreplace list first last ?element ...?
 * - Replaces elements from first to last with new elements
 * - If last < first, inserts without deleting
 * - Negative indices treated as before first element (prepend)
 * - Indices past end treated as after last element (append)
 * - No replacement elements = deletion
 */

#include "internal.h"

/* Parse an index value for lreplace (supports "end", "end-N", and integers)
 * Returns the resolved index in *out
 * Returns 0 on success, -1 if index is out of bounds or invalid
 * Special handling for lreplace:
 *  - Negative indices return -1 with *out set to 0 (prepend)
 *  - Indices >= listLen return -1 with *out set to listLen (append)
 */
static int parseListIndexForReplace(const TclHost *host, TclObj *indexObj, size_t listLen, size_t *out, int *isNegative, int *isPastEnd) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    *isNegative = 0;
    *isPastEnd = 0;

    /* Check for "end" */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (listLen == 0) {
            *out = 0;
            *isPastEnd = 1;
            return -1;
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
                *isNegative = 1;
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
        *isNegative = 1;
        return -1;
    }

    if ((size_t)idx >= listLen) {
        *out = listLen;
        *isPastEnd = 1;
        return -1;
    }

    *out = (size_t)idx;
    return 0;
}

/* ========================================================================
 * lreplace Command
 * ======================================================================== */

TclResult tclCmdLreplace(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 4) {
        tclSetError(interp, "wrong # args: should be \"lreplace list first last ?element ...?\"", -1);
        return TCL_ERROR;
    }

    /* Parse the list */
    TclObj **listElems;
    size_t listLen;
    if (host->asList(objv[1], &listElems, &listLen) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    /* Parse first index */
    size_t first = 0;
    int firstNegative = 0;
    int firstPastEnd = 0;

    if (parseListIndexForReplace(host, objv[2], listLen, &first, &firstNegative, &firstPastEnd) != 0) {
        /* Special handling for out of bounds */
        if (firstPastEnd) {
            /* first is past end - append mode */
            first = listLen;
        } else if (firstNegative) {
            /* first is negative - prepend mode */
            first = 0;
        }
    }

    /* Parse last index */
    size_t last = 0;
    int lastNegative = 0;
    int lastPastEnd = 0;

    if (parseListIndexForReplace(host, objv[3], listLen, &last, &lastNegative, &lastPastEnd) != 0) {
        /* Special handling for out of bounds */
        if (lastPastEnd) {
            /* last is past end */
            last = listLen;
        } else if (lastNegative) {
            /* last is negative */
            last = 0;
        }
    }

    /* Count replacement elements */
    size_t replCount = (objc > 4) ? (size_t)(objc - 4) : 0;

    /* Calculate result size */
    size_t deleteCount = 0;

    /* Handle special cases */
    if (firstNegative && lastNegative) {
        /* Both negative - prepend mode */
        deleteCount = 0;
        first = 0;
    } else if (firstPastEnd) {
        /* first past end - append mode */
        deleteCount = 0;
        first = listLen;
    } else if (last < first) {
        /* Insert mode (no deletion) */
        deleteCount = 0;
    } else {
        /* Normal replace/delete mode */
        if (lastPastEnd) {
            last = listLen - 1;
        }
        if (last >= listLen) {
            last = listLen - 1;
        }
        if (first < listLen && last < listLen && first <= last) {
            deleteCount = last - first + 1;
        }
    }

    size_t resultLen = listLen - deleteCount + replCount;

    /* Build result list */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **resultElems = host->arenaAlloc(arena, resultLen * sizeof(TclObj*), sizeof(void*));
    size_t resultIdx = 0;

    /* Copy elements before first */
    for (size_t i = 0; i < first && i < listLen; i++) {
        resultElems[resultIdx++] = host->dup(listElems[i]);
    }

    /* Add replacement elements */
    for (size_t i = 0; i < replCount; i++) {
        resultElems[resultIdx++] = host->dup(objv[4 + i]);
    }

    /* Copy elements after last */
    size_t afterIdx = first + deleteCount;
    for (size_t i = afterIdx; i < listLen; i++) {
        resultElems[resultIdx++] = host->dup(listElems[i]);
    }

    TclObj *result = host->newList(resultElems, resultLen);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}
