/*
 * subst.c - TCL Substitution Engine
 *
 * Performs $var, [cmd], and \x substitution.
 * Uses arena for intermediate strings, returns host-allocated TclObj.
 */

#include "internal.h"

/* Forward declaration for command substitution */
extern TclResult tclEvalBracketed(TclInterp *interp, const char *cmd, size_t len);

/* ========================================================================
 * Backslash Substitution
 * ======================================================================== */

/* Process a single backslash escape sequence.
 * Returns number of input characters consumed (including backslash).
 * Writes output character to *out.
 */
/* Helper to convert hex char to value */
static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Helper to convert octal char to value */
static int octDigit(char c) {
    if (c >= '0' && c <= '7') return c - '0';
    return -1;
}

int tclSubstBackslashChar(const char *src, const char *end, char *out) {
    if (src >= end || *src != '\\') {
        return 0;
    }

    if (src + 1 >= end) {
        *out = '\\';
        return 1;
    }

    char c = src[1];
    switch (c) {
        case 'a':  *out = '\a'; return 2;
        case 'b':  *out = '\b'; return 2;
        case 'f':  *out = '\f'; return 2;
        case 'n':  *out = '\n'; return 2;
        case 'r':  *out = '\r'; return 2;
        case 't':  *out = '\t'; return 2;
        case 'v':  *out = '\v'; return 2;
        case '\\': *out = '\\'; return 2;
        case '"':  *out = '"';  return 2;
        case '{':  *out = '{';  return 2;
        case '}':  *out = '}';  return 2;
        case '[':  *out = '[';  return 2;
        case ']':  *out = ']';  return 2;
        case '$':  *out = '$';  return 2;
        case '\n':
            /* Backslash-newline: replace with single space, skip following whitespace */
            *out = ' ';
            return 2;  /* Caller should handle skipping whitespace */
        case 'x': {
            /* Hex escape: \xNN (1-2 hex digits) */
            int val = 0;
            int consumed = 2;  /* \x */
            const char *p = src + 2;
            while (p < end && consumed < 4) {  /* Up to 2 hex digits */
                int d = hexDigit(*p);
                if (d < 0) break;
                val = (val << 4) | d;
                p++;
                consumed++;
            }
            if (consumed == 2) {
                /* No hex digits - output literal 'x' */
                *out = 'x';
                return 2;
            }
            *out = (char)val;
            return consumed;
        }
        case 'u': {
            /* Unicode escape: \uNNNN (4 hex digits) */
            if (src + 5 < end) {
                int val = 0;
                for (int i = 0; i < 4; i++) {
                    int d = hexDigit(src[2 + i]);
                    if (d < 0) {
                        *out = 'u';
                        return 2;
                    }
                    val = (val << 4) | d;
                }
                /* For now, just output low byte (ASCII/Latin-1) */
                *out = (char)(val & 0xFF);
                return 6;
            }
            *out = 'u';
            return 2;
        }
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': {
            /* Octal escape: \NNN (1-3 octal digits) */
            int val = 0;
            int consumed = 1;  /* \ */
            const char *p = src + 1;
            while (p < end && consumed < 4) {  /* Up to 3 octal digits */
                int d = octDigit(*p);
                if (d < 0) break;
                val = (val << 3) | d;
                p++;
                consumed++;
            }
            *out = (char)val;
            return consumed;
        }
        default:
            /* Unknown escape: just pass through the character */
            *out = c;
            return 2;
    }
}

/* ========================================================================
 * Variable Substitution Helpers
 * ======================================================================== */

/* Check if character is valid in a variable name */
static int isVarChar(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

/* ========================================================================
 * Main Substitution Function
 * ======================================================================== */

TclObj *tclSubstString(TclInterp *interp, const char *str, size_t len, int flags) {
    const TclHost *host = interp->host;
    void *arena = host->arenaPush(interp->hostCtx);

    /* Allocate output buffer in arena (may need more space for escapes) */
    size_t bufSize = len * 2 + 1;
    if (bufSize < 256) bufSize = 256;
    char *buf = host->arenaAlloc(arena, bufSize, 1);
    if (!buf) {
        host->arenaPop(interp->hostCtx, arena);
        return NULL;
    }

    const char *src = str;
    const char *end = str + len;
    char *dst = buf;
    char *dstEnd = buf + bufSize - 1;

    while (src < end && dst < dstEnd) {
        /* Backslash substitution */
        if ((flags & TCL_SUBST_BACKSLASH) && *src == '\\' && src + 1 < end) {
            char c;
            int consumed = tclSubstBackslashChar(src, end, &c);
            *dst++ = c;
            src += consumed;

            /* Handle backslash-newline: skip following whitespace */
            if (c == ' ' && consumed == 2 && src > str && src[-1] == '\n') {
                while (src < end && (*src == ' ' || *src == '\t')) {
                    src++;
                }
            }
            continue;
        }

        /* Variable substitution */
        if ((flags & TCL_SUBST_VARIABLES) && *src == '$') {
            src++;  /* Skip $ */

            const char *varName;
            size_t varNameLen;
            const char *arrayKey = NULL;
            size_t arrayKeyLen = 0;

            if (src < end && *src == '{') {
                /* ${varname} syntax */
                src++;  /* Skip { */
                varName = src;
                while (src < end && *src != '}') {
                    src++;
                }
                varNameLen = src - varName;
                if (src < end) src++;  /* Skip } */
            } else {
                /* $varname syntax */
                varName = src;
                while (src < end && isVarChar(*src)) {
                    src++;
                }
                varNameLen = src - varName;

                /* Check for array element: $arr(key) */
                if (src < end && *src == '(') {
                    src++;  /* Skip ( */
                    arrayKey = src;
                    int depth = 1;
                    while (src < end && depth > 0) {
                        if (*src == '(') depth++;
                        else if (*src == ')') depth--;
                        if (depth > 0) src++;
                    }
                    arrayKeyLen = src - arrayKey;
                    if (src < end) src++;  /* Skip ) */
                }
            }

            if (varNameLen == 0) {
                /* Lone $ - output it literally */
                *dst++ = '$';
                continue;
            }

            /* Look up variable */
            TclObj *value = NULL;
            void *vars = interp->currentFrame->varsHandle;

            if (arrayKey) {
                value = host->arrayGet(vars, varName, varNameLen, arrayKey, arrayKeyLen);
            } else {
                value = host->varGet(vars, varName, varNameLen);
            }

            /* Try global frame if not found */
            if (!value && interp->currentFrame != interp->globalFrame) {
                vars = interp->globalFrame->varsHandle;
                if (arrayKey) {
                    value = host->arrayGet(vars, varName, varNameLen, arrayKey, arrayKeyLen);
                } else {
                    value = host->varGet(vars, varName, varNameLen);
                }
            }

            if (!value) {
                /* Variable not found - error: can't read "varname": no such variable */
                char *errBuf = host->arenaAlloc(arena, varNameLen + 50, 1);
                char *ep = errBuf;
                const char *prefix = "can't read \"";
                while (*prefix) *ep++ = *prefix++;
                for (size_t i = 0; i < varNameLen; i++) *ep++ = varName[i];
                const char *suffix = "\": no such variable";
                while (*suffix) *ep++ = *suffix++;
                *ep = '\0';
                tclSetError(interp, errBuf, ep - errBuf);
                host->arenaPop(interp->hostCtx, arena);
                return NULL;
            }

            /* Append value to output */
            size_t valLen;
            const char *valStr = host->getStringPtr(value, &valLen);
            size_t dstUsed = dst - buf;
            size_t needed = dstUsed + valLen + (end - src) + 1;

            /* Reallocate if needed */
            if (needed > bufSize) {
                size_t newSize = needed * 2;
                char *newBuf = host->arenaAlloc(arena, newSize, 1);
                if (!newBuf) {
                    host->arenaPop(interp->hostCtx, arena);
                    return NULL;
                }
                for (size_t i = 0; i < dstUsed; i++) {
                    newBuf[i] = buf[i];
                }
                buf = newBuf;
                dst = buf + dstUsed;
                bufSize = newSize;
                dstEnd = buf + bufSize - 1;
            }

            /* Now copy the value */
            for (size_t i = 0; i < valLen; i++) {
                *dst++ = valStr[i];
            }
            continue;
        }

        /* Command substitution */
        if ((flags & TCL_SUBST_COMMANDS) && *src == '[') {
            src++;  /* Skip [ */
            const char *cmdStart = src;
            int depth = 1;

            while (src < end && depth > 0) {
                char ch = *src;
                if (ch == '{') {
                    /* Skip braced content - brackets inside don't count */
                    int braceDepth = 1;
                    src++;
                    while (src < end && braceDepth > 0) {
                        if (*src == '{') braceDepth++;
                        else if (*src == '}') braceDepth--;
                        if (braceDepth > 0) src++;
                    }
                    if (src < end) src++;  /* Skip } */
                    continue;
                } else if (ch == '"') {
                    /* Skip quoted content - brackets inside don't count */
                    src++;
                    while (src < end && *src != '"') {
                        if (*src == '\\' && src + 1 < end) src++;  /* Skip escaped char */
                        src++;
                    }
                    if (src < end) src++;  /* Skip " */
                    continue;
                } else if (ch == '[') {
                    depth++;
                } else if (ch == ']') {
                    depth--;
                }
                if (depth > 0) src++;
            }

            size_t cmdLen = src - cmdStart;
            if (src < end) src++;  /* Skip ] */

            /* Execute the command */
            TclResult result = tclEvalBracketed(interp, cmdStart, cmdLen);
            if (result == TCL_ERROR) {
                host->arenaPop(interp->hostCtx, arena);
                return NULL;
            }
            if (result == TCL_BREAK) {
                /* Break: stop substitution, return what we have so far */
                *dst = '\0';
                size_t resultLen = dst - buf;
                TclObj *res = host->newString(buf, resultLen);
                host->arenaPop(interp->hostCtx, arena);
                return res;
            }
            if (result == TCL_CONTINUE) {
                /* Continue: substitute empty string (skip appending result) */
                continue;
            }

            /* TCL_OK, TCL_RETURN, or other: append result to output */
            if (interp->result) {
                size_t resLen;
                const char *resStr = host->getStringPtr(interp->result, &resLen);
                size_t dstUsed = dst - buf;
                size_t needed = dstUsed + resLen + (end - src) + 1;

                /* Reallocate if needed */
                if (needed > bufSize) {
                    size_t newSize = needed * 2;
                    char *newBuf = host->arenaAlloc(arena, newSize, 1);
                    if (!newBuf) {
                        host->arenaPop(interp->hostCtx, arena);
                        return NULL;
                    }
                    for (size_t i = 0; i < dstUsed; i++) {
                        newBuf[i] = buf[i];
                    }
                    buf = newBuf;
                    dst = buf + dstUsed;
                    bufSize = newSize;
                    dstEnd = buf + bufSize - 1;
                }

                /* Now copy the result */
                for (size_t i = 0; i < resLen; i++) {
                    *dst++ = resStr[i];
                }
            }
            continue;
        }

        /* Regular character */
        *dst++ = *src++;
    }

    *dst = '\0';
    size_t resultLen = dst - buf;

    /* Create result object */
    TclObj *result = host->newString(buf, resultLen);
    host->arenaPop(interp->hostCtx, arena);

    return result;
}

/* ========================================================================
 * Word Substitution
 * ======================================================================== */

TclObj *tclSubstWord(TclInterp *interp, TclWord *word, int flags) {
    const TclHost *host = interp->host;

    if (word->type == TCL_WORD_BRACES) {
        /* Braces: no substitution, return literal content */
        return host->newString(word->start, word->len);
    }

    /* Quotes and bare words: perform substitution */
    return tclSubstString(interp, word->start, word->len, flags);
}
