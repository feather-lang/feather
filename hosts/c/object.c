/*
 * object.c - TclObj Implementation for C Host (GLib version)
 *
 * Implements TCL value objects with string representation and
 * optional cached numeric representations using GLib.
 */

#include "../../core/tclc.h"
#include <glib.h>
#include <string.h>
#include <math.h>

/* Object structure - opaque to core */
struct TclObj {
    gchar   *stringRep;      /* String representation (always valid) */
    gsize    stringLen;      /* Length of string */
    gboolean hasInt;         /* Has cached integer value */
    gint64   intRep;         /* Cached integer value */
    gboolean hasDouble;      /* Has cached double value */
    gdouble  doubleRep;      /* Cached double value */
    gint     refCount;       /* Reference count (for future use) */
};

/* Create a new string object */
TclObj *hostNewString(const char *s, size_t len) {
    TclObj *obj = g_new0(TclObj, 1);
    if (!obj) return NULL;

    obj->stringRep = g_strndup(s, len);
    if (!obj->stringRep) {
        g_free(obj);
        return NULL;
    }

    obj->stringLen = len;
    obj->hasInt = FALSE;
    obj->hasDouble = FALSE;
    obj->refCount = 1;

    return obj;
}

/* Create a new integer object */
TclObj *hostNewInt(int64_t val) {
    gchar *buf = g_strdup_printf("%" G_GINT64_FORMAT, val);
    gsize len = strlen(buf);

    TclObj *obj = hostNewString(buf, len);
    g_free(buf);

    if (obj) {
        obj->hasInt = TRUE;
        obj->intRep = val;
    }
    return obj;
}

/* Create a new double object */
TclObj *hostNewDouble(double val) {
    gchar buf[64];
    gint len;

    /* Handle special values */
    if (isnan(val)) {
        len = g_snprintf(buf, sizeof(buf), "NaN");
    } else if (isinf(val)) {
        len = g_snprintf(buf, sizeof(buf), val > 0 ? "Inf" : "-Inf");
    } else {
        len = g_snprintf(buf, sizeof(buf), "%g", val);

        /* Tcl always shows at least ".0" for floats that are whole numbers */
        gboolean hasDot = FALSE, hasE = FALSE;
        for (gint i = 0; i < len; i++) {
            if (buf[i] == '.') hasDot = TRUE;
            if (buf[i] == 'e' || buf[i] == 'E') hasE = TRUE;
        }
        if (!hasDot && !hasE && len < 62) {
            buf[len++] = '.';
            buf[len++] = '0';
            buf[len] = '\0';
        }
    }

    TclObj *obj = hostNewString(buf, len);
    if (obj) {
        obj->hasDouble = TRUE;
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
static gint needsListQuoting(const gchar *s, gsize len) {
    if (len == 0) return 1;  /* Empty string needs braces */

    /* Check if starts with # (needs brace quoting to prevent comment) */
    gboolean startsWithHash = (s[0] == '#');

    gboolean needsQuote = startsWithHash;
    gint braceDepth = 0;
    gboolean hasUnbalancedBrace = FALSE;
    gboolean hasQuote = FALSE;

    /* If string starts with { it could be parsed as a braced word - needs quoting */
    gboolean startsWithBrace = (s[0] == '{');
    if (startsWithBrace) needsQuote = TRUE;

    for (gsize i = 0; i < len; i++) {
        gchar c = s[i];
        if (c == '{') {
            braceDepth++;
        } else if (c == '}') {
            braceDepth--;
            if (braceDepth < 0) hasUnbalancedBrace = TRUE;
        } else if (c == '"') {
            hasQuote = TRUE;
            needsQuote = TRUE;
        } else if (c == '\\') {
            needsQuote = TRUE;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                   c == '$' || c == '[' || c == ']' || c == ';') {
            needsQuote = TRUE;
        }
    }

    if (braceDepth != 0) hasUnbalancedBrace = TRUE;

    /* Unbalanced braces need quoting */
    if (hasUnbalancedBrace) needsQuote = TRUE;

    /* Count trailing backslashes - odd count can't use brace quoting */
    gint trailingBackslashes = 0;
    for (gsize i = len; i > 0; i--) {
        if (s[i-1] == '\\') trailingBackslashes++;
        else break;
    }
    gboolean hasOddTrailingBackslash = (trailingBackslashes % 2 == 1);

    if (!needsQuote) return 0;
    /* Can't use brace quoting if: unbalanced braces, has quotes, or odd trailing backslash */
    if (hasUnbalancedBrace || hasQuote || hasOddTrailingBackslash) return 2;
    return 1;  /* can use brace quoting */
}

/* Count backslash-escaped length for an element */
static gsize backslashQuotedLen(const gchar *s, gsize len) {
    gsize result = 0;
    for (gsize i = 0; i < len; i++) {
        gchar c = s[i];
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
static gchar *writeBackslashQuoted(gchar *p, const gchar *s, gsize len) {
    for (gsize i = 0; i < len; i++) {
        gchar c = s[i];
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
    gsize totalLen = 0;
    for (gsize i = 0; i < count; i++) {
        gint quoteType = needsListQuoting(elems[i]->stringRep, elems[i]->stringLen);
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

    gchar *buf = g_malloc(totalLen + 1);
    if (!buf) return NULL;

    gchar *p = buf;
    for (gsize i = 0; i < count; i++) {
        if (i > 0) *p++ = ' ';
        gint quoteType = needsListQuoting(elems[i]->stringRep, elems[i]->stringLen);
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
    g_free(buf);
    return obj;
}

/* Create empty dict (as empty string for now) */
TclObj *hostNewDict(void) {
    return hostNewString("", 0);
}

/* Forward declaration */
const char *hostGetStringPtr(TclObj *obj, size_t *lenOut);

/* Set a key-value pair in a dict (appends to string representation) */
void hostDictSetInternal(TclObj *dict, const char *key, TclObj *val) {
    if (!dict || !key || !val) return;

    size_t valLen;
    const char *valStr = hostGetStringPtr(val, &valLen);

    /* Build new string: existing + " " + key + " " + value */
    size_t keyLen = strlen(key);
    size_t oldLen = dict->stringLen;
    size_t newLen = oldLen + (oldLen > 0 ? 1 : 0) + keyLen + 1 + valLen;

    gchar *newStr = g_malloc(newLen + 1);
    gchar *p = newStr;

    /* Copy existing content */
    if (oldLen > 0) {
        memcpy(p, dict->stringRep, oldLen);
        p += oldLen;
        *p++ = ' ';
    }

    /* Add key */
    memcpy(p, key, keyLen);
    p += keyLen;
    *p++ = ' ';

    /* Add value */
    memcpy(p, valStr, valLen);
    p += valLen;
    *p = '\0';

    /* Replace string rep */
    g_free(dict->stringRep);
    dict->stringRep = newStr;
    dict->stringLen = newLen;
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
        g_free(obj->stringRep);
        g_free(obj);
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
    gchar *endptr;
    gint64 val = g_ascii_strtoll(obj->stringRep, &endptr, 0);

    /* Check for errors */
    if (endptr == obj->stringRep) {
        return -1;
    }

    /* Skip trailing whitespace */
    while (*endptr == ' ' || *endptr == '\t') endptr++;
    if (*endptr != '\0') {
        return -1;
    }

    obj->hasInt = TRUE;
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
    gchar *endptr;
    gdouble val = g_ascii_strtod(obj->stringRep, &endptr);

    /* Check for errors */
    if (endptr == obj->stringRep) {
        return -1;
    }

    /* Skip trailing whitespace */
    while (*endptr == ' ' || *endptr == '\t') endptr++;
    if (*endptr != '\0') {
        return -1;
    }

    obj->hasDouble = TRUE;
    obj->doubleRep = val;
    *out = val;
    return 0;
}

/* Convert to boolean */
int hostAsBool(TclObj *obj, int *out) {
    if (!obj) return -1;

    /* Check for common boolean strings */
    const gchar *s = obj->stringRep;
    gsize len = obj->stringLen;

    if (len == 1) {
        if (s[0] == '0') { *out = 0; return 0; }
        if (s[0] == '1') { *out = 1; return 0; }
    }

    if (g_ascii_strcasecmp(s, "true") == 0 ||
        g_ascii_strcasecmp(s, "yes") == 0 ||
        g_ascii_strcasecmp(s, "on") == 0) {
        *out = 1;
        return 0;
    }

    if (g_ascii_strcasecmp(s, "false") == 0 ||
        g_ascii_strcasecmp(s, "no") == 0 ||
        g_ascii_strcasecmp(s, "off") == 0) {
        *out = 0;
        return 0;
    }

    /* Try as integer */
    gint64 ival;
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

static gboolean isListSpace(gchar c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Parse a single list element starting at *pos.
 * Updates *pos to point past the element.
 * Returns a new TclObj for the element, or NULL on error.
 */
static TclObj *parseListElement(const gchar **pos, const gchar *end) {
    const gchar *p = *pos;

    /* Skip leading whitespace */
    while (p < end && isListSpace(*p)) p++;

    if (p >= end) {
        *pos = p;
        return NULL;  /* No more elements */
    }

    const gchar *start;
    gsize len;

    if (*p == '{') {
        /* Braced element - find matching close brace */
        p++;  /* Skip { */
        start = p;
        gint depth = 1;
        while (p < end && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            if (depth > 0) p++;
        }
        /* Check for unclosed brace */
        if (depth > 0) {
            *pos = p;
            return (TclObj *)(intptr_t)-1;  /* Error indicator */
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
    gboolean hasEscape = FALSE;
    const gchar *scanP = p;
    while (scanP < end) {
        if (*scanP == '\\' && scanP + 1 < end) {
            hasEscape = TRUE;
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
    gchar *buf = g_malloc(len + 1);
    if (!buf) return hostNewString(start, len);

    gchar *out = buf;
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
    g_free(buf);
    return result;
}

/* Parse list into array of elements.
 * Caller must free the array and its elements.
 */
static gint parseList(TclObj *obj, TclObj ***elemsOut, gsize *countOut) {
    if (!obj) {
        *elemsOut = NULL;
        *countOut = 0;
        return 0;
    }

    const gchar *str = obj->stringRep;
    gsize strLen = obj->stringLen;
    const gchar *end = str + strLen;

    /* Use GPtrArray to collect elements */
    GPtrArray *elems = g_ptr_array_new();
    const gchar *pos = str;

    while (pos < end) {
        /* Skip whitespace */
        while (pos < end && isListSpace(*pos)) pos++;
        if (pos >= end) break;

        TclObj *elem = parseListElement(&pos, end);
        if (!elem) break;
        if (elem == (TclObj *)(intptr_t)-1) {
            /* Parse error - unclosed brace/quote */
            for (guint i = 0; i < elems->len; i++) {
                hostFreeObj(g_ptr_array_index(elems, i));
            }
            g_ptr_array_free(elems, TRUE);
            return -1;
        }

        g_ptr_array_add(elems, elem);
    }

    *countOut = elems->len;
    *elemsOut = (TclObj **)g_ptr_array_free(elems, FALSE);
    return 0;
}

/* Convert to list */
int hostAsList(TclObj *obj, TclObj ***elemsOut, size_t *countOut) {
    gsize count;
    gint result = parseList(obj, elemsOut, &count);
    *countOut = count;
    return result;
}

/* Get list length */
size_t hostListLengthImpl(TclObj *list) {
    if (!list || list->stringLen == 0) return 0;

    TclObj **elems;
    gsize count;
    if (parseList(list, &elems, &count) != 0) return 0;

    /* Free the parsed elements */
    for (gsize i = 0; i < count; i++) {
        hostFreeObj(elems[i]);
    }
    g_free(elems);

    return count;
}

/* Get list element by index */
TclObj *hostListIndexImpl(TclObj *list, size_t idx) {
    if (!list) return NULL;

    TclObj **elems;
    gsize count;
    if (parseList(list, &elems, &count) != 0) return NULL;

    TclObj *result = NULL;
    if (idx < count) {
        result = hostDup(elems[idx]);
    }

    /* Free the parsed elements */
    for (gsize i = 0; i < count; i++) {
        hostFreeObj(elems[i]);
    }
    g_free(elems);

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
