/*
 * builtin_dict.c - TCL dict Command Implementation
 *
 * Implements all dict subcommands for TCL.
 *
 * Note: TCL dicts are represented as flat lists of alternating key-value pairs.
 * Since the host dict callbacks may be stubs, this implementation works
 * directly with list operations.
 */

#include "internal.h"

/* Helper: find key in dict (list), returns index of value or -1 if not found */
static int dictFindKey(const TclHost *host, TclObj *dict, TclObj *key) {
    TclObj **elems;
    size_t count;

    if (host->asList(dict, &elems, &count) != 0) {
        return -1;
    }

    size_t keyLen;
    const char *keyStr = host->getStringPtr(key, &keyLen);

    for (size_t i = 0; i + 1 < count; i += 2) {
        size_t kLen;
        const char *kStr = host->getStringPtr(elems[i], &kLen);
        if (kLen == keyLen && tclStrncmp(kStr, keyStr, keyLen) == 0) {
            return (int)(i + 1);  /* Return index of value */
        }
    }

    return -1;
}

/* Helper: get value for key, returns NULL if not found */
static TclObj *dictGetValue(const TclHost *host, TclObj *dict, TclObj *key) {
    int idx = dictFindKey(host, dict, key);
    if (idx < 0) {
        return NULL;
    }

    TclObj **elems;
    size_t count;
    if (host->asList(dict, &elems, &count) != 0) {
        return NULL;
    }

    return elems[idx];
}

/* Helper: check if key exists */
static int dictHasKey(const TclHost *host, TclObj *dict, TclObj *key) {
    return dictFindKey(host, dict, key) >= 0;
}

/* Helper: set key-value in dict, returns new dict */
static TclObj *dictSetValue(TclInterp *interp, TclObj *dict, TclObj *key, TclObj *val) {
    const TclHost *host = interp->host;
    TclObj **elems;
    size_t count;

    if (host->asList(dict, &elems, &count) != 0) {
        count = 0;
        elems = NULL;
    }

    /* Check for existing key */
    int keyIdx = dictFindKey(host, dict, key);

    void *arena = host->arenaPush(interp->hostCtx);

    if (keyIdx >= 0) {
        /* Replace existing value */
        size_t newCount = count;
        TclObj **newElems = host->arenaAlloc(arena, newCount * sizeof(TclObj*), sizeof(void*));
        for (size_t i = 0; i < count; i++) {
            if ((int)i == keyIdx) {
                newElems[i] = val;
            } else {
                newElems[i] = elems[i];
            }
        }
        TclObj *result = host->newList(newElems, newCount);
        host->arenaPop(interp->hostCtx, arena);
        return result;
    } else {
        /* Add new key-value pair */
        size_t newCount = count + 2;
        TclObj **newElems = host->arenaAlloc(arena, newCount * sizeof(TclObj*), sizeof(void*));
        for (size_t i = 0; i < count; i++) {
            newElems[i] = elems[i];
        }
        newElems[count] = key;
        newElems[count + 1] = val;
        TclObj *result = host->newList(newElems, newCount);
        host->arenaPop(interp->hostCtx, arena);
        return result;
    }
}

/* Helper: remove key from dict, returns new dict */
static TclObj *dictRemoveKey(TclInterp *interp, TclObj *dict, TclObj *key) {
    const TclHost *host = interp->host;
    TclObj **elems;
    size_t count;

    if (host->asList(dict, &elems, &count) != 0) {
        /* Not a valid list - return empty dict */
        return host->newString("", 0);
    }

    int keyIdx = dictFindKey(host, dict, key);
    if (keyIdx < 0) {
        /* Key not found - return a copy of the original dict */
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **newElems = host->arenaAlloc(arena, count * sizeof(TclObj*), sizeof(void*));
        for (size_t i = 0; i < count; i++) {
            newElems[i] = elems[i];
        }
        TclObj *result = host->newList(newElems, count);
        host->arenaPop(interp->hostCtx, arena);
        return result;
    }

    /* keyIdx is index of value, key is at keyIdx - 1 */
    size_t keyPos = (size_t)(keyIdx - 1);

    void *arena = host->arenaPush(interp->hostCtx);
    size_t newCount = count - 2;
    TclObj **newElems = host->arenaAlloc(arena, newCount * sizeof(TclObj*), sizeof(void*));

    size_t j = 0;
    for (size_t i = 0; i < count; i += 2) {
        if (i != keyPos) {
            newElems[j++] = elems[i];
            newElems[j++] = elems[i + 1];
        }
    }

    TclObj *result = host->newList(newElems, newCount);
    host->arenaPop(interp->hostCtx, arena);
    return result;
}

/* Helper: get list of keys */
static TclObj *dictGetKeys(TclInterp *interp, TclObj *dict, const char *pattern) {
    const TclHost *host = interp->host;
    TclObj **elems;
    size_t count;

    if (host->asList(dict, &elems, &count) != 0 || count == 0) {
        return host->newString("", 0);
    }

    void *arena = host->arenaPush(interp->hostCtx);
    size_t maxKeys = count / 2;
    TclObj **keys = host->arenaAlloc(arena, maxKeys * sizeof(TclObj*), sizeof(void*));
    size_t keyCount = 0;

    for (size_t i = 0; i + 1 < count; i += 2) {
        if (pattern) {
            if (host->stringMatch(pattern, elems[i], 0)) {
                keys[keyCount++] = elems[i];
            }
        } else {
            keys[keyCount++] = elems[i];
        }
    }

    TclObj *result = host->newList(keys, keyCount);
    host->arenaPop(interp->hostCtx, arena);
    return result;
}

/* Helper: get list of values */
static TclObj *dictGetValues(TclInterp *interp, TclObj *dict, const char *pattern) {
    const TclHost *host = interp->host;
    TclObj **elems;
    size_t count;

    if (host->asList(dict, &elems, &count) != 0 || count == 0) {
        return host->newString("", 0);
    }

    void *arena = host->arenaPush(interp->hostCtx);
    size_t maxVals = count / 2;
    TclObj **vals = host->arenaAlloc(arena, maxVals * sizeof(TclObj*), sizeof(void*));
    size_t valCount = 0;

    for (size_t i = 0; i + 1 < count; i += 2) {
        if (pattern) {
            if (host->stringMatch(pattern, elems[i + 1], 0)) {
                vals[valCount++] = elems[i + 1];
            }
        } else {
            vals[valCount++] = elems[i + 1];
        }
    }

    TclObj *result = host->newList(vals, valCount);
    host->arenaPop(interp->hostCtx, arena);
    return result;
}

/* Helper: get dict size (number of key-value pairs) */
static size_t dictGetSize(const TclHost *host, TclObj *dict) {
    TclObj **elems;
    size_t count;

    if (host->asList(dict, &elems, &count) != 0) {
        return 0;
    }

    return count / 2;
}

TclResult tclCmdDict(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"dict subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* ===== dict append ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "append", 6) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"dict append dictVarName key ?string ...?\"", -1);
            return TCL_ERROR;
        }

        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict (or create empty) */
        TclObj *dict = host->varGet(vars, varName, varLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        TclObj *key = objv[3];

        /* Get current value for key (or empty string) */
        TclObj *currentVal = dictGetValue(host, dict, key);
        size_t currentLen = 0;
        const char *currentStr = "";
        if (currentVal) {
            currentStr = host->getStringPtr(currentVal, &currentLen);
        }

        /* Calculate total length */
        size_t totalLen = currentLen;
        for (int i = 4; i < objc; i++) {
            size_t len;
            host->getStringPtr(objv[i], &len);
            totalLen += len;
        }

        /* Build new value */
        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
        char *p = buf;

        for (size_t i = 0; i < currentLen; i++) {
            *p++ = currentStr[i];
        }
        for (int i = 4; i < objc; i++) {
            size_t len;
            const char *s = host->getStringPtr(objv[i], &len);
            for (size_t j = 0; j < len; j++) {
                *p++ = s[j];
            }
        }
        *p = '\0';

        TclObj *newVal = host->newString(buf, totalLen);
        host->arenaPop(interp->hostCtx, arena);

        /* Set in dict */
        dict = dictSetValue(interp, dict, key, newVal);
        host->varSet(vars, varName, varLen, host->dup(dict));

        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict create ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "create", 6) == 0) {
        if ((objc - 2) % 2 != 0) {
            tclSetError(interp, "wrong # args: should be \"dict create ?key value ...?\"", -1);
            return TCL_ERROR;
        }

        if (objc == 2) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Build dict as list, handling duplicate keys (last value wins) */
        void *arena = host->arenaPush(interp->hostCtx);
        int pairCount = (objc - 2) / 2;
        TclObj **keys = host->arenaAlloc(arena, pairCount * sizeof(TclObj*), sizeof(void*));
        TclObj **vals = host->arenaAlloc(arena, pairCount * sizeof(TclObj*), sizeof(void*));
        int finalCount = 0;

        for (int i = 2; i < objc; i += 2) {
            TclObj *key = objv[i];
            TclObj *val = objv[i + 1];

            /* Check if key already exists */
            int found = -1;
            size_t keyLen;
            const char *keyStr = host->getStringPtr(key, &keyLen);
            for (int j = 0; j < finalCount; j++) {
                size_t kLen;
                const char *kStr = host->getStringPtr(keys[j], &kLen);
                if (kLen == keyLen && tclStrncmp(kStr, keyStr, keyLen) == 0) {
                    found = j;
                    break;
                }
            }

            if (found >= 0) {
                /* Update existing key */
                vals[found] = val;
            } else {
                /* Add new key */
                keys[finalCount] = key;
                vals[finalCount] = val;
                finalCount++;
            }
        }

        /* Build final list */
        TclObj **elems = host->arenaAlloc(arena, finalCount * 2 * sizeof(TclObj*), sizeof(void*));
        for (int i = 0; i < finalCount; i++) {
            elems[i * 2] = keys[i];
            elems[i * 2 + 1] = vals[i];
        }

        TclObj *result = host->newList(elems, finalCount * 2);
        host->arenaPop(interp->hostCtx, arena);

        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== dict exists ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "exists", 6) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"dict exists dictionary key ?key ...?\"", -1);
            return TCL_ERROR;
        }

        TclObj *dict = objv[2];

        /* Navigate nested keys */
        for (int i = 3; i < objc; i++) {
            if (!dictHasKey(host, dict, objv[i])) {
                tclSetResult(interp, host->newInt(0));
                return TCL_OK;
            }
            if (i < objc - 1) {
                dict = dictGetValue(host, dict, objv[i]);
                if (!dict) {
                    tclSetResult(interp, host->newInt(0));
                    return TCL_OK;
                }
            }
        }

        tclSetResult(interp, host->newInt(1));
        return TCL_OK;
    }

    /* ===== dict filter ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "filter", 6) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"dict filter dictionary filterType ...\"", -1);
            return TCL_ERROR;
        }

        TclObj *dict = objv[2];
        size_t filterLen;
        const char *filter = host->getStringPtr(objv[3], &filterLen);

        if (filterLen == 3 && tclStrncmp(filter, "key", 3) == 0) {
            /* dict filter dict key pattern ?pattern ...? */
            if (objc < 5) {
                tclSetError(interp, "wrong # args: should be \"dict filter dictionary key ?pattern ...?\"", -1);
                return TCL_ERROR;
            }

            TclObj **elems;
            size_t count;
            if (host->asList(dict, &elems, &count) != 0) {
                count = 0;
            }

            void *arena = host->arenaPush(interp->hostCtx);
            TclObj **result = host->arenaAlloc(arena, count * sizeof(TclObj*), sizeof(void*));
            size_t resultCount = 0;

            for (size_t i = 0; i + 1 < count; i += 2) {
                /* Check if key matches any pattern */
                int matches = 0;
                for (int p = 4; p < objc && !matches; p++) {
                    size_t patLen;
                    const char *pat = host->getStringPtr(objv[p], &patLen);
                    if (host->stringMatch(pat, elems[i], 0)) {
                        matches = 1;
                    }
                }
                if (matches) {
                    result[resultCount++] = elems[i];
                    result[resultCount++] = elems[i + 1];
                }
            }

            TclObj *resultDict = host->newList(result, resultCount);
            host->arenaPop(interp->hostCtx, arena);
            tclSetResult(interp, resultDict);
            return TCL_OK;
        }

        if (filterLen == 5 && tclStrncmp(filter, "value", 5) == 0) {
            /* dict filter dict value pattern */
            if (objc < 5) {
                tclSetError(interp, "wrong # args: should be \"dict filter dictionary value pattern\"", -1);
                return TCL_ERROR;
            }

            size_t patLen;
            const char *pat = host->getStringPtr(objv[4], &patLen);

            TclObj **elems;
            size_t count;
            if (host->asList(dict, &elems, &count) != 0) {
                count = 0;
            }

            void *arena = host->arenaPush(interp->hostCtx);
            TclObj **result = host->arenaAlloc(arena, count * sizeof(TclObj*), sizeof(void*));
            size_t resultCount = 0;

            for (size_t i = 0; i + 1 < count; i += 2) {
                if (host->stringMatch(pat, elems[i + 1], 0)) {
                    result[resultCount++] = elems[i];
                    result[resultCount++] = elems[i + 1];
                }
            }

            TclObj *resultDict = host->newList(result, resultCount);
            host->arenaPop(interp->hostCtx, arena);
            tclSetResult(interp, resultDict);
            return TCL_OK;
        }

        if (filterLen == 6 && tclStrncmp(filter, "script", 6) == 0) {
            /* dict filter dict script {k v} body */
            if (objc != 6) {
                tclSetError(interp, "wrong # args: should be \"dict filter dictionary script {keyVarName valueVarName} script\"", -1);
                return TCL_ERROR;
            }

            /* Parse variable names */
            TclObj **varNames;
            size_t varCount;
            if (host->asList(objv[4], &varNames, &varCount) != 0 || varCount != 2) {
                tclSetError(interp, "must have exactly two variable names", -1);
                return TCL_ERROR;
            }

            size_t keyVarLen, valVarLen;
            const char *keyVar = host->getStringPtr(varNames[0], &keyVarLen);
            const char *valVar = host->getStringPtr(varNames[1], &valVarLen);

            size_t bodyLen;
            const char *body = host->getStringPtr(objv[5], &bodyLen);

            void *vars = interp->currentFrame->varsHandle;

            TclObj **elems;
            size_t count;
            if (host->asList(dict, &elems, &count) != 0) {
                count = 0;
            }

            void *arena = host->arenaPush(interp->hostCtx);
            TclObj **result = host->arenaAlloc(arena, count * sizeof(TclObj*), sizeof(void*));
            size_t resultCount = 0;

            for (size_t i = 0; i + 1 < count; i += 2) {
                host->varSet(vars, keyVar, keyVarLen, host->dup(elems[i]));
                host->varSet(vars, valVar, valVarLen, host->dup(elems[i + 1]));

                TclResult code = tclEvalScript(interp, body, bodyLen);

                if (code == TCL_BREAK) {
                    break;
                }
                if (code == TCL_CONTINUE) {
                    continue;
                }
                if (code == TCL_ERROR || code == TCL_RETURN) {
                    host->arenaPop(interp->hostCtx, arena);
                    return code;
                }

                /* Check if result is true */
                int boolVal = 0;
                if (interp->result && host->asBool(interp->result, &boolVal) == 0 && boolVal) {
                    result[resultCount++] = elems[i];
                    result[resultCount++] = elems[i + 1];
                }
            }

            TclObj *resultDict = host->newList(result, resultCount);
            host->arenaPop(interp->hostCtx, arena);
            tclSetResult(interp, resultDict);
            return TCL_OK;
        }

        tclSetError(interp, "bad filterType: must be key, script, or value", -1);
        return TCL_ERROR;
    }

    /* ===== dict for ===== */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "for", 3) == 0) {
        if (objc != 5) {
            tclSetError(interp, "wrong # args: should be \"dict for {keyVarName valueVarName} dictionary script\"", -1);
            return TCL_ERROR;
        }

        /* Parse variable names */
        TclObj **varNames;
        size_t varCount;
        if (host->asList(objv[2], &varNames, &varCount) != 0 || varCount != 2) {
            tclSetError(interp, "must have exactly two variable names", -1);
            return TCL_ERROR;
        }

        size_t keyVarLen, valVarLen;
        const char *keyVar = host->getStringPtr(varNames[0], &keyVarLen);
        const char *valVar = host->getStringPtr(varNames[1], &valVarLen);

        TclObj *dict = objv[3];
        size_t bodyLen;
        const char *body = host->getStringPtr(objv[4], &bodyLen);

        void *vars = interp->currentFrame->varsHandle;

        TclObj **elems;
        size_t count;
        if (host->asList(dict, &elems, &count) != 0) {
            count = 0;
        }

        for (size_t i = 0; i + 1 < count; i += 2) {
            host->varSet(vars, keyVar, keyVarLen, host->dup(elems[i]));
            host->varSet(vars, valVar, valVarLen, host->dup(elems[i + 1]));

            TclResult code = tclEvalScript(interp, body, bodyLen);

            if (code == TCL_BREAK) {
                break;
            }
            if (code == TCL_CONTINUE) {
                continue;
            }
            if (code == TCL_ERROR || code == TCL_RETURN) {
                return code;
            }
        }

        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* ===== dict get ===== */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "get", 3) == 0) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"dict get dictionary ?key ...?\"", -1);
            return TCL_ERROR;
        }

        TclObj *dict = objv[2];

        /* No keys: return the dict itself */
        if (objc == 3) {
            tclSetResult(interp, host->dup(dict));
            return TCL_OK;
        }

        /* Navigate nested keys */
        for (int i = 3; i < objc; i++) {
            TclObj *val = dictGetValue(host, dict, objv[i]);
            if (!val) {
                /* Build error message */
                void *arena = host->arenaPush(interp->hostCtx);
                size_t keyLen;
                const char *keyStr = host->getStringPtr(objv[i], &keyLen);
                char *errBuf = host->arenaAlloc(arena, keyLen + 50, 1);
                char *ep = errBuf;
                const char *prefix = "key \"";
                while (*prefix) *ep++ = *prefix++;
                for (size_t j = 0; j < keyLen; j++) *ep++ = keyStr[j];
                const char *suffix = "\" not known in dictionary";
                while (*suffix) *ep++ = *suffix++;
                *ep = '\0';
                tclSetError(interp, errBuf, ep - errBuf);
                host->arenaPop(interp->hostCtx, arena);
                return TCL_ERROR;
            }
            dict = val;
        }

        tclSetResult(interp, host->dup(dict));
        return TCL_OK;
    }

    /* ===== dict getdef / dict getwithdefault ===== */
    if ((subcmdLen == 6 && tclStrncmp(subcmd, "getdef", 6) == 0) ||
        (subcmdLen == 14 && tclStrncmp(subcmd, "getwithdefault", 14) == 0)) {
        if (objc < 5) {
            tclSetError(interp, "wrong # args: should be \"dict getdef dictionary ?key ...? key default\"", -1);
            return TCL_ERROR;
        }

        TclObj *dict = objv[2];
        TclObj *defVal = objv[objc - 1];

        /* Navigate nested keys (all but the last one before default) */
        for (int i = 3; i < objc - 1; i++) {
            TclObj *val = dictGetValue(host, dict, objv[i]);
            if (!val) {
                tclSetResult(interp, host->dup(defVal));
                return TCL_OK;
            }
            dict = val;
        }

        tclSetResult(interp, host->dup(dict));
        return TCL_OK;
    }

    /* ===== dict incr ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "incr", 4) == 0) {
        if (objc < 4 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"dict incr dictVarName key ?increment?\"", -1);
            return TCL_ERROR;
        }

        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict (or create empty) */
        TclObj *dict = host->varGet(vars, varName, varLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        TclObj *key = objv[3];

        /* Get increment (default 1) */
        int64_t increment = 1;
        if (objc == 5) {
            if (host->asInt(objv[4], &increment) != 0) {
                tclSetError(interp, "expected integer but got non-integer value", -1);
                return TCL_ERROR;
            }
        }

        /* Get current value (default 0) */
        int64_t currentVal = 0;
        TclObj *current = dictGetValue(host, dict, key);
        if (current) {
            if (host->asInt(current, &currentVal) != 0) {
                /* Build error message */
                void *arena = host->arenaPush(interp->hostCtx);
                size_t valLen;
                const char *valStr = host->getStringPtr(current, &valLen);
                char *errBuf = host->arenaAlloc(arena, valLen + 50, 1);
                char *ep = errBuf;
                const char *prefix = "expected integer but got \"";
                while (*prefix) *ep++ = *prefix++;
                for (size_t j = 0; j < valLen; j++) *ep++ = valStr[j];
                *ep++ = '"';
                *ep = '\0';
                tclSetError(interp, errBuf, ep - errBuf);
                host->arenaPop(interp->hostCtx, arena);
                return TCL_ERROR;
            }
        }

        /* Calculate new value */
        int64_t newVal = currentVal + increment;
        TclObj *newValObj = host->newInt(newVal);

        /* Set in dict */
        dict = dictSetValue(interp, dict, key, newValObj);
        host->varSet(vars, varName, varLen, host->dup(dict));

        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict info ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "info", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"dict info dictionary\"", -1);
            return TCL_ERROR;
        }

        /* Return implementation info (simple placeholder) */
        size_t size = dictGetSize(host, objv[2]);
        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, 100, 1);
        char *p = buf;

        const char *prefix = "Dictionary has ";
        while (*prefix) *p++ = *prefix++;

        /* Simple integer to string */
        char numBuf[20];
        int numLen = 0;
        int64_t n = (int64_t)size;
        if (n == 0) {
            numBuf[numLen++] = '0';
        } else {
            while (n > 0) {
                numBuf[numLen++] = '0' + (n % 10);
                n /= 10;
            }
        }
        for (int i = numLen - 1; i >= 0; i--) {
            *p++ = numBuf[i];
        }

        const char *suffix = " entries";
        while (*suffix) *p++ = *suffix++;
        *p = '\0';

        TclObj *result = host->newString(buf, p - buf);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== dict keys ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "keys", 4) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"dict keys dictionary ?pattern?\"", -1);
            return TCL_ERROR;
        }

        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        TclObj *keys = dictGetKeys(interp, objv[2], pattern);
        tclSetResult(interp, keys);
        return TCL_OK;
    }

    /* ===== dict lappend ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "lappend", 7) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"dict lappend dictVarName key ?value ...?\"", -1);
            return TCL_ERROR;
        }

        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict (or create empty) */
        TclObj *dict = host->varGet(vars, varName, varLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        TclObj *key = objv[3];

        /* Get current value (or empty list) */
        TclObj *current = dictGetValue(host, dict, key);
        if (!current) {
            current = host->newString("", 0);
        }

        /* Append each value */
        TclObj *list = current;
        for (int i = 4; i < objc; i++) {
            list = host->listAppend(list, objv[i]);
        }

        /* Set in dict */
        dict = dictSetValue(interp, dict, key, list);
        host->varSet(vars, varName, varLen, host->dup(dict));

        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict map ===== */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "map", 3) == 0) {
        if (objc != 5) {
            tclSetError(interp, "wrong # args: should be \"dict map {keyVarName valueVarName} dictionary script\"", -1);
            return TCL_ERROR;
        }

        /* Parse variable names */
        TclObj **varNames;
        size_t varCount;
        if (host->asList(objv[2], &varNames, &varCount) != 0 || varCount != 2) {
            tclSetError(interp, "must have exactly two variable names", -1);
            return TCL_ERROR;
        }

        size_t keyVarLen, valVarLen;
        const char *keyVar = host->getStringPtr(varNames[0], &keyVarLen);
        const char *valVar = host->getStringPtr(varNames[1], &valVarLen);

        TclObj *dict = objv[3];
        size_t bodyLen;
        const char *body = host->getStringPtr(objv[4], &bodyLen);

        void *vars = interp->currentFrame->varsHandle;

        TclObj **elems;
        size_t count;
        if (host->asList(dict, &elems, &count) != 0) {
            count = 0;
        }

        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **result = host->arenaAlloc(arena, count * sizeof(TclObj*), sizeof(void*));
        size_t resultCount = 0;

        for (size_t i = 0; i + 1 < count; i += 2) {
            host->varSet(vars, keyVar, keyVarLen, host->dup(elems[i]));
            host->varSet(vars, valVar, valVarLen, host->dup(elems[i + 1]));

            TclResult code = tclEvalScript(interp, body, bodyLen);

            if (code == TCL_BREAK) {
                /* Break returns empty dict */
                host->arenaPop(interp->hostCtx, arena);
                tclSetResult(interp, host->newString("", 0));
                return TCL_OK;
            }
            if (code == TCL_CONTINUE) {
                continue;
            }
            if (code == TCL_ERROR || code == TCL_RETURN) {
                host->arenaPop(interp->hostCtx, arena);
                return code;
            }

            /* Add result to new dict */
            if (interp->result) {
                result[resultCount++] = elems[i];
                result[resultCount++] = interp->result;
            }
        }

        TclObj *resultDict = host->newList(result, resultCount);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, resultDict);
        return TCL_OK;
    }

    /* ===== dict merge ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "merge", 5) == 0) {
        if (objc == 2) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        TclObj *result = host->newString("", 0);

        for (int i = 2; i < objc; i++) {
            TclObj **elems;
            size_t count;
            if (host->asList(objv[i], &elems, &count) != 0) {
                count = 0;
            }

            for (size_t j = 0; j + 1 < count; j += 2) {
                result = dictSetValue(interp, result, elems[j], elems[j + 1]);
            }
        }

        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== dict remove ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "remove", 6) == 0) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"dict remove dictionary ?key ...?\"", -1);
            return TCL_ERROR;
        }

        TclObj *dict = objv[2];

        for (int i = 3; i < objc; i++) {
            dict = dictRemoveKey(interp, dict, objv[i]);
        }

        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict replace ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "replace", 7) == 0) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"dict replace dictionary ?key value ...?\"", -1);
            return TCL_ERROR;
        }

        if ((objc - 3) % 2 != 0) {
            tclSetError(interp, "wrong # args: should be \"dict replace dictionary ?key value ...?\"", -1);
            return TCL_ERROR;
        }

        TclObj *dict = objv[2];

        for (int i = 3; i < objc; i += 2) {
            dict = dictSetValue(interp, dict, objv[i], objv[i + 1]);
        }

        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict set ===== */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "set", 3) == 0) {
        if (objc < 5) {
            tclSetError(interp, "wrong # args: should be \"dict set dictVarName key ?key ...? value\"", -1);
            return TCL_ERROR;
        }

        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict (or create empty) */
        TclObj *dict = host->varGet(vars, varName, varLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        TclObj *value = objv[objc - 1];

        if (objc == 5) {
            /* Simple single-key case */
            dict = dictSetValue(interp, dict, objv[3], value);
        } else {
            /* Nested key path - build from inside out */
            TclObj *innerValue = value;

            for (int i = objc - 2; i >= 3; i--) {
                if (i == 3) {
                    /* Navigate to the parent dict at level objv[3] */
                    dict = dictSetValue(interp, dict, objv[i], innerValue);
                } else {
                    /* Create intermediate wrapper dict */
                    TclObj *wrapper = host->newString("", 0);
                    wrapper = dictSetValue(interp, wrapper, objv[i], innerValue);
                    innerValue = wrapper;
                }
            }
        }

        host->varSet(vars, varName, varLen, host->dup(dict));
        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict size ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "size", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"dict size dictionary\"", -1);
            return TCL_ERROR;
        }

        /* Validate it's a valid dict */
        TclObj **elems;
        size_t elemCount;
        if (host->asList(objv[2], &elems, &elemCount) != 0) {
            tclSetError(interp, "missing value to go with key", -1);
            return TCL_ERROR;
        }
        if (elemCount % 2 != 0) {
            tclSetError(interp, "missing value to go with key", -1);
            return TCL_ERROR;
        }

        size_t size = elemCount / 2;
        tclSetResult(interp, host->newInt((int64_t)size));
        return TCL_OK;
    }

    /* ===== dict unset ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "unset", 5) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"dict unset dictVarName key ?key ...?\"", -1);
            return TCL_ERROR;
        }

        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict */
        TclObj *dict = host->varGet(vars, varName, varLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        if (objc == 4) {
            /* Simple single-key case */
            TclObj *newDict = dictRemoveKey(interp, dict, objv[3]);
            host->varSet(vars, varName, varLen, host->dup(newDict));
            tclSetResult(interp, newDict);
            return TCL_OK;
        } else {
            /* Nested key path - navigate to parent and remove last key */
            TclObj *parent = dict;
            for (int i = 3; i < objc - 1; i++) {
                TclObj *next = dictGetValue(host, parent, objv[i]);
                if (!next) {
                    /* Path doesn't exist, nothing to unset */
                    host->varSet(vars, varName, varLen, host->dup(dict));
                    tclSetResult(interp, host->dup(dict));
                    return TCL_OK;
                }
                parent = next;
            }

            /* Remove the last key from parent */
            TclObj *lastKey = objv[objc - 1];
            TclObj *updatedParent = dictRemoveKey(interp, parent, lastKey);

            /* Rebuild path from inside out */
            TclObj *current = updatedParent;
            for (int i = objc - 2; i >= 3; i--) {
                if (i == 3) {
                    dict = dictSetValue(interp, dict, objv[i], current);
                } else {
                    /* Get the parent at this level */
                    TclObj *levelParent = dict;
                    for (int j = 3; j < i; j++) {
                        levelParent = dictGetValue(host, levelParent, objv[j]);
                    }
                    current = dictSetValue(interp, levelParent, objv[i], current);
                }
            }
        }

        host->varSet(vars, varName, varLen, host->dup(dict));
        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict update ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "update", 6) == 0) {
        /* dict update dictVarName key varName ?key varName ...? body */
        /* objc must be at least 6: dict update var key varName body */
        /* (objc - 4) must be even for complete key-varName pairs */
        if (objc < 6 || (objc - 4) % 2 != 0) {
            tclSetError(interp, "wrong # args: should be \"dict update dictVarName key varName ?key varName ...? body\"", -1);
            return TCL_ERROR;
        }

        size_t dictVarLen;
        const char *dictVarName = host->getStringPtr(objv[2], &dictVarLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict */
        TclObj *dict = host->varGet(vars, dictVarName, dictVarLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        size_t bodyLen;
        const char *body = host->getStringPtr(objv[objc - 1], &bodyLen);

        /* Number of key-var pairs: (objc - 4) / 2 */
        /* objv[3..objc-2] are key-varName pairs, objv[objc-1] is body */
        int pairCount = (objc - 4) / 2;

        /* Set local variables from dict */
        for (int i = 0; i < pairCount; i++) {
            TclObj *key = objv[3 + i * 2];
            size_t localVarLen;
            const char *localVarName = host->getStringPtr(objv[4 + i * 2], &localVarLen);

            TclObj *val = dictGetValue(host, dict, key);
            if (val) {
                host->varSet(vars, localVarName, localVarLen, host->dup(val));
            }
            /* If key doesn't exist, don't set the variable (it won't exist) */
        }

        /* Execute body */
        TclResult code = tclEvalScript(interp, body, bodyLen);

        /* Read back local variables into dict */
        for (int i = 0; i < pairCount; i++) {
            TclObj *key = objv[3 + i * 2];
            size_t localVarLen;
            const char *localVarName = host->getStringPtr(objv[4 + i * 2], &localVarLen);

            if (host->varExists(vars, localVarName, localVarLen)) {
                TclObj *val = host->varGet(vars, localVarName, localVarLen);
                dict = dictSetValue(interp, dict, key, val);
            } else {
                /* Variable was unset, remove key from dict */
                dict = dictRemoveKey(interp, dict, key);
            }
        }

        host->varSet(vars, dictVarName, dictVarLen, host->dup(dict));

        if (code == TCL_ERROR || code == TCL_RETURN) {
            return code;
        }

        tclSetResult(interp, dict);
        return TCL_OK;
    }

    /* ===== dict values ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "values", 6) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"dict values dictionary ?pattern?\"", -1);
            return TCL_ERROR;
        }

        const char *pattern = NULL;
        if (objc == 4) {
            size_t patLen;
            pattern = host->getStringPtr(objv[3], &patLen);
        }

        TclObj *values = dictGetValues(interp, objv[2], pattern);
        tclSetResult(interp, values);
        return TCL_OK;
    }

    /* ===== dict with ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "with", 4) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"dict with dictVarName ?key ...? body\"", -1);
            return TCL_ERROR;
        }

        size_t dictVarLen;
        const char *dictVarName = host->getStringPtr(objv[2], &dictVarLen);
        void *vars = interp->currentFrame->varsHandle;

        /* Get current dict, navigating nested keys if present */
        TclObj *dict = host->varGet(vars, dictVarName, dictVarLen);
        if (!dict) {
            dict = host->newString("", 0);
        }

        TclObj *rootDict = dict;

        /* Navigate nested keys (all but the last arg which is the body) */
        for (int i = 3; i < objc - 1; i++) {
            TclObj *next = dictGetValue(host, dict, objv[i]);
            if (!next) {
                next = host->newString("", 0);
            }
            dict = next;
        }

        size_t bodyLen;
        const char *body = host->getStringPtr(objv[objc - 1], &bodyLen);

        /* Get all keys and set as local variables */
        TclObj **elems;
        size_t count;
        if (host->asList(dict, &elems, &count) != 0) {
            count = 0;
        }

        for (size_t i = 0; i + 1 < count; i += 2) {
            size_t keyLen;
            const char *keyName = host->getStringPtr(elems[i], &keyLen);
            host->varSet(vars, keyName, keyLen, host->dup(elems[i + 1]));
        }

        /* Execute body */
        TclResult code = tclEvalScript(interp, body, bodyLen);

        /* Read back local variables into dict */
        TclObj *newDict = host->newString("", 0);
        for (size_t i = 0; i + 1 < count; i += 2) {
            size_t keyLen;
            const char *keyName = host->getStringPtr(elems[i], &keyLen);
            if (host->varExists(vars, keyName, keyLen)) {
                TclObj *val = host->varGet(vars, keyName, keyLen);
                newDict = dictSetValue(interp, newDict, elems[i], val);
            }
            /* If variable was unset, don't add to new dict */
        }

        /* Store back through the nested path if any */
        if (objc == 4) {
            /* No nested keys */
            host->varSet(vars, dictVarName, dictVarLen, host->dup(newDict));
        } else {
            /* Need to rebuild nested path */
            TclObj *current = newDict;
            for (int i = objc - 2; i >= 3; i--) {
                if (i == 3) {
                    rootDict = dictSetValue(interp, rootDict, objv[i], current);
                } else {
                    TclObj *levelParent = rootDict;
                    for (int j = 3; j < i; j++) {
                        levelParent = dictGetValue(host, levelParent, objv[j]);
                        if (!levelParent) {
                            levelParent = host->newString("", 0);
                        }
                    }
                    current = dictSetValue(interp, levelParent, objv[i], current);
                }
            }
            host->varSet(vars, dictVarName, dictVarLen, host->dup(rootDict));
        }

        if (code == TCL_ERROR || code == TCL_RETURN) {
            return code;
        }

        tclSetResult(interp, interp->result ? interp->result : host->newString("", 0));
        return TCL_OK;
    }

    /* Unknown subcommand */
    tclSetError(interp, "unknown or ambiguous subcommand \"unknown\": must be append, create, exists, filter, for, get, getdef, getwithdefault, incr, info, keys, lappend, map, merge, remove, replace, set, size, unset, update, values, or with", -1);
    return TCL_ERROR;
}
