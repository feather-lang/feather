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
                /* Variable not found - error */
                tclSetError(interp, "can't read variable: no such variable", -1);
                host->arenaPop(interp->hostCtx, arena);
                return NULL;
            }

            /* Append value to output */
            size_t valLen;
            const char *valStr = host->getStringPtr(value, &valLen);
            if (dst + valLen < dstEnd) {
                for (size_t i = 0; i < valLen; i++) {
                    *dst++ = valStr[i];
                }
            }
            continue;
        }

        /* Command substitution */
        if ((flags & TCL_SUBST_COMMANDS) && *src == '[') {
            src++;  /* Skip [ */
            const char *cmdStart = src;
            int depth = 1;

            while (src < end && depth > 0) {
                if (*src == '[') depth++;
                else if (*src == ']') depth--;
                if (depth > 0) src++;
            }

            size_t cmdLen = src - cmdStart;
            if (src < end) src++;  /* Skip ] */

            /* Execute the command */
            TclResult result = tclEvalBracketed(interp, cmdStart, cmdLen);
            if (result != TCL_OK) {
                host->arenaPop(interp->hostCtx, arena);
                return NULL;
            }

            /* Append result to output */
            if (interp->result) {
                size_t resLen;
                const char *resStr = host->getStringPtr(interp->result, &resLen);
                if (dst + resLen < dstEnd) {
                    for (size_t i = 0; i < resLen; i++) {
                        *dst++ = resStr[i];
                    }
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
