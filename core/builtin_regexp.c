/*
 * builtin_regexp.c - TCL regexp and regsub Command Implementation
 *
 * Implements regular expression matching and substitution.
 * Uses host-provided regex functions (glib-2.0).
 */

#include "internal.h"

/* Regex flags for host functions */
#define REGEX_FLAG_NOCASE   (1 << 0)
#define REGEX_FLAG_ALL      (1 << 1)
#define REGEX_FLAG_INDICES  (1 << 2)
#define REGEX_FLAG_INLINE   (1 << 3)

/*
 * regexp ?switches? exp string ?matchVar? ?subMatchVar ...?
 *
 * Returns 1 if match, 0 if no match.
 * With -inline, returns list of matches.
 * With -all, returns count of matches (or concatenated list with -inline).
 */
TclResult tclCmdRegexp(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 3) {
        tclSetError(interp, "wrong # args: should be \"regexp ?switches? exp string ?matchVar? ?subMatchVar ...?\"", -1);
        return TCL_ERROR;
    }

    /* Parse switches */
    int flags = 0;
    int64_t startOffset = 0;
    int argIdx = 1;

    while (argIdx < objc) {
        size_t optLen;
        const char *opt = host->getStringPtr(objv[argIdx], &optLen);

        if (optLen == 0 || opt[0] != '-') break;

        if (optLen == 2 && opt[1] == '-') {
            /* End of switches */
            argIdx++;
            break;
        }

        if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
            flags |= REGEX_FLAG_NOCASE;
            argIdx++;
        } else if (optLen == 4 && tclStrncmp(opt, "-all", 4) == 0) {
            flags |= REGEX_FLAG_ALL;
            argIdx++;
        } else if (optLen == 8 && tclStrncmp(opt, "-indices", 8) == 0) {
            flags |= REGEX_FLAG_INDICES;
            argIdx++;
        } else if (optLen == 7 && tclStrncmp(opt, "-inline", 7) == 0) {
            flags |= REGEX_FLAG_INLINE;
            argIdx++;
        } else if (optLen == 6 && tclStrncmp(opt, "-start", 6) == 0) {
            argIdx++;
            if (argIdx >= objc) {
                tclSetError(interp, "wrong # args: -start requires an argument", -1);
                return TCL_ERROR;
            }
            if (host->asInt(objv[argIdx], &startOffset) != 0) {
                tclSetError(interp, "expected integer but got invalid value", -1);
                return TCL_ERROR;
            }
            argIdx++;
        } else if (optLen == 9 && tclStrncmp(opt, "-expanded", 9) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 5 && tclStrncmp(opt, "-line", 5) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 9 && tclStrncmp(opt, "-linestop", 9) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 11 && tclStrncmp(opt, "-lineanchor", 11) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 6 && tclStrncmp(opt, "-about", 6) == 0) {
            /* Return regex info - simplified */
            tclSetResult(interp, host->newString("0 {}", 4));
            return TCL_OK;
        } else {
            /* Unknown switch - assume it's the pattern */
            break;
        }
    }

    /* Must have at least pattern and string */
    if (objc - argIdx < 2) {
        tclSetError(interp, "wrong # args: should be \"regexp ?switches? exp string ?matchVar? ?subMatchVar ...?\"", -1);
        return TCL_ERROR;
    }

    /* Get pattern and string */
    size_t patLen;
    const char *pattern = host->getStringPtr(objv[argIdx], &patLen);
    TclObj *str = objv[argIdx + 1];
    int varStartIdx = argIdx + 2;
    int numVars = objc - varStartIdx;

    /* Cannot use -inline with match variables */
    if ((flags & REGEX_FLAG_INLINE) && numVars > 0) {
        tclSetError(interp, "regexp match variables not allowed when using -inline", -1);
        return TCL_ERROR;
    }

    /* Handle -start by extracting substring */
    TclObj *searchStr = str;
    if (startOffset > 0) {
        size_t strLen;
        const char *strPtr = host->getStringPtr(str, &strLen);
        if ((size_t)startOffset < strLen) {
            searchStr = host->newString(strPtr + startOffset, strLen - (size_t)startOffset);
        } else {
            /* Start is past end - no match possible */
            if (flags & REGEX_FLAG_INLINE) {
                tclSetResult(interp, host->newList(NULL, 0));
            } else {
                tclSetResult(interp, host->newInt(0));
            }
            return TCL_OK;
        }
    }

    /* Call host regex function */
    TclObj *result = host->regexMatch(pattern, patLen, searchStr, flags);

    if (!result) {
        /* No match - do NOT set match variables (TCL behavior) */
        if (flags & REGEX_FLAG_INLINE) {
            tclSetResult(interp, host->newList(NULL, 0));
        } else {
            tclSetResult(interp, host->newInt(0));
        }
        return TCL_OK;
    }

    /* Match found - handle result based on flags */
    if (flags & REGEX_FLAG_INLINE) {
        /* Return the result directly - host provides list of matches */
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* Set match variables if provided */
    if (numVars > 0) {
        TclObj **elems;
        size_t count;
        if (host->asList(result, &elems, &count) == 0) {
            for (int i = 0; i < numVars && (size_t)i < count; i++) {
                size_t varLen;
                const char *varName = host->getStringPtr(objv[varStartIdx + i], &varLen);
                host->varSet(interp->currentFrame->varsHandle, varName, varLen, elems[i]);
            }
            /* Set remaining vars to empty */
            for (size_t i = count; i < (size_t)numVars; i++) {
                size_t varLen;
                const char *varName = host->getStringPtr(objv[varStartIdx + i], &varLen);
                if (flags & REGEX_FLAG_INDICES) {
                    host->varSet(interp->currentFrame->varsHandle, varName, varLen,
                                host->newString("-1 -1", 5));
                } else {
                    host->varSet(interp->currentFrame->varsHandle, varName, varLen,
                                host->newString("", 0));
                }
            }
        }
    }

    /* Return match count (1 for single match, count for -all) */
    if (flags & REGEX_FLAG_ALL) {
        /* For -all, return the count of matches */
        TclObj **elems;
        size_t count;
        if (host->asList(result, &elems, &count) == 0) {
            tclSetResult(interp, host->newInt((int64_t)count));
        } else {
            tclSetResult(interp, host->newInt(1));
        }
    } else {
        tclSetResult(interp, host->newInt(1));
    }

    return TCL_OK;
}

/*
 * regsub ?switches? exp string subSpec ?varName?
 *
 * Without varName: returns the substituted string
 * With varName: stores result in varName and returns count of substitutions
 */
TclResult tclCmdRegsub(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 4) {
        tclSetError(interp, "wrong # args: should be \"regsub ?switches? exp string subSpec ?varName?\"", -1);
        return TCL_ERROR;
    }

    /* Parse switches */
    int flags = 0;
    int64_t startOffset = 0;
    int argIdx = 1;

    while (argIdx < objc) {
        size_t optLen;
        const char *opt = host->getStringPtr(objv[argIdx], &optLen);

        if (optLen == 0 || opt[0] != '-') break;

        if (optLen == 2 && opt[1] == '-') {
            /* End of switches */
            argIdx++;
            break;
        }

        if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
            flags |= REGEX_FLAG_NOCASE;
            argIdx++;
        } else if (optLen == 4 && tclStrncmp(opt, "-all", 4) == 0) {
            flags |= REGEX_FLAG_ALL;
            argIdx++;
        } else if (optLen == 6 && tclStrncmp(opt, "-start", 6) == 0) {
            argIdx++;
            if (argIdx >= objc) {
                tclSetError(interp, "wrong # args: -start requires an argument", -1);
                return TCL_ERROR;
            }
            if (host->asInt(objv[argIdx], &startOffset) != 0) {
                tclSetError(interp, "expected integer but got invalid value", -1);
                return TCL_ERROR;
            }
            argIdx++;
        } else if (optLen == 9 && tclStrncmp(opt, "-expanded", 9) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 5 && tclStrncmp(opt, "-line", 5) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 9 && tclStrncmp(opt, "-linestop", 9) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 11 && tclStrncmp(opt, "-lineanchor", 11) == 0) {
            /* Ignore for now */
            argIdx++;
        } else if (optLen == 8 && tclStrncmp(opt, "-command", 8) == 0) {
            /* TODO: Implement -command */
            argIdx++;
        } else {
            /* Unknown switch - assume it's the pattern */
            break;
        }
    }

    /* Must have pattern, string, and subspec (optionally varName) */
    int remaining = objc - argIdx;
    if (remaining < 3 || remaining > 4) {
        tclSetError(interp, "wrong # args: should be \"regsub ?switches? exp string subSpec ?varName?\"", -1);
        return TCL_ERROR;
    }

    /* Get pattern, string, and replacement */
    size_t patLen;
    const char *pattern = host->getStringPtr(objv[argIdx], &patLen);
    TclObj *str = objv[argIdx + 1];
    TclObj *replacement = objv[argIdx + 2];
    TclObj *varNameObj = (remaining == 4) ? objv[argIdx + 3] : NULL;

    /* Handle -start by preserving prefix */
    TclObj *prefix = NULL;
    TclObj *searchStr = str;
    if (startOffset > 0) {
        size_t strLen;
        const char *strPtr = host->getStringPtr(str, &strLen);
        if ((size_t)startOffset < strLen) {
            prefix = host->newString(strPtr, (size_t)startOffset);
            searchStr = host->newString(strPtr + startOffset, strLen - (size_t)startOffset);
        }
    }

    /* Call host regex substitution function */
    TclObj *result = host->regexSubst(pattern, patLen, searchStr, replacement, flags);

    if (!result) {
        /* No match - return original string */
        result = str;
    }

    /* Prepend prefix if we had -start offset */
    if (prefix) {
        TclObj *parts[2] = {prefix, result};
        result = host->stringConcat(parts, 2);
    }

    if (varNameObj) {
        /* Store result in variable, return count of replacements */
        size_t varLen;
        const char *varName = host->getStringPtr(varNameObj, &varLen);
        host->varSet(interp->currentFrame->varsHandle, varName, varLen, result);

        /* Count replacements by counting matches in the original search string */
        int countFlags = REGEX_FLAG_ALL;
        if (flags & REGEX_FLAG_NOCASE) {
            countFlags |= REGEX_FLAG_NOCASE;
        }
        TclObj *matchResult = host->regexMatch(pattern, patLen, searchStr, countFlags);
        int64_t matchCount = 0;
        if (matchResult) {
            TclObj **elems;
            size_t count;
            if (host->asList(matchResult, &elems, &count) == 0) {
                matchCount = (int64_t)count;
            }
        }

        /* If not using -all, max count is 1 */
        if (!(flags & REGEX_FLAG_ALL) && matchCount > 1) {
            matchCount = 1;
        }

        tclSetResult(interp, host->newInt(matchCount));
    } else {
        /* Return substituted string */
        tclSetResult(interp, result);
    }

    return TCL_OK;
}
