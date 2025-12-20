/*
 * builtin_lmap.c - TCL lmap Command Implementation
 *
 * lmap command: Like foreach but collects body results into a list.
 * Supports multiple var/list pairs like foreach.
 */

#include "internal.h"

/* ========================================================================
 * lmap Command
 * ======================================================================== */

TclResult tclCmdLmap(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Need at least: lmap varList list body */
    if (objc < 4 || (objc % 2) != 0) {
        tclSetError(interp, "wrong # args: should be \"lmap varList list ?varList list ...? command\"", -1);
        return TCL_ERROR;
    }

    /* Body is the last argument */
    TclObj *body = objv[objc - 1];

    /* Number of var/list pairs */
    int numPairs = (objc - 2) / 2;

    /* Use arena for temporary allocations */
    void *arena = host->arenaPush(interp->hostCtx);

    /* Parse all var lists and value lists */
    typedef struct {
        TclObj **varNames;      /* Array of variable name objects */
        size_t varCount;        /* Number of variables in this list */
        TclObj **values;        /* Array of value objects */
        size_t valueCount;      /* Number of values in this list */
        size_t currentIndex;    /* Current iteration index */
    } VarListPair;

    VarListPair *pairs = host->arenaAlloc(arena, sizeof(VarListPair) * numPairs, sizeof(void*));

    /* Parse all var/list pairs */
    for (int i = 0; i < numPairs; i++) {
        int varListIdx = 1 + (i * 2);
        int valueListIdx = 1 + (i * 2) + 1;

        /* Parse variable list */
        if (host->asList(objv[varListIdx], &pairs[i].varNames, &pairs[i].varCount) != 0) {
            host->arenaPop(interp->hostCtx, arena);
            tclSetError(interp, "invalid variable list", -1);
            return TCL_ERROR;
        }

        /* Variable list cannot be empty */
        if (pairs[i].varCount == 0) {
            host->arenaPop(interp->hostCtx, arena);
            tclSetError(interp, "foreach varlist is empty", -1);
            return TCL_ERROR;
        }

        /* Parse value list */
        if (host->asList(objv[valueListIdx], &pairs[i].values, &pairs[i].valueCount) != 0) {
            host->arenaPop(interp->hostCtx, arena);
            tclSetError(interp, "invalid list", -1);
            return TCL_ERROR;
        }

        pairs[i].currentIndex = 0;
    }

    /* Result list - we'll collect results here */
    /* Start with a reasonable initial capacity */
    size_t resultCapacity = 16;
    size_t resultCount = 0;
    TclObj **resultList = host->arenaAlloc(arena, sizeof(TclObj*) * resultCapacity, sizeof(void*));

    void *vars = interp->currentFrame->varsHandle;

    /* Loop until all lists are exhausted */
    int continueLoop = 1;
    while (continueLoop) {
        /* Check if any list still has elements */
        continueLoop = 0;
        for (int i = 0; i < numPairs; i++) {
            if (pairs[i].currentIndex < pairs[i].valueCount) {
                continueLoop = 1;
                break;
            }
        }

        if (!continueLoop) {
            break;
        }

        /* Set variables for this iteration */
        for (int i = 0; i < numPairs; i++) {
            for (size_t v = 0; v < pairs[i].varCount; v++) {
                size_t varNameLen;
                const char *varName = host->getStringPtr(pairs[i].varNames[v], &varNameLen);

                /* Calculate which value to use */
                size_t valueIdx = pairs[i].currentIndex + v;

                if (valueIdx < pairs[i].valueCount) {
                    /* Set variable to the value */
                    host->varSet(vars, varName, varNameLen, host->dup(pairs[i].values[valueIdx]));
                } else {
                    /* Past end of list, set to empty string */
                    host->varSet(vars, varName, varNameLen, host->newString("", 0));
                }
            }

            /* Advance index by number of variables consumed */
            pairs[i].currentIndex += pairs[i].varCount;
        }

        /* Execute body */
        TclResult result = tclEvalObj(interp, body, 0);

        if (result == TCL_BREAK) {
            break;
        }
        if (result == TCL_CONTINUE) {
            continue;  /* Skip adding to result list */
        }
        if (result == TCL_ERROR || result == TCL_RETURN) {
            /* Clean up and return error */
            host->arenaPop(interp->hostCtx, arena);
            return result;
        }

        /* Add result to list (result == TCL_OK) */
        if (resultCount >= resultCapacity) {
            /* Need to grow the array - allocate new one and copy */
            size_t newCapacity = resultCapacity * 2;
            TclObj **newList = host->arenaAlloc(arena, sizeof(TclObj*) * newCapacity, sizeof(void*));
            /* Copy old elements */
            for (size_t i = 0; i < resultCount; i++) {
                newList[i] = resultList[i];
            }
            resultList = newList;
            resultCapacity = newCapacity;
        }

        /* Add the result */
        resultList[resultCount++] = host->dup(interp->result);
    }

    /* Build final result list */
    TclObj *finalResult = host->newList(resultList, resultCount);

    /* Clean up arena */
    host->arenaPop(interp->hostCtx, arena);

    tclSetResult(interp, finalResult);
    return TCL_OK;
}
