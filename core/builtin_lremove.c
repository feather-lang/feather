/*
 * builtin_lremove.c - TCL lremove Command Implementation
 *
 * lremove list ?index ...? - removes elements at specified indices
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

TclResult tclCmdLremove(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"lremove list ?index ...?\"", -1);
        return TCL_ERROR;
    }

    /* Get the list */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(objv[1], &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    /* If no indices, return original list */
    if (objc == 2) {
        tclSetResult(interp, host->dup(objv[1]));
        return TCL_OK;
    }

    /* Parse all indices and mark which elements to remove */
    void *arena = host->arenaPush(interp->hostCtx);

    /* Create a bitmap to track which indices to remove */
    int *toRemove = host->arenaAlloc(arena, elemCount * sizeof(int), sizeof(int));
    for (size_t i = 0; i < elemCount; i++) {
        toRemove[i] = 0;
    }

    /* Parse each index argument and mark for removal */
    for (int i = 2; i < objc; i++) {
        size_t idx;
        if (parseListIndex(host, objv[i], elemCount, &idx) == 0) {
            toRemove[idx] = 1;  /* Mark this index for removal */
        }
        /* Silently ignore invalid indices (TCL behavior) */
    }

    /* Count how many elements will remain */
    size_t newCount = 0;
    for (size_t i = 0; i < elemCount; i++) {
        if (!toRemove[i]) {
            newCount++;
        }
    }

    /* Build result list with elements that aren't removed */
    TclObj **newElems = host->arenaAlloc(arena, newCount * sizeof(TclObj*), sizeof(void*));
    size_t j = 0;
    for (size_t i = 0; i < elemCount; i++) {
        if (!toRemove[i]) {
            newElems[j++] = elems[i];
        }
    }

    TclObj *result = host->newList(newElems, newCount);
    host->arenaPop(interp->hostCtx, arena);

    tclSetResult(interp, result);
    return TCL_OK;
}
