/*
 * builtin_lseq.c - TCL lseq Command Implementation
 *
 * Generates sequences of numbers similar to seq(1) or range functions.
 */

#include "internal.h"
#include <math.h>

/* ========================================================================
 * lseq Command
 * ======================================================================== */

/* Helper to check if a string represents a double */
static int isDouble(const char *str, size_t len) {
    int hasDot = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '.') {
            if (hasDot) return 0;  /* Multiple dots */
            hasDot = 1;
        } else if (str[i] == '-' || str[i] == '+') {
            if (i != 0) return 0;  /* Sign not at start */
        } else if (str[i] < '0' || str[i] > '9') {
            /* Check for 'e' or 'E' for scientific notation */
            if (str[i] == 'e' || str[i] == 'E') {
                hasDot = 1;  /* Treat scientific notation as double */
            } else {
                return 0;
            }
        }
    }
    return hasDot;
}

TclResult tclCmdLseq(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    /* Validate argument count */
    if (objc < 2 || objc > 6) {
        tclSetError(interp, "wrong # args: should be \"lseq n ??op? n ??by? n??\"", -1);
        return TCL_ERROR;
    }

    /* Parse arguments to determine form:
     * 1 arg:  lseq count           -> 0 to count-1
     * 2 args: lseq start end       -> start to end
     * 3 args: lseq start to end    -> start to end
     *         lseq start .. end    -> start to end
     *         lseq start count N   -> N elements from start
     * 4 args: lseq start end by step
     * 5 args: lseq start to end by step
     *         lseq start .. end by step
     */

    double start = 0.0;
    double end = 0.0;
    double step = 1.0;
    int useCount = 0;
    int64_t count = 0;
    int useDouble = 0;

    if (objc == 2) {
        /* Form: lseq count */
        int64_t n;
        if (host->asInt(objv[1], &n) != 0) {
            /* Try as double */
            if (host->asDouble(objv[1], &end) != 0) {
                tclSetError(interp, "expected integer but got something else", -1);
                return TCL_ERROR;
            }
            useDouble = 1;
            start = 0.0;
            if (end < 0.0) end = 0.0;
            if (end > 0.0) end -= 1.0;
        } else {
            start = 0.0;
            if (n <= 0) {
                tclSetResult(interp, host->newString("", 0));
                return TCL_OK;
            }
            end = (double)(n - 1);
        }
    } else if (objc == 3) {
        /* Form: lseq start end */
        if (host->asInt(objv[1], &count) == 0 && host->asInt(objv[2], &count) == 0) {
            /* Both are integers */
            int64_t s, e;
            host->asInt(objv[1], &s);
            host->asInt(objv[2], &e);
            start = (double)s;
            end = (double)e;
        } else {
            /* At least one is double */
            if (host->asDouble(objv[1], &start) != 0) {
                tclSetError(interp, "expected number but got something else", -1);
                return TCL_ERROR;
            }
            if (host->asDouble(objv[2], &end) != 0) {
                tclSetError(interp, "expected number but got something else", -1);
                return TCL_ERROR;
            }
            useDouble = 1;
        }
    } else if (objc == 4) {
        /* Form: lseq start to/.. end OR lseq start count N */
        size_t opLen;
        const char *op = host->getStringPtr(objv[2], &opLen);

        if ((opLen == 2 && tclStrncmp(op, "to", 2) == 0) ||
            (opLen == 2 && tclStrncmp(op, "..", 2) == 0)) {
            /* lseq start to/.. end */
            if (host->asDouble(objv[1], &start) != 0 ||
                host->asDouble(objv[3], &end) != 0) {
                tclSetError(interp, "expected number but got something else", -1);
                return TCL_ERROR;
            }
            /* Check if we need doubles */
            size_t len1, len3;
            const char *s1 = host->getStringPtr(objv[1], &len1);
            const char *s3 = host->getStringPtr(objv[3], &len3);
            if (isDouble(s1, len1) || isDouble(s3, len3)) {
                useDouble = 1;
            }
        } else if (opLen == 5 && tclStrncmp(op, "count", 5) == 0) {
            /* lseq start count N */
            if (host->asDouble(objv[1], &start) != 0) {
                tclSetError(interp, "expected number but got something else", -1);
                return TCL_ERROR;
            }
            if (host->asInt(objv[3], &count) != 0) {
                tclSetError(interp, "expected integer but got something else", -1);
                return TCL_ERROR;
            }
            useCount = 1;
            size_t len1;
            const char *s1 = host->getStringPtr(objv[1], &len1);
            if (isDouble(s1, len1)) {
                useDouble = 1;
            }
        } else {
            /* Try as: lseq start end by step (missing 'by') */
            tclSetError(interp, "wrong # args: should be \"lseq n ??op? n ??by? n??\"", -1);
            return TCL_ERROR;
        }
    } else if (objc == 5) {
        /* Form: lseq start end by step (missing to/..) */
        size_t opLen;
        const char *op = host->getStringPtr(objv[3], &opLen);

        if (opLen == 2 && tclStrncmp(op, "by", 2) == 0) {
            /* lseq start end by step */
            if (host->asDouble(objv[1], &start) != 0 ||
                host->asDouble(objv[2], &end) != 0 ||
                host->asDouble(objv[4], &step) != 0) {
                tclSetError(interp, "expected number but got something else", -1);
                return TCL_ERROR;
            }
            /* Check if we need doubles */
            size_t len1, len2, len4;
            const char *s1 = host->getStringPtr(objv[1], &len1);
            const char *s2 = host->getStringPtr(objv[2], &len2);
            const char *s4 = host->getStringPtr(objv[4], &len4);
            if (isDouble(s1, len1) || isDouble(s2, len2) || isDouble(s4, len4)) {
                useDouble = 1;
            }
        } else {
            tclSetError(interp, "wrong # args: should be \"lseq n ??op? n ??by? n??\"", -1);
            return TCL_ERROR;
        }
    } else if (objc == 6) {
        /* Form: lseq start to/.. end by step */
        size_t opLen;
        const char *op = host->getStringPtr(objv[2], &opLen);
        size_t byLen;
        const char *by = host->getStringPtr(objv[4], &byLen);

        if (((opLen == 2 && tclStrncmp(op, "to", 2) == 0) ||
             (opLen == 2 && tclStrncmp(op, "..", 2) == 0)) &&
            (byLen == 2 && tclStrncmp(by, "by", 2) == 0)) {
            /* lseq start to/.. end by step */
            if (host->asDouble(objv[1], &start) != 0 ||
                host->asDouble(objv[3], &end) != 0 ||
                host->asDouble(objv[5], &step) != 0) {
                tclSetError(interp, "expected number but got something else", -1);
                return TCL_ERROR;
            }
            /* Check if we need doubles */
            size_t len1, len3, len5;
            const char *s1 = host->getStringPtr(objv[1], &len1);
            const char *s3 = host->getStringPtr(objv[3], &len3);
            const char *s5 = host->getStringPtr(objv[5], &len5);
            if (isDouble(s1, len1) || isDouble(s3, len3) || isDouble(s5, len5)) {
                useDouble = 1;
            }
        } else {
            tclSetError(interp, "wrong # args: should be \"lseq n ??op? n ??by? n??\"", -1);
            return TCL_ERROR;
        }
    }

    /* Handle count form */
    if (useCount) {
        if (count <= 0) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }
        end = start + (double)(count - 1) * step;
    }

    /* Determine step direction if not explicitly set */
    if (objc == 2 || objc == 3 || (objc == 4 && !useCount)) {
        if (end < start) {
            step = -1.0;
        } else {
            step = 1.0;
        }
    }

    /* Check for zero step */
    if (step == 0.0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Check for wrong direction */
    if ((end > start && step < 0.0) || (end < start && step > 0.0)) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Calculate number of elements */
    int64_t numElements;
    if (step > 0.0) {
        numElements = (int64_t)((end - start) / step) + 1;
    } else {
        numElements = (int64_t)((start - end) / (-step)) + 1;
    }

    if (numElements <= 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Generate sequence */
    void *arena = host->arenaPush(interp->hostCtx);
    TclObj **elems = host->arenaAlloc(arena, numElements * sizeof(TclObj*), sizeof(void*));

    for (int64_t i = 0; i < numElements; i++) {
        double val = start + i * step;

        /* Handle floating point precision for the last element */
        if (step > 0.0 && val > end) val = end;
        if (step < 0.0 && val < end) val = end;

        if (useDouble) {
            elems[i] = host->newDouble(val);
        } else {
            elems[i] = host->newInt((int64_t)val);
        }
    }

    TclObj *result = host->newList(elems, numElements);
    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}
