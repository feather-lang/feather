/*
 * builtin_string.c - TCL string Command Implementation
 *
 * Implements all string subcommands for TCL.
 */

#include "internal.h"

/* Helper: parse index specification (integer, end, end-N, end+N, M+N, M-N) */
static int parseIndex(const TclHost *host, TclObj *indexObj, size_t strLen, size_t *out) {
    size_t idxLen;
    const char *idxStr = host->getStringPtr(indexObj, &idxLen);

    /* Check for "end" prefix */
    if (idxLen >= 3 && tclStrncmp(idxStr, "end", 3) == 0) {
        if (idxLen == 3) {
            *out = strLen > 0 ? strLen - 1 : 0;
            return 0;
        }
        if (idxLen > 3 && idxStr[3] == '-') {
            /* end-N */
            int64_t offset = 0;
            size_t i = 4;
            while (i < idxLen && idxStr[i] >= '0' && idxStr[i] <= '9') {
                offset = offset * 10 + (idxStr[i] - '0');
                i++;
            }
            if (i != idxLen) return -1; /* Invalid format */
            if ((size_t)offset >= strLen) {
                *out = (size_t)-1; /* Before start */
            } else {
                *out = strLen - 1 - (size_t)offset;
            }
            return 0;
        }
        if (idxLen > 3 && idxStr[3] == '+') {
            /* end+N */
            int64_t offset = 0;
            size_t i = 4;
            while (i < idxLen && idxStr[i] >= '0' && idxStr[i] <= '9') {
                offset = offset * 10 + (idxStr[i] - '0');
                i++;
            }
            if (i != idxLen) return -1;
            *out = (strLen > 0 ? strLen - 1 : 0) + (size_t)offset;
            return 0;
        }
        return -1;
    }

    /* Try as plain integer */
    int64_t val;
    if (host->asInt(indexObj, &val) == 0) {
        if (val < 0) {
            *out = (size_t)-1; /* Negative index means before start */
        } else {
            *out = (size_t)val;
        }
        return 0;
    }

    return -1; /* Could not parse */
}

/* Helper: check if character is whitespace */
static int isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' || c == '\0';
}

/* Helper: check if character is in chars set */
static int charInSet(char c, const char *chars, size_t charsLen) {
    for (size_t i = 0; i < charsLen; i++) {
        if (chars[i] == c) return 1;
    }
    return 0;
}

/* Helper: convert char to lowercase (ASCII only) */
static char toLowerChar(char c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/* Helper: convert char to uppercase (ASCII only) */
static char toUpperChar(char c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

TclResult tclCmdString(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"string subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* ===== string cat ===== */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "cat", 3) == 0) {
        if (objc == 2) {
            tclSetResult(interp, host->newString("", 0));
            return TCL_OK;
        }

        /* Calculate total length */
        size_t totalLen = 0;
        for (int i = 2; i < objc; i++) {
            size_t len;
            host->getStringPtr(objv[i], &len);
            totalLen += len;
        }

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, totalLen + 1, 1);
        char *p = buf;

        for (int i = 2; i < objc; i++) {
            size_t len;
            const char *s = host->getStringPtr(objv[i], &len);
            for (size_t j = 0; j < len; j++) {
                *p++ = s[j];
            }
        }
        *p = '\0';

        TclObj *result = host->newString(buf, totalLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string compare ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "compare", 7) == 0) {
        int nocase = 0;
        int64_t length = -1;
        int argIdx = 2;

        /* Parse options */
        while (argIdx < objc - 2) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen >= 2 && opt[0] == '-') {
                if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
                    nocase = 1;
                    argIdx++;
                } else if (optLen == 7 && tclStrncmp(opt, "-length", 7) == 0) {
                    argIdx++;
                    if (argIdx >= objc - 2) {
                        tclSetError(interp, "wrong # args: should be \"string compare ?-nocase? ?-length length? string1 string2\"", -1);
                        return TCL_ERROR;
                    }
                    if (host->asInt(objv[argIdx], &length) != 0) {
                        tclSetError(interp, "expected integer but got invalid value", -1);
                        return TCL_ERROR;
                    }
                    argIdx++;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        if (objc - argIdx != 2) {
            tclSetError(interp, "wrong # args: should be \"string compare ?-nocase? ?-length length? string1 string2\"", -1);
            return TCL_ERROR;
        }

        size_t len1, len2;
        const char *s1 = host->getStringPtr(objv[argIdx], &len1);
        const char *s2 = host->getStringPtr(objv[argIdx + 1], &len2);

        size_t cmpLen1 = len1;
        size_t cmpLen2 = len2;
        if (length >= 0) {
            if ((size_t)length < cmpLen1) cmpLen1 = (size_t)length;
            if ((size_t)length < cmpLen2) cmpLen2 = (size_t)length;
        }

        int result = 0;
        size_t minLen = cmpLen1 < cmpLen2 ? cmpLen1 : cmpLen2;
        for (size_t i = 0; i < minLen; i++) {
            char c1 = nocase ? toLowerChar(s1[i]) : s1[i];
            char c2 = nocase ? toLowerChar(s2[i]) : s2[i];
            if (c1 < c2) { result = -1; break; }
            if (c1 > c2) { result = 1; break; }
        }
        if (result == 0) {
            if (cmpLen1 < cmpLen2) result = -1;
            else if (cmpLen1 > cmpLen2) result = 1;
        }

        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== string equal ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "equal", 5) == 0) {
        int nocase = 0;
        int64_t length = -1;
        int argIdx = 2;

        while (argIdx < objc - 2) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen >= 2 && opt[0] == '-') {
                if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
                    nocase = 1;
                    argIdx++;
                } else if (optLen == 7 && tclStrncmp(opt, "-length", 7) == 0) {
                    argIdx++;
                    if (argIdx >= objc - 2) {
                        tclSetError(interp, "wrong # args: should be \"string equal ?-nocase? ?-length length? string1 string2\"", -1);
                        return TCL_ERROR;
                    }
                    if (host->asInt(objv[argIdx], &length) != 0) {
                        tclSetError(interp, "expected integer but got invalid value", -1);
                        return TCL_ERROR;
                    }
                    argIdx++;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        if (objc - argIdx != 2) {
            tclSetError(interp, "wrong # args: should be \"string equal ?-nocase? ?-length length? string1 string2\"", -1);
            return TCL_ERROR;
        }

        size_t len1, len2;
        const char *s1 = host->getStringPtr(objv[argIdx], &len1);
        const char *s2 = host->getStringPtr(objv[argIdx + 1], &len2);

        if (length >= 0) {
            if ((size_t)length < len1) len1 = (size_t)length;
            if ((size_t)length < len2) len2 = (size_t)length;
        }

        int equal = 1;
        if (len1 != len2) {
            equal = 0;
        } else {
            for (size_t i = 0; i < len1; i++) {
                char c1 = nocase ? toLowerChar(s1[i]) : s1[i];
                char c2 = nocase ? toLowerChar(s2[i]) : s2[i];
                if (c1 != c2) { equal = 0; break; }
            }
        }

        tclSetResult(interp, host->newInt(equal));
        return TCL_OK;
    }

    /* ===== string first ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "first", 5) == 0) {
        if (objc < 4 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"string first needleString haystackString ?startIndex?\"", -1);
            return TCL_ERROR;
        }

        size_t needleLen, haystackLen;
        const char *needle = host->getStringPtr(objv[2], &needleLen);
        const char *haystack = host->getStringPtr(objv[3], &haystackLen);

        size_t startIdx = 0;
        if (objc == 5) {
            if (parseIndex(host, objv[4], haystackLen, &startIdx) != 0) {
                int64_t val;
                if (host->asInt(objv[4], &val) == 0) {
                    startIdx = val < 0 ? 0 : (size_t)val;
                } else {
                    tclSetError(interp, "bad index", -1);
                    return TCL_ERROR;
                }
            }
        }

        /* Empty needle returns -1 */
        if (needleLen == 0) {
            tclSetResult(interp, host->newInt(-1));
            return TCL_OK;
        }

        int64_t foundIdx = -1;
        for (size_t i = startIdx; i + needleLen <= haystackLen; i++) {
            int match = 1;
            for (size_t j = 0; j < needleLen; j++) {
                if (haystack[i + j] != needle[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                foundIdx = (int64_t)i;
                break;
            }
        }

        tclSetResult(interp, host->newInt(foundIdx));
        return TCL_OK;
    }

    /* ===== string index ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "index", 5) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"string index string charIndex\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t idx;
        if (parseIndex(host, objv[3], strLen, &idx) != 0) {
            tclSetError(interp, "bad index", -1);
            return TCL_ERROR;
        }

        if (idx >= strLen || idx == (size_t)-1) {
            tclSetResult(interp, host->newString("", 0));
        } else {
            tclSetResult(interp, host->newString(&str[idx], 1));
        }
        return TCL_OK;
    }

    /* ===== string insert ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "insert", 6) == 0) {
        if (objc != 5) {
            tclSetError(interp, "wrong # args: should be \"string insert string index insertString\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t insertLen;
        const char *insertStr = host->getStringPtr(objv[4], &insertLen);

        size_t idx;
        /* Check for "end" index specially */
        size_t idxObjLen;
        const char *idxStr = host->getStringPtr(objv[3], &idxObjLen);

        int isEnd = (idxObjLen == 3 && tclStrncmp(idxStr, "end", 3) == 0);
        int isEndMinus = (idxObjLen > 4 && tclStrncmp(idxStr, "end-", 4) == 0);

        if (isEnd) {
            /* end means after last character */
            idx = strLen;
        } else if (isEndMinus) {
            /* end-N: insert before the character at that position */
            int64_t offset = 0;
            size_t i = 4;
            while (i < idxObjLen && idxStr[i] >= '0' && idxStr[i] <= '9') {
                offset = offset * 10 + (idxStr[i] - '0');
                i++;
            }
            if (strLen == 0) {
                idx = 0;
            } else if ((size_t)offset >= strLen) {
                idx = 0;
            } else {
                idx = strLen - (size_t)offset;
            }
        } else {
            int64_t val;
            if (host->asInt(objv[3], &val) == 0) {
                if (val < 0) {
                    idx = 0;
                } else if ((size_t)val > strLen) {
                    idx = strLen;
                } else {
                    idx = (size_t)val;
                }
            } else {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }

        void *arena = host->arenaPush(interp->hostCtx);
        size_t resultLen = strLen + insertLen;
        char *buf = host->arenaAlloc(arena, resultLen + 1, 1);

        /* Copy: prefix + insert + suffix */
        size_t p = 0;
        for (size_t i = 0; i < idx && i < strLen; i++) buf[p++] = str[i];
        for (size_t i = 0; i < insertLen; i++) buf[p++] = insertStr[i];
        for (size_t i = idx; i < strLen; i++) buf[p++] = str[i];
        buf[p] = '\0';

        TclObj *result = host->newString(buf, p);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string is ===== */
    if (subcmdLen == 2 && tclStrncmp(subcmd, "is", 2) == 0) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"string is class ?-strict? ?-failindex varname? string\"", -1);
            return TCL_ERROR;
        }

        size_t classLen;
        const char *class = host->getStringPtr(objv[2], &classLen);

        int strict = 0;
        int argIdx = 3;

        /* Parse options */
        while (argIdx < objc - 1) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 7 && tclStrncmp(opt, "-strict", 7) == 0) {
                strict = 1;
                argIdx++;
            } else if (optLen == 10 && tclStrncmp(opt, "-failindex", 10) == 0) {
                /* Skip -failindex and varname for now */
                argIdx += 2;
            } else {
                break;
            }
        }

        if (argIdx >= objc) {
            tclSetError(interp, "wrong # args: should be \"string is class ?-strict? ?-failindex varname? string\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[argIdx], &strLen);

        int result = 1;

        /* Empty string handling */
        if (strLen == 0) {
            result = strict ? 0 : 1;
            tclSetResult(interp, host->newInt(result));
            return TCL_OK;
        }

        /* Check class */
        if ((classLen == 7 && tclStrncmp(class, "integer", 7) == 0) ||
            (classLen == 6 && tclStrncmp(class, "entier", 6) == 0) ||
            (classLen == 11 && tclStrncmp(class, "wideinteger", 11) == 0)) {
            size_t i = 0;
            if (str[0] == '-' || str[0] == '+') i++;
            if (i >= strLen) result = 0;
            for (; i < strLen && result; i++) {
                if (str[i] < '0' || str[i] > '9') result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "alpha", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "alnum", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "digit", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                if (str[i] < '0' || str[i] > '9') result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "space", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                if (!isWhitespace(str[i])) result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "upper", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                if (c >= 'a' && c <= 'z') result = 0;
                /* Non-letters are OK in upper class */
            }
        } else if (classLen == 5 && tclStrncmp(class, "lower", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                if (c >= 'A' && c <= 'Z') result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "ascii", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                if ((unsigned char)str[i] >= 128) result = 0;
            }
        } else if (classLen == 6 && tclStrncmp(class, "xdigit", 6) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) result = 0;
            }
        } else if (classLen == 7 && tclStrncmp(class, "boolean", 7) == 0) {
            int boolVal;
            result = (host->asBool(objv[argIdx], &boolVal) == 0) ? 1 : 0;
        } else if (classLen == 4 && tclStrncmp(class, "true", 4) == 0) {
            int boolVal;
            result = (host->asBool(objv[argIdx], &boolVal) == 0 && boolVal) ? 1 : 0;
        } else if (classLen == 5 && tclStrncmp(class, "false", 5) == 0) {
            int boolVal;
            result = (host->asBool(objv[argIdx], &boolVal) == 0 && !boolVal) ? 1 : 0;
        } else if (classLen == 6 && tclStrncmp(class, "double", 6) == 0) {
            double dblVal;
            result = (host->asDouble(objv[argIdx], &dblVal) == 0) ? 1 : 0;
        } else if (classLen == 4 && tclStrncmp(class, "list", 4) == 0) {
            TclObj **elems;
            size_t count;
            result = (host->asList(objv[argIdx], &elems, &count) == 0) ? 1 : 0;
        } else if (classLen == 5 && tclStrncmp(class, "print", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                unsigned char c = (unsigned char)str[i];
                /* Control chars (0-31 except tab, newline, etc.) and DEL are not printable */
                if (c < 32 && c != '\t' && c != '\n' && c != '\r') result = 0;
                if (c == 127) result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "graph", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                unsigned char c = (unsigned char)str[i];
                if (c <= 32 || c == 127) result = 0;
            }
        } else if (classLen == 7 && tclStrncmp(class, "control", 7) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                unsigned char c = (unsigned char)str[i];
                if (c >= 32 && c != 127) result = 0;
            }
        } else if (classLen == 5 && tclStrncmp(class, "punct", 5) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                int isPunct = (c >= '!' && c <= '/') || (c >= ':' && c <= '@') ||
                              (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
                if (!isPunct) result = 0;
            }
        } else if (classLen == 8 && tclStrncmp(class, "wordchar", 8) == 0) {
            for (size_t i = 0; i < strLen && result; i++) {
                char c = str[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_')) result = 0;
            }
        } else {
            /* Unknown class - return error */
            tclSetError(interp, "bad class: must be alnum, alpha, ascii, boolean, control, digit, double, entier, false, graph, integer, list, lower, print, punct, space, true, upper, wideinteger, wordchar, or xdigit", -1);
            return TCL_ERROR;
        }

        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== string last ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "last", 4) == 0) {
        if (objc < 4 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"string last needleString haystackString ?lastIndex?\"", -1);
            return TCL_ERROR;
        }

        size_t needleLen, haystackLen;
        const char *needle = host->getStringPtr(objv[2], &needleLen);
        const char *haystack = host->getStringPtr(objv[3], &haystackLen);

        size_t lastIdx = haystackLen;
        if (objc == 5) {
            if (parseIndex(host, objv[4], haystackLen, &lastIdx) != 0) {
                int64_t val;
                if (host->asInt(objv[4], &val) == 0) {
                    lastIdx = val < 0 ? 0 : (size_t)val;
                } else {
                    tclSetError(interp, "bad index", -1);
                    return TCL_ERROR;
                }
            }
        }

        /* Empty needle returns -1 */
        if (needleLen == 0) {
            tclSetResult(interp, host->newInt(-1));
            return TCL_OK;
        }

        int64_t foundIdx = -1;
        /* Search backwards from lastIdx */
        size_t searchEnd = lastIdx;
        if (searchEnd + needleLen > haystackLen) {
            searchEnd = haystackLen >= needleLen ? haystackLen - needleLen : 0;
        }

        for (size_t i = 0; i <= searchEnd; i++) {
            size_t pos = searchEnd - i;
            int match = 1;
            for (size_t j = 0; j < needleLen; j++) {
                if (haystack[pos + j] != needle[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                foundIdx = (int64_t)pos;
                break;
            }
        }

        tclSetResult(interp, host->newInt(foundIdx));
        return TCL_OK;
    }

    /* ===== string length ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "length", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"string length string\"", -1);
            return TCL_ERROR;
        }
        size_t len;
        host->getStringPtr(objv[2], &len);
        tclSetResult(interp, host->newInt((int64_t)len));
        return TCL_OK;
    }

    /* ===== string map ===== */
    if (subcmdLen == 3 && tclStrncmp(subcmd, "map", 3) == 0) {
        int nocase = 0;
        int argIdx = 2;

        if (argIdx < objc) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
                nocase = 1;
                argIdx++;
            }
        }

        if (objc - argIdx != 2) {
            tclSetError(interp, "wrong # args: should be \"string map ?-nocase? mapping string\"", -1);
            return TCL_ERROR;
        }

        TclObj **mapElems;
        size_t mapCount;
        if (host->asList(objv[argIdx], &mapElems, &mapCount) != 0 || mapCount % 2 != 0) {
            tclSetError(interp, "list must have an even number of elements", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[argIdx + 1], &strLen);

        void *arena = host->arenaPush(interp->hostCtx);
        /* Worst case: each key->value mapping could expand string */
        char *buf = host->arenaAlloc(arena, strLen * 10 + 1, 1);
        size_t outLen = 0;

        size_t i = 0;
        while (i < strLen) {
            int matched = 0;

            /* Try each key in order */
            for (size_t k = 0; k < mapCount; k += 2) {
                size_t keyLen;
                const char *key = host->getStringPtr(mapElems[k], &keyLen);

                if (keyLen == 0 || i + keyLen > strLen) continue;

                int match = 1;
                for (size_t j = 0; j < keyLen && match; j++) {
                    char c1 = nocase ? toLowerChar(str[i + j]) : str[i + j];
                    char c2 = nocase ? toLowerChar(key[j]) : key[j];
                    if (c1 != c2) match = 0;
                }

                if (match) {
                    /* Replace with value */
                    size_t valLen;
                    const char *val = host->getStringPtr(mapElems[k + 1], &valLen);
                    for (size_t j = 0; j < valLen; j++) {
                        buf[outLen++] = val[j];
                    }
                    i += keyLen;
                    matched = 1;
                    break;
                }
            }

            if (!matched) {
                buf[outLen++] = str[i++];
            }
        }
        buf[outLen] = '\0';

        TclObj *result = host->newString(buf, outLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string match ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "match", 5) == 0) {
        int nocase = 0;
        int argIdx = 2;

        if (argIdx < objc) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 7 && tclStrncmp(opt, "-nocase", 7) == 0) {
                nocase = 1;
                argIdx++;
            }
        }

        if (objc - argIdx != 2) {
            tclSetError(interp, "wrong # args: should be \"string match ?-nocase? pattern string\"", -1);
            return TCL_ERROR;
        }

        size_t patLen;
        const char *pattern = host->getStringPtr(objv[argIdx], &patLen);

        int result = host->stringMatch(pattern, objv[argIdx + 1], nocase);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== string range ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "range", 5) == 0) {
        if (objc != 5) {
            tclSetError(interp, "wrong # args: should be \"string range string first last\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t first, last;
        if (parseIndex(host, objv[3], strLen, &first) != 0) {
            int64_t val;
            if (host->asInt(objv[3], &val) == 0) {
                first = val < 0 ? 0 : (size_t)val;
            } else {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }
        if (parseIndex(host, objv[4], strLen, &last) != 0) {
            int64_t val;
            if (host->asInt(objv[4], &val) == 0) {
                last = val < 0 ? 0 : (size_t)val;
            } else {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }

        /* Clamp indices */
        if (first == (size_t)-1) first = 0;
        if (first < 0) first = 0;
        if (last >= strLen && strLen > 0) last = strLen - 1;

        if (first > last || first >= strLen) {
            tclSetResult(interp, host->newString("", 0));
        } else {
            tclSetResult(interp, host->newString(&str[first], last - first + 1));
        }
        return TCL_OK;
    }

    /* ===== string repeat ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "repeat", 6) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"string repeat string count\"", -1);
            return TCL_ERROR;
        }
        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        int64_t count;
        if (host->asInt(objv[3], &count) != 0 || count < 0) {
            tclSetError(interp, "expected integer but got invalid value", -1);
            return TCL_ERROR;
        }

        size_t resultLen = strLen * (size_t)count;
        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, resultLen + 1, 1);

        char *p = buf;
        for (int64_t i = 0; i < count; i++) {
            for (size_t j = 0; j < strLen; j++) {
                *p++ = str[j];
            }
        }
        *p = '\0';

        TclObj *result = host->newString(buf, resultLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string replace ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "replace", 7) == 0) {
        if (objc < 5 || objc > 6) {
            tclSetError(interp, "wrong # args: should be \"string replace string first last ?newstring?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        int64_t firstVal = 0, lastVal = 0;
        size_t first, last;

        /* Parse first index - try as integer first to handle negative clamping */
        if (host->asInt(objv[3], &firstVal) == 0) {
            first = firstVal < 0 ? 0 : (size_t)firstVal;
        } else if (parseIndex(host, objv[3], strLen, &first) == 0) {
            /* end-relative index */
            if (first == (size_t)-1) first = 0;
        } else {
            tclSetError(interp, "bad index", -1);
            return TCL_ERROR;
        }

        /* Parse last index - keep track of negative values */
        if (host->asInt(objv[4], &lastVal) == 0) {
            last = lastVal < 0 ? (size_t)-1 : (size_t)lastVal;
        } else if (parseIndex(host, objv[4], strLen, &last) == 0) {
            lastVal = (int64_t)last;
        } else {
            tclSetError(interp, "bad index", -1);
            return TCL_ERROR;
        }

        const char *newStr = "";
        size_t newLen = 0;
        if (objc == 6) {
            newStr = host->getStringPtr(objv[5], &newLen);
        }

        /* If first > last or first >= strlen or last < 0, return original */
        if (first > last || first >= strLen || lastVal < 0) {
            tclSetResult(interp, host->dup(objv[2]));
            return TCL_OK;
        }

        /* Clamp last if beyond string length */
        if (last >= strLen) last = strLen - 1;

        void *arena = host->arenaPush(interp->hostCtx);
        size_t resultLen = first + newLen + (strLen - last - 1);
        char *buf = host->arenaAlloc(arena, resultLen + 1, 1);

        size_t p = 0;
        for (size_t i = 0; i < first; i++) buf[p++] = str[i];
        for (size_t i = 0; i < newLen; i++) buf[p++] = newStr[i];
        for (size_t i = last + 1; i < strLen; i++) buf[p++] = str[i];
        buf[p] = '\0';

        TclObj *result = host->newString(buf, p);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string reverse ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "reverse", 7) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"string reverse string\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, strLen + 1, 1);

        for (size_t i = 0; i < strLen; i++) {
            buf[i] = str[strLen - 1 - i];
        }
        buf[strLen] = '\0';

        TclObj *result = host->newString(buf, strLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string tolower ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "tolower", 7) == 0) {
        if (objc < 3 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"string tolower string ?first? ?last?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t first = 0, last = strLen > 0 ? strLen - 1 : 0;

        if (objc >= 4) {
            if (parseIndex(host, objv[3], strLen, &first) != 0) {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }
        if (objc == 5) {
            if (parseIndex(host, objv[4], strLen, &last) != 0) {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, strLen + 1, 1);

        for (size_t i = 0; i < strLen; i++) {
            if (i >= first && i <= last) {
                buf[i] = toLowerChar(str[i]);
            } else {
                buf[i] = str[i];
            }
        }
        buf[strLen] = '\0';

        TclObj *result = host->newString(buf, strLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string totitle ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "totitle", 7) == 0) {
        if (objc < 3 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"string totitle string ?first? ?last?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t first = 0, last = strLen > 0 ? strLen - 1 : 0;

        if (objc >= 4) {
            if (parseIndex(host, objv[3], strLen, &first) != 0) {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }
        if (objc == 5) {
            if (parseIndex(host, objv[4], strLen, &last) != 0) {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, strLen + 1, 1);

        for (size_t i = 0; i < strLen; i++) {
            if (i == first) {
                buf[i] = toUpperChar(str[i]);
            } else if (i > first && i <= last) {
                buf[i] = toLowerChar(str[i]);
            } else {
                buf[i] = str[i];
            }
        }
        buf[strLen] = '\0';

        TclObj *result = host->newString(buf, strLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string toupper ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "toupper", 7) == 0) {
        if (objc < 3 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"string toupper string ?first? ?last?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t first = 0, last = strLen > 0 ? strLen - 1 : 0;

        if (objc >= 4) {
            if (parseIndex(host, objv[3], strLen, &first) != 0) {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }
        if (objc == 5) {
            if (parseIndex(host, objv[4], strLen, &last) != 0) {
                tclSetError(interp, "bad index", -1);
                return TCL_ERROR;
            }
        }

        void *arena = host->arenaPush(interp->hostCtx);
        char *buf = host->arenaAlloc(arena, strLen + 1, 1);

        for (size_t i = 0; i < strLen; i++) {
            if (i >= first && i <= last) {
                buf[i] = toUpperChar(str[i]);
            } else {
                buf[i] = str[i];
            }
        }
        buf[strLen] = '\0';

        TclObj *result = host->newString(buf, strLen);
        host->arenaPop(interp->hostCtx, arena);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== string trim ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "trim", 4) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"string trim string ?chars?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        const char *chars = NULL;
        size_t charsLen = 0;
        if (objc == 4) {
            chars = host->getStringPtr(objv[3], &charsLen);
        }

        size_t first = 0, last = strLen > 0 ? strLen - 1 : 0;

        /* Trim left */
        while (first < strLen) {
            char c = str[first];
            int shouldTrim = chars ? charInSet(c, chars, charsLen) : isWhitespace(c);
            if (!shouldTrim) break;
            first++;
        }

        /* Trim right */
        while (last > first) {
            char c = str[last];
            int shouldTrim = chars ? charInSet(c, chars, charsLen) : isWhitespace(c);
            if (!shouldTrim) break;
            last--;
        }

        if (first > last || first >= strLen) {
            tclSetResult(interp, host->newString("", 0));
        } else {
            tclSetResult(interp, host->newString(&str[first], last - first + 1));
        }
        return TCL_OK;
    }

    /* ===== string trimleft ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "trimleft", 8) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"string trimleft string ?chars?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        const char *chars = NULL;
        size_t charsLen = 0;
        if (objc == 4) {
            chars = host->getStringPtr(objv[3], &charsLen);
        }

        size_t first = 0;
        while (first < strLen) {
            char c = str[first];
            int shouldTrim = chars ? charInSet(c, chars, charsLen) : isWhitespace(c);
            if (!shouldTrim) break;
            first++;
        }

        tclSetResult(interp, host->newString(&str[first], strLen - first));
        return TCL_OK;
    }

    /* ===== string trimright ===== */
    if (subcmdLen == 9 && tclStrncmp(subcmd, "trimright", 9) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"string trimright string ?chars?\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        const char *chars = NULL;
        size_t charsLen = 0;
        if (objc == 4) {
            chars = host->getStringPtr(objv[3], &charsLen);
        }

        size_t last = strLen;
        while (last > 0) {
            char c = str[last - 1];
            int shouldTrim = chars ? charInSet(c, chars, charsLen) : isWhitespace(c);
            if (!shouldTrim) break;
            last--;
        }

        tclSetResult(interp, host->newString(str, last));
        return TCL_OK;
    }

    /* ===== string wordend ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "wordend", 7) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"string wordend string charIndex\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t idx;
        if (parseIndex(host, objv[3], strLen, &idx) != 0) {
            tclSetError(interp, "bad index", -1);
            return TCL_ERROR;
        }

        if (idx >= strLen) {
            tclSetResult(interp, host->newInt((int64_t)strLen));
            return TCL_OK;
        }

        /* Find end of word */
        char c = str[idx];
        int isWordChar = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_';

        if (!isWordChar) {
            /* Single non-word char is its own word */
            tclSetResult(interp, host->newInt((int64_t)(idx + 1)));
        } else {
            /* Find end of word chars */
            size_t end = idx + 1;
            while (end < strLen) {
                c = str[end];
                isWordChar = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '_';
                if (!isWordChar) break;
                end++;
            }
            tclSetResult(interp, host->newInt((int64_t)end));
        }
        return TCL_OK;
    }

    /* ===== string wordstart ===== */
    if (subcmdLen == 9 && tclStrncmp(subcmd, "wordstart", 9) == 0) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"string wordstart string charIndex\"", -1);
            return TCL_ERROR;
        }

        size_t strLen;
        const char *str = host->getStringPtr(objv[2], &strLen);

        size_t idx;
        if (parseIndex(host, objv[3], strLen, &idx) != 0) {
            tclSetError(interp, "bad index", -1);
            return TCL_ERROR;
        }

        if (idx >= strLen) idx = strLen > 0 ? strLen - 1 : 0;

        /* Find start of word */
        char c = str[idx];
        int isWordChar = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_';

        if (!isWordChar) {
            /* Single non-word char is its own word */
            tclSetResult(interp, host->newInt((int64_t)idx));
        } else {
            /* Find start of word chars */
            size_t start = idx;
            while (start > 0) {
                c = str[start - 1];
                isWordChar = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '_';
                if (!isWordChar) break;
                start--;
            }
            tclSetResult(interp, host->newInt((int64_t)start));
        }
        return TCL_OK;
    }

    /* Unknown subcommand */
    tclSetError(interp, "unknown or ambiguous subcommand \"unknown\": must be cat, compare, equal, first, index, insert, is, last, length, map, match, range, repeat, replace, reverse, tolower, totitle, toupper, trim, trimleft, trimright, wordend, or wordstart", -1);
    return TCL_ERROR;
}
