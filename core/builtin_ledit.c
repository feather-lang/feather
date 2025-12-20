/*
 * builtin_ledit.c - TCL ledit Command Implementation
 *
 * ledit listVar first last ?element ...?
 * Like lreplace but modifies the variable in place.
 */

#include "internal.h"

/* Parse an index value for ledit (supports "end", "end-N", "end+N", and integers) */
static int parseLeditIndex(const TclHost *host, TclObj *indexObj, size_t listLen,
                           int64_t *outIdx, int *isPastEnd, int *isNegative) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    *isPastEnd = 0;
    *isNegative = 0;

    /* Check for "end" */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (listLen == 0) {
            *outIdx = 0;
            return 0;
        }
        if (idxLen == 3) {
            *outIdx = (int64_t)(listLen - 1);
            return 0;
        }
        /* end-N or end+N */
        if (idxLen > 4 && (idxStr[3] == '-' || idxStr[3] == '+')) {
            int64_t offset = 0;
            int isAdd = (idxStr[3] == '+');
            for (size_t i = 4; i < idxLen; i++) {
                if (idxStr[i] >= '0' && idxStr[i] <= '9') {
                    offset = offset * 10 + (idxStr[i] - '0');
                } else {
                    return -1;
                }
            }
            if (isAdd) {
                *outIdx = (int64_t)(listLen - 1) + offset;
                if (*outIdx >= (int64_t)listLen) {
                    *isPastEnd = 1;
                }
            } else {
                if (offset > (int64_t)(listLen - 1)) {
                    *outIdx = -((int64_t)offset - (int64_t)(listLen - 1));
                    *isNegative = 1;
                } else {
                    *outIdx = (int64_t)(listLen - 1) - offset;
                }
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

    *outIdx = idx;
    if (idx < 0) {
        *isNegative = 1;
    } else if ((size_t)idx >= listLen) {
        *isPastEnd = 1;
    }

    return 0;
}

TclResult tclCmdLedit(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 4) {
        tclSetError(interp, "wrong # args: should be \"ledit listVar first last ?element ...?\"", -1);
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

    /* Get list elements */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(current, &elems, &elemCount) != 0) {
        tclSetError(interp, "variable value is not a valid list", -1);
        return TCL_ERROR;
    }

    /* Parse first index */
    int64_t firstIdx;
    int firstPastEnd, firstNegative;
    if (parseLeditIndex(host, objv[2], elemCount, &firstIdx, &firstPastEnd, &firstNegative) != 0) {
        tclSetError(interp, "bad index: must be integer?[+-]integer? or end?[+-]integer?", -1);
        return TCL_ERROR;
    }

    /* Parse last index */
    int64_t lastIdx;
    int lastPastEnd, lastNegative;
    if (parseLeditIndex(host, objv[3], elemCount, &lastIdx, &lastPastEnd, &lastNegative) != 0) {
        tclSetError(interp, "bad index: must be integer?[+-]integer? or end?[+-]integer?", -1);
        return TCL_ERROR;
    }

    /* Normalize indices for the replace operation */
    size_t replaceStart, replaceEnd;
    int isInsert = 0;

    /* Handle negative first index (prepend) */
    if (firstNegative) {
        replaceStart = 0;
        isInsert = 1;  /* Insert at beginning */
        replaceEnd = 0;  /* No elements to delete */
    }
    /* Handle first past end (append) */
    else if (firstPastEnd) {
        replaceStart = elemCount;
        isInsert = 1;  /* Insert at end */
        replaceEnd = 0;  /* No elements to delete */
    }
    else {
        replaceStart = (size_t)firstIdx;
    }

    /* Handle last index */
    if (!isInsert) {
        if (lastNegative || lastIdx < firstIdx) {
            /* last < first means insert without deleting */
            isInsert = 1;
            replaceEnd = replaceStart;  /* No deletion */
        } else if (lastPastEnd || (size_t)lastIdx >= elemCount) {
            replaceEnd = elemCount - 1;
        } else {
            replaceEnd = (size_t)lastIdx;
        }
    }

    /* Build the new list */
    void *arena = host->arenaPush(interp->hostCtx);

    /* Calculate new element count */
    size_t newElemCount = objc - 4;  /* Replacement elements */
    size_t beforeCount = replaceStart;
    size_t afterCount;

    if (isInsert) {
        if (replaceStart >= elemCount) {
            afterCount = 0;
        } else {
            afterCount = elemCount - replaceStart;
        }
    } else {
        if (replaceEnd + 1 >= elemCount) {
            afterCount = 0;
        } else {
            afterCount = elemCount - (replaceEnd + 1);
        }
    }

    size_t totalCount = beforeCount + newElemCount + afterCount;

    /* Allocate array for new list elements */
    TclObj **newElems = host->arenaAlloc(arena, totalCount * sizeof(TclObj*), sizeof(void*));
    size_t pos = 0;

    /* Copy elements before replacement point */
    for (size_t i = 0; i < beforeCount; i++) {
        newElems[pos++] = elems[i];
    }

    /* Add replacement elements */
    for (int i = 4; i < objc; i++) {
        newElems[pos++] = objv[i];
    }

    /* Copy elements after replacement point */
    size_t afterStart = isInsert ? replaceStart : (replaceEnd + 1);
    for (size_t i = 0; i < afterCount; i++) {
        newElems[pos++] = elems[afterStart + i];
    }

    /* Create new list */
    TclObj *result = host->newList(newElems, totalCount);
    host->arenaPop(interp->hostCtx, arena);

    /* Set the variable */
    host->varSet(vars, varName, varLen, result);

    /* Return the new value */
    tclSetResult(interp, host->dup(result));
    return TCL_OK;
}
