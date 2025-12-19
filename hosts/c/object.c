/*
 * object.c - TclObj Implementation for C Host
 *
 * Implements TCL value objects with string representation and
 * optional cached numeric representations.
 */

#include "../../core/tclc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

/* Object structure - opaque to core */
struct TclObj {
    char   *stringRep;      /* String representation (always valid) */
    size_t  stringLen;      /* Length of string */
    int     hasInt;         /* Has cached integer value */
    int64_t intRep;         /* Cached integer value */
    int     hasDouble;      /* Has cached double value */
    double  doubleRep;      /* Cached double value */
    int     refCount;       /* Reference count (for future use) */
};

/* Create a new string object */
TclObj *hostNewString(const char *s, size_t len) {
    TclObj *obj = malloc(sizeof(TclObj));
    if (!obj) return NULL;

    obj->stringRep = malloc(len + 1);
    if (!obj->stringRep) {
        free(obj);
        return NULL;
    }

    memcpy(obj->stringRep, s, len);
    obj->stringRep[len] = '\0';
    obj->stringLen = len;
    obj->hasInt = 0;
    obj->hasDouble = 0;
    obj->refCount = 1;

    return obj;
}

/* Create a new integer object */
TclObj *hostNewInt(int64_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)val);

    TclObj *obj = hostNewString(buf, len);
    if (obj) {
        obj->hasInt = 1;
        obj->intRep = val;
    }
    return obj;
}

/* Helper to check if value is infinite */
static int isInf(double val) {
    return val > 1e308 || val < -1e308;
}

/* Helper to check if value is NaN */
static int isNaN(double val) {
    return val != val;
}

/* Create a new double object */
TclObj *hostNewDouble(double val) {
    char buf[64];
    int len;

    /* Handle special values */
    if (isNaN(val)) {
        len = snprintf(buf, sizeof(buf), "NaN");
    } else if (isInf(val)) {
        len = snprintf(buf, sizeof(buf), val > 0 ? "Inf" : "-Inf");
    } else {
        len = snprintf(buf, sizeof(buf), "%g", val);

        /* Tcl always shows at least ".0" for floats that are whole numbers */
        /* Check if we need to add ".0" - if no '.' or 'e' in the output */
        int hasDot = 0, hasE = 0;
        for (int i = 0; i < len; i++) {
            if (buf[i] == '.') hasDot = 1;
            if (buf[i] == 'e' || buf[i] == 'E') hasE = 1;
        }
        if (!hasDot && !hasE && len < 62) {
            buf[len++] = '.';
            buf[len++] = '0';
            buf[len] = '\0';
        }
    }

    TclObj *obj = hostNewString(buf, len);
    if (obj) {
        obj->hasDouble = 1;
        obj->doubleRep = val;
    }
    return obj;
}

/* Create a new boolean object */
TclObj *hostNewBool(int val) {
    return hostNewString(val ? "1" : "0", 1);
}

/* Check if a string needs quoting in a list - returns:
 * 0 = no quoting needed
 * 1 = can use brace quoting
 * 2 = needs backslash quoting (unbalanced braces, odd trailing \, or ")
 */
static int needsListQuoting(const char *s, size_t len) {
    if (len == 0) return 1;  /* Empty string needs braces */

    /* Check if starts with # (needs brace quoting to prevent comment) */
    int startsWithHash = (s[0] == '#');

    int needsQuote = startsWithHash;
    int braceDepth = 0;
    int hasUnbalancedBrace = 0;
    int hasQuote = 0;

    /* Check if starts with { or ends with } - these need quoting */
    int startsWithBrace = (len > 0 && s[0] == '{');
    int endsWithBrace = (len > 0 && s[len-1] == '}');

    if (startsWithBrace || endsWithBrace) needsQuote = 1;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '{') {
            braceDepth++;
        } else if (c == '}') {
            braceDepth--;
            if (braceDepth < 0) hasUnbalancedBrace = 1;
        } else if (c == '"') {
            hasQuote = 1;
            needsQuote = 1;
        } else if (c == '\\') {
            needsQuote = 1;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                   c == '$' || c == '[' || c == ']' || c == ';') {
            needsQuote = 1;
        }
    }

    if (braceDepth != 0) hasUnbalancedBrace = 1;

    /* Unbalanced braces need quoting */
    if (hasUnbalancedBrace) needsQuote = 1;

    /* Count trailing backslashes - odd count can't use brace quoting */
    int trailingBackslashes = 0;
    for (size_t i = len; i > 0; i--) {
        if (s[i-1] == '\\') trailingBackslashes++;
        else break;
    }
    int hasOddTrailingBackslash = (trailingBackslashes % 2 == 1);

    if (!needsQuote) return 0;
    /* Can't use brace quoting if: unbalanced braces, has quotes, or odd trailing backslash */
    if (hasUnbalancedBrace || hasQuote || hasOddTrailingBackslash) return 2;
    return 1;  /* can use brace quoting */
}

/* Count backslash-escaped length for an element */
static size_t backslashQuotedLen(const char *s, size_t len) {
    size_t result = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '{' || c == '}' || c == '\\' || c == '"' ||
            c == '$' || c == '[' || c == ']' || c == ' ' || c == ';') {
            result += 2;  /* backslash + char */
        } else {
            result += 1;
        }
    }
    return result;
}

/* Write backslash-quoted string */
static char *writeBackslashQuoted(char *p, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '{' || c == '}' || c == '\\' || c == '"' ||
            c == '$' || c == '[' || c == ']' || c == ' ' || c == ';') {
            *p++ = '\\';
        }
        *p++ = c;
    }
    return p;
}

/* Create a new list object with proper quoting */
TclObj *hostNewList(TclObj **elems, size_t count) {
    if (count == 0) {
        return hostNewString("", 0);
    }

    /* Calculate total length needed */
    size_t totalLen = 0;
    for (size_t i = 0; i < count; i++) {
        int quoteType = needsListQuoting(elems[i]->stringRep, elems[i]->stringLen);
        if (quoteType == 2) {
            /* Backslash quoting */
            totalLen += backslashQuotedLen(elems[i]->stringRep, elems[i]->stringLen);
        } else if (quoteType == 1) {
            totalLen += elems[i]->stringLen + 2;  /* Add {} */
        } else {
            totalLen += elems[i]->stringLen;
        }
        if (i > 0) totalLen++;  /* space separator */
    }

    char *buf = malloc(totalLen + 1);
    if (!buf) return NULL;

    char *p = buf;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) *p++ = ' ';
        int quoteType = needsListQuoting(elems[i]->stringRep, elems[i]->stringLen);
        if (quoteType == 2) {
            /* Backslash quoting */
            p = writeBackslashQuoted(p, elems[i]->stringRep, elems[i]->stringLen);
        } else if (quoteType == 1) {
            *p++ = '{';
            memcpy(p, elems[i]->stringRep, elems[i]->stringLen);
            p += elems[i]->stringLen;
            *p++ = '}';
        } else {
            memcpy(p, elems[i]->stringRep, elems[i]->stringLen);
            p += elems[i]->stringLen;
        }
    }
    *p = '\0';

    TclObj *obj = hostNewString(buf, p - buf);
    free(buf);
    return obj;
}

/* Create empty dict (as empty string for now) */
TclObj *hostNewDict(void) {
    return hostNewString("", 0);
}

/* Duplicate an object */
TclObj *hostDup(TclObj *obj) {
    if (!obj) return NULL;
    TclObj *dup = hostNewString(obj->stringRep, obj->stringLen);
    if (dup) {
        dup->hasInt = obj->hasInt;
        dup->intRep = obj->intRep;
        dup->hasDouble = obj->hasDouble;
        dup->doubleRep = obj->doubleRep;
    }
    return dup;
}

/* Free an object */
void hostFreeObj(TclObj *obj) {
    if (obj) {
        free(obj->stringRep);
        free(obj);
    }
}

/* Get string representation */
const char *hostGetStringPtr(TclObj *obj, size_t *lenOut) {
    if (!obj) {
        if (lenOut) *lenOut = 0;
        return "";
    }
    if (lenOut) *lenOut = obj->stringLen;
    return obj->stringRep;
}

/* Convert to integer (caches result) */
int hostAsInt(TclObj *obj, int64_t *out) {
    if (!obj) return -1;

    if (obj->hasInt) {
        *out = obj->intRep;
        return 0;
    }

    /* Try to parse */
    char *endptr;
    errno = 0;
    long long val = strtoll(obj->stringRep, &endptr, 0);

    /* Check for errors */
    if (errno == ERANGE || endptr == obj->stringRep) {
        return -1;
    }

    /* Skip trailing whitespace */
    while (*endptr == ' ' || *endptr == '\t') endptr++;
    if (*endptr != '\0') {
        return -1;
    }

    obj->hasInt = 1;
    obj->intRep = val;
    *out = val;
    return 0;
}

/* Convert to double (caches result) */
int hostAsDouble(TclObj *obj, double *out) {
    if (!obj) return -1;

    if (obj->hasDouble) {
        *out = obj->doubleRep;
        return 0;
    }

    /* Try to parse */
    char *endptr;
    errno = 0;
    double val = strtod(obj->stringRep, &endptr);

    /* Check for errors */
    if (errno == ERANGE || endptr == obj->stringRep) {
        return -1;
    }

    /* Skip trailing whitespace */
    while (*endptr == ' ' || *endptr == '\t') endptr++;
    if (*endptr != '\0') {
        return -1;
    }

    obj->hasDouble = 1;
    obj->doubleRep = val;
    *out = val;
    return 0;
}

/* Convert to boolean */
int hostAsBool(TclObj *obj, int *out) {
    if (!obj) return -1;

    /* Check for common boolean strings */
    const char *s = obj->stringRep;
    size_t len = obj->stringLen;

    if (len == 1) {
        if (s[0] == '0') { *out = 0; return 0; }
        if (s[0] == '1') { *out = 1; return 0; }
    }

    if ((len == 4 && strncmp(s, "true", 4) == 0) ||
        (len == 3 && strncmp(s, "yes", 3) == 0) ||
        (len == 2 && strncmp(s, "on", 2) == 0)) {
        *out = 1;
        return 0;
    }

    if ((len == 5 && strncmp(s, "false", 5) == 0) ||
        (len == 2 && strncmp(s, "no", 2) == 0) ||
        (len == 3 && strncmp(s, "off", 3) == 0)) {
        *out = 0;
        return 0;
    }

    /* Try as integer */
    int64_t ival;
    if (hostAsInt(obj, &ival) == 0) {
        *out = (ival != 0);
        return 0;
    }

    return -1;
}

/* ========================================================================
 * TCL List Parsing
 *
 * Parses TCL list syntax: elements separated by whitespace.
 * Elements can be quoted with braces {}, double quotes "", or bare words.
 * ======================================================================== */

static int isListSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Parse a single list element starting at *pos.
 * Updates *pos to point past the element.
 * Returns a new TclObj for the element, or NULL on error.
 */
static TclObj *parseListElement(const char **pos, const char *end) {
    const char *p = *pos;

    /* Skip leading whitespace */
    while (p < end && isListSpace(*p)) p++;

    if (p >= end) {
        *pos = p;
        return NULL;  /* No more elements */
    }

    const char *start;
    size_t len;

    if (*p == '{') {
        /* Braced element - find matching close brace */
        p++;  /* Skip { */
        start = p;
        int depth = 1;
        while (p < end && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            if (depth > 0) p++;
        }
        len = p - start;
        if (p < end) p++;  /* Skip } */
        *pos = p;
        return hostNewString(start, len);
    }

    if (*p == '"') {
        /* Quoted element - find closing quote */
        p++;  /* Skip " */
        start = p;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) p++;  /* Skip escape */
            p++;
        }
        len = p - start;
        if (p < end) p++;  /* Skip " */
        *pos = p;
        return hostNewString(start, len);
    }

    /* Bare word - read until unescaped whitespace, and unescape backslashes */
    start = p;
    int hasEscape = 0;
    const char *scanP = p;
    while (scanP < end) {
        if (*scanP == '\\' && scanP + 1 < end) {
            hasEscape = 1;
            scanP += 2;
        } else if (isListSpace(*scanP)) {
            break;
        } else {
            scanP++;
        }
    }
    len = scanP - start;
    *pos = scanP;

    if (!hasEscape) {
        return hostNewString(start, len);
    }

    /* Need to unescape backslashes */
    char *buf = malloc(len + 1);
    if (!buf) return hostNewString(start, len);

    char *out = buf;
    p = start;
    while (p < scanP) {
        if (*p == '\\' && p + 1 < scanP) {
            p++;  /* Skip backslash */
            *out++ = *p++;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';

    TclObj *result = hostNewString(buf, out - buf);
    free(buf);
    return result;
}

/* Parse list into array of elements.
 * Caller must free the array and its elements.
 */
static int parseList(TclObj *obj, TclObj ***elemsOut, size_t *countOut) {
    if (!obj) {
        *elemsOut = NULL;
        *countOut = 0;
        return 0;
    }

    const char *str = obj->stringRep;
    size_t strLen = obj->stringLen;
    const char *end = str + strLen;

    /* Count elements first (estimate) */
    size_t capacity = 16;
    TclObj **elems = malloc(capacity * sizeof(TclObj*));
    if (!elems) return -1;

    size_t count = 0;
    const char *pos = str;

    while (pos < end) {
        /* Skip whitespace */
        while (pos < end && isListSpace(*pos)) pos++;
        if (pos >= end) break;

        TclObj *elem = parseListElement(&pos, end);
        if (!elem) break;

        /* Grow array if needed */
        if (count >= capacity) {
            capacity *= 2;
            TclObj **newElems = realloc(elems, capacity * sizeof(TclObj*));
            if (!newElems) {
                for (size_t i = 0; i < count; i++) {
                    free(elems[i]->stringRep);
                    free(elems[i]);
                }
                free(elems);
                return -1;
            }
            elems = newElems;
        }

        elems[count++] = elem;
    }

    *elemsOut = elems;
    *countOut = count;
    return 0;
}

/* Convert to list */
int hostAsList(TclObj *obj, TclObj ***elemsOut, size_t *countOut) {
    return parseList(obj, elemsOut, countOut);
}

/* Get list length */
size_t hostListLengthImpl(TclObj *list) {
    if (!list || list->stringLen == 0) return 0;

    TclObj **elems;
    size_t count;
    if (parseList(list, &elems, &count) != 0) return 0;

    /* Free the parsed elements */
    for (size_t i = 0; i < count; i++) {
        free(elems[i]->stringRep);
        free(elems[i]);
    }
    free(elems);

    return count;
}

/* Get list element by index */
TclObj *hostListIndexImpl(TclObj *list, size_t idx) {
    if (!list) return NULL;

    TclObj **elems;
    size_t count;
    if (parseList(list, &elems, &count) != 0) return NULL;

    TclObj *result = NULL;
    if (idx < count) {
        result = hostDup(elems[idx]);
    }

    /* Free the parsed elements */
    for (size_t i = 0; i < count; i++) {
        free(elems[i]->stringRep);
        free(elems[i]);
    }
    free(elems);

    return result;
}

/* String length in characters (bytes for now, TODO: UTF-8) */
size_t hostStringLength(TclObj *str) {
    return str ? str->stringLen : 0;
}

/* String comparison */
int hostStringCompare(TclObj *a, TclObj *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a->stringRep, b->stringRep);
}
