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

/* Create a new double object */
TclObj *hostNewDouble(double val) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", val);

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

/* Create a new list object (space-separated for now) */
TclObj *hostNewList(TclObj **elems, size_t count) {
    if (count == 0) {
        return hostNewString("", 0);
    }

    /* Calculate total length needed */
    size_t totalLen = 0;
    for (size_t i = 0; i < count; i++) {
        totalLen += elems[i]->stringLen;
        if (i > 0) totalLen++; /* space separator */
    }

    char *buf = malloc(totalLen + 1);
    if (!buf) return NULL;

    char *p = buf;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) *p++ = ' ';
        memcpy(p, elems[i]->stringRep, elems[i]->stringLen);
        p += elems[i]->stringLen;
    }
    *p = '\0';

    TclObj *obj = hostNewString(buf, totalLen);
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

/* Convert to list (stub - proper implementation would parse) */
int hostAsList(TclObj *obj, TclObj ***elemsOut, size_t *countOut) {
    /* TODO: Proper list parsing */
    (void)obj;
    (void)elemsOut;
    (void)countOut;
    return -1;
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
