/*
 * builtin_lsearch.c - TCL lsearch Command Implementation
 *
 * Search for elements in a list based on various matching modes.
 */

#include "internal.h"

/* Matching modes */
#define MATCH_EXACT     0
#define MATCH_GLOB      1
#define MATCH_REGEXP    2
#define MATCH_SORTED    3

/* Modifier flags */
#define FLAG_ALL        (1 << 0)
#define FLAG_INLINE     (1 << 1)
#define FLAG_NOT        (1 << 2)
#define FLAG_NOCASE     (1 << 3)

/* Content types */
#define TYPE_ASCII      0
#define TYPE_INTEGER    1

/* Regex flags for host functions */
#define REGEX_FLAG_NOCASE   (1 << 0)

/* Helper: perform case-insensitive string comparison */
static int stringCompareNocase(const char *a, size_t aLen, const char *b, size_t bLen) {
    size_t minLen = aLen < bLen ? aLen : bLen;

    for (size_t i = 0; i < minLen; i++) {
        char ca = a[i];
        char cb = b[i];

        /* Convert to lowercase */
        if (ca >= 'A' && ca <= 'Z') ca = ca + ('a' - 'A');
        if (cb >= 'A' && cb <= 'Z') cb = cb + ('a' - 'A');

        if (ca != cb) return ca - cb;
    }

    return (int)aLen - (int)bLen;
}

/* Helper: check if element matches pattern */
static int elementMatches(const TclHost *host, TclObj *elem, const char *pattern,
                         size_t patLen, int matchMode, int flags, int contentType) {
    size_t elemLen;
    const char *elemStr = host->getStringPtr(elem, &elemLen);
    int matches = 0;

    if (matchMode == MATCH_EXACT) {
        /* Exact string match */
        if (flags & FLAG_NOCASE) {
            matches = (stringCompareNocase(elemStr, elemLen, pattern, patLen) == 0);
        } else {
            matches = (elemLen == patLen && tclStrncmp(elemStr, pattern, patLen) == 0);
        }
    } else if (matchMode == MATCH_GLOB) {
        /* Glob pattern match using host callback */
        matches = host->stringMatch(pattern, elem, (flags & FLAG_NOCASE) ? 1 : 0);
    } else if (matchMode == MATCH_REGEXP) {
        /* Regular expression match */
        int regexFlags = 0;
        if (flags & FLAG_NOCASE) {
            regexFlags |= REGEX_FLAG_NOCASE;
        }
        TclObj *result = host->regexMatch(pattern, patLen, elem, regexFlags);
        matches = (result != NULL);
    } else if (matchMode == MATCH_SORTED) {
        /* For sorted search, we use binary search with exact match */
        /* This is simplified - just exact match for now */
        if (contentType == TYPE_INTEGER) {
            int64_t elemVal, patVal;
            if (host->asInt(elem, &elemVal) == 0) {
                /* Parse pattern as integer */
                TclObj *patObj = host->newString(pattern, patLen);
                if (host->asInt(patObj, &patVal) == 0) {
                    matches = (elemVal == patVal);
                }
                /* GC will clean up patObj */
            }
        } else {
            /* ASCII comparison */
            if (flags & FLAG_NOCASE) {
                matches = (stringCompareNocase(elemStr, elemLen, pattern, patLen) == 0);
            } else {
                matches = (elemLen == patLen && tclStrncmp(elemStr, pattern, patLen) == 0);
            }
        }
    }

    /* Apply negation if -not flag is set */
    if (flags & FLAG_NOT) {
        matches = !matches;
    }

    return matches;
}

/* Helper: binary search for sorted list */
static int binarySearch(const TclHost *host, TclObj **elems, size_t count,
                       const char *pattern, size_t patLen, int flags, int contentType) {
    if (count == 0) return -1;

    int left = 0;
    int right = (int)count - 1;

    while (left <= right) {
        int mid = (left + right) / 2;
        size_t elemLen;
        const char *elemStr = host->getStringPtr(elems[mid], &elemLen);
        int cmp;

        if (contentType == TYPE_INTEGER) {
            int64_t elemVal, patVal;
            TclObj *patObj = host->newString(pattern, patLen);

            if (host->asInt(elems[mid], &elemVal) == 0 && host->asInt(patObj, &patVal) == 0) {
                if (elemVal < patVal) cmp = -1;
                else if (elemVal > patVal) cmp = 1;
                else cmp = 0;
            } else {
                /* Fallback to string comparison */
                if (flags & FLAG_NOCASE) {
                    cmp = stringCompareNocase(elemStr, elemLen, pattern, patLen);
                } else {
                    size_t minLen = elemLen < patLen ? elemLen : patLen;
                    cmp = tclStrncmp(elemStr, pattern, minLen);
                    if (cmp == 0) {
                        if (elemLen < patLen) cmp = -1;
                        else if (elemLen > patLen) cmp = 1;
                    }
                }
            }
            /* GC will clean up patObj */
        } else {
            /* ASCII comparison */
            if (flags & FLAG_NOCASE) {
                cmp = stringCompareNocase(elemStr, elemLen, pattern, patLen);
            } else {
                size_t minLen = elemLen < patLen ? elemLen : patLen;
                cmp = tclStrncmp(elemStr, pattern, minLen);
                if (cmp == 0) {
                    if (elemLen < patLen) cmp = -1;
                    else if (elemLen > patLen) cmp = 1;
                }
            }
        }

        if (cmp < 0) {
            left = mid + 1;
        } else if (cmp > 0) {
            right = mid - 1;
        } else {
            return mid;
        }
    }

    return -1;
}

/*
 * lsearch ?options? list pattern
 *
 * Searches for pattern in list and returns index (or -1 if not found).
 * Options control matching mode, modifiers, and content type.
 */
TclResult tclCmdLsearch(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"lsearch ?-option value ...? list pattern\"", -1);
        return TCL_ERROR;
    }

    /* Parse options */
    int matchMode = MATCH_GLOB;  /* Default is glob */
    int flags = 0;
    int contentType = TYPE_ASCII;
    int64_t startIdx = 0;
    int argIdx = 1;

    while (argIdx < objc - 2) {
        size_t optLen;
        const char *opt = host->getStringPtr(objv[argIdx], &optLen);

        if (optLen == 0 || opt[0] != '-') break;

        /* Check for -- to end options */
        if (optLen == 2 && opt[1] == '-') {
            argIdx++;
            break;
        }

        /* Matching modes */
        if (optLen == 6 && tclStrncmp(opt, "-exact", 6) == 0) {
            matchMode = MATCH_EXACT;
            argIdx++;
        } else if (optLen == 5 && tclStrncmp(opt, "-glob", 5) == 0) {
            matchMode = MATCH_GLOB;
            argIdx++;
        } else if (optLen == 7 && tclStrncmp(opt, "-regexp", 7) == 0) {
            matchMode = MATCH_REGEXP;
            argIdx++;
        } else if (optLen == 7 && tclStrncmp(opt, "-sorted", 7) == 0) {
            matchMode = MATCH_SORTED;
            argIdx++;
        }
        /* Modifier options */
        else if (optLen == 4 && tclStrncmp(opt, "-all", 4) == 0) {
            flags |= FLAG_ALL;
            argIdx++;
        } else if (optLen == 7 && tclStrncmp(opt, "-inline", 7) == 0) {
            flags |= FLAG_INLINE;
            argIdx++;
        } else if (optLen == 4 && tclStrncmp(opt, "-not", 4) == 0) {
            flags |= FLAG_NOT;
            argIdx++;
        } else if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
            flags |= FLAG_NOCASE;
            argIdx++;
        }
        /* Content type options */
        else if (optLen == 6 && tclStrncmp(opt, "-ascii", 6) == 0) {
            contentType = TYPE_ASCII;
            argIdx++;
        } else if (optLen == 8 && tclStrncmp(opt, "-integer", 8) == 0) {
            contentType = TYPE_INTEGER;
            argIdx++;
        }
        /* -start option */
        else if (optLen == 6 && tclStrncmp(opt, "-start", 6) == 0) {
            argIdx++;
            if (argIdx >= objc - 2) {
                tclSetError(interp, "wrong # args: should be \"lsearch ?-option value ...? list pattern\"", -1);
                return TCL_ERROR;
            }
            if (host->asInt(objv[argIdx], &startIdx) != 0) {
                tclSetError(interp, "expected integer but got invalid value", -1);
                return TCL_ERROR;
            }
            if (startIdx < 0) startIdx = 0;
            argIdx++;
        } else {
            /* Unknown option - stop parsing */
            break;
        }
    }

    /* Must have list and pattern */
    if (objc - argIdx != 2) {
        tclSetError(interp, "wrong # args: should be \"lsearch ?-option value ...? list pattern\"", -1);
        return TCL_ERROR;
    }

    /* Get list and pattern */
    TclObj *listObj = objv[argIdx];
    size_t patLen;
    const char *pattern = host->getStringPtr(objv[argIdx + 1], &patLen);

    /* Parse list */
    TclObj **elems;
    size_t elemCount;
    if (host->asList(listObj, &elems, &elemCount) != 0) {
        tclSetError(interp, "invalid list", -1);
        return TCL_ERROR;
    }

    /* Handle -sorted with binary search */
    if (matchMode == MATCH_SORTED) {
        /* Binary search only works without -all, -not, -start */
        if ((flags & FLAG_ALL) || (flags & FLAG_NOT) || startIdx > 0) {
            /* Fall back to linear search */
            matchMode = MATCH_EXACT;
        } else {
            int idx = binarySearch(host, elems, elemCount, pattern, patLen, flags, contentType);
            if (idx >= 0) {
                if (flags & FLAG_INLINE) {
                    tclSetResult(interp, host->dup(elems[idx]));
                } else {
                    tclSetResult(interp, host->newInt(idx));
                }
            } else {
                if (flags & FLAG_INLINE) {
                    tclSetResult(interp, host->newString("", 0));
                } else {
                    tclSetResult(interp, host->newInt(-1));
                }
            }
            return TCL_OK;
        }
    }

    /* Linear search */
    void *arena = host->arenaPush(interp->hostCtx);
    size_t maxResults = elemCount;
    int *indices = host->arenaAlloc(arena, maxResults * sizeof(int), sizeof(int));
    size_t resultCount = 0;

    for (size_t i = (size_t)startIdx; i < elemCount; i++) {
        if (elementMatches(host, elems[i], pattern, patLen, matchMode, flags, contentType)) {
            indices[resultCount++] = (int)i;
            if (!(flags & FLAG_ALL)) break;  /* Stop after first match if not -all */
        }
    }

    /* Build result based on flags */
    TclObj *result;

    if (flags & FLAG_ALL) {
        if (flags & FLAG_INLINE) {
            /* Return list of matching values */
            TclObj **values = host->arenaAlloc(arena, resultCount * sizeof(TclObj*), sizeof(void*));
            for (size_t i = 0; i < resultCount; i++) {
                values[i] = elems[indices[i]];
            }
            result = host->newList(values, resultCount);
        } else {
            /* Return list of indices */
            TclObj **indexObjs = host->arenaAlloc(arena, resultCount * sizeof(TclObj*), sizeof(void*));
            for (size_t i = 0; i < resultCount; i++) {
                indexObjs[i] = host->newInt(indices[i]);
            }
            result = host->newList(indexObjs, resultCount);
        }
    } else {
        /* Return single result */
        if (resultCount > 0) {
            if (flags & FLAG_INLINE) {
                result = host->dup(elems[indices[0]]);
            } else {
                result = host->newInt(indices[0]);
            }
        } else {
            if (flags & FLAG_INLINE) {
                result = host->newString("", 0);
            } else {
                result = host->newInt(-1);
            }
        }
    }

    host->arenaPop(interp->hostCtx, arena);
    tclSetResult(interp, result);
    return TCL_OK;
}
