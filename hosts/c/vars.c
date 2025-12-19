/*
 * vars.c - Variable Table Implementation for C Host
 *
 * Simple hash table for variable storage.
 */

#include "../../core/tclc.h"
#include <stdlib.h>
#include <string.h>

/* External object functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);
extern void hostFreeObj(TclObj *obj);
extern const char *hostGetStringPtr(TclObj *obj, size_t *lenOut);

/* Forward declaration */
typedef struct VarTable VarTable;

/* Hash table entry */
typedef struct VarEntry {
    char            *name;      /* Variable name */
    size_t           nameLen;   /* Name length */
    TclObj          *value;     /* Variable value (NULL if linked) */
    struct VarEntry *next;      /* Next in chain */
    /* Link information for upvar/global */
    VarTable        *linkTable; /* Target table (NULL if not linked) */
    char            *linkName;  /* Target variable name */
    size_t           linkNameLen;
} VarEntry;

/* Hash table */
#define VAR_TABLE_SIZE 256

struct VarTable {
    VarEntry *buckets[VAR_TABLE_SIZE];
};

/* Simple hash function */
static unsigned int hashName(const char *name, size_t len) {
    unsigned int hash = 0;
    for (size_t i = 0; i < len; i++) {
        hash = hash * 31 + (unsigned char)name[i];
    }
    return hash % VAR_TABLE_SIZE;
}

/* Create new variable table */
void *hostVarsNew(void *ctx) {
    (void)ctx;
    VarTable *table = calloc(1, sizeof(VarTable));
    return table;
}

/* Free variable table */
void hostVarsFree(void *ctx, void *vars) {
    (void)ctx;
    VarTable *table = vars;
    if (!table) return;

    for (int i = 0; i < VAR_TABLE_SIZE; i++) {
        VarEntry *entry = table->buckets[i];
        while (entry) {
            VarEntry *next = entry->next;
            free(entry->name);
            if (entry->value) hostFreeObj(entry->value);
            if (entry->linkName) free(entry->linkName);
            free(entry);
            entry = next;
        }
    }
    free(table);
}

/* Find entry by name */
static VarEntry *findEntry(VarTable *table, const char *name, size_t len) {
    unsigned int h = hashName(name, len);
    VarEntry *entry = table->buckets[h];

    while (entry) {
        if (entry->nameLen == len && memcmp(entry->name, name, len) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Get variable value (follows links) */
TclObj *hostVarGet(void *vars, const char *name, size_t len) {
    VarTable *table = vars;
    if (!table) return NULL;

    VarEntry *entry = findEntry(table, name, len);
    if (!entry) return NULL;

    /* Follow link if this is a linked variable */
    if (entry->linkTable) {
        return hostVarGet(entry->linkTable, entry->linkName, entry->linkNameLen);
    }

    return entry->value;
}

/* Set variable value (follows links) */
void hostVarSet(void *vars, const char *name, size_t len, TclObj *val) {
    VarTable *table = vars;
    if (!table) return;

    VarEntry *entry = findEntry(table, name, len);
    if (entry) {
        /* Follow link if this is a linked variable */
        if (entry->linkTable) {
            hostVarSet(entry->linkTable, entry->linkName, entry->linkNameLen, val);
            return;
        }
        /* Replace existing value */
        if (entry->value) hostFreeObj(entry->value);
        entry->value = val;
    } else {
        /* Create new entry */
        entry = malloc(sizeof(VarEntry));
        if (!entry) return;

        entry->name = malloc(len + 1);
        if (!entry->name) {
            free(entry);
            return;
        }
        memcpy(entry->name, name, len);
        entry->name[len] = '\0';
        entry->nameLen = len;
        entry->value = val;
        entry->linkTable = NULL;
        entry->linkName = NULL;
        entry->linkNameLen = 0;

        /* Insert at head of bucket */
        unsigned int h = hashName(name, len);
        entry->next = table->buckets[h];
        table->buckets[h] = entry;
    }
}

/* Unset variable */
void hostVarUnset(void *vars, const char *name, size_t len) {
    VarTable *table = vars;
    if (!table) return;

    unsigned int h = hashName(name, len);
    VarEntry **pp = &table->buckets[h];

    while (*pp) {
        VarEntry *entry = *pp;
        if (entry->nameLen == len && memcmp(entry->name, name, len) == 0) {
            *pp = entry->next;
            free(entry->name);
            hostFreeObj(entry->value);
            free(entry);
            return;
        }
        pp = &entry->next;
    }
}

/* Check if variable exists (follows links) */
int hostVarExists(void *vars, const char *name, size_t len) {
    VarTable *table = vars;
    if (!table) return 0;

    VarEntry *entry = findEntry(table, name, len);
    if (!entry) return 0;

    /* Follow link if this is a linked variable */
    if (entry->linkTable) {
        return hostVarExists(entry->linkTable, entry->linkName, entry->linkNameLen);
    }

    return entry->value != NULL;
}

/* Get list of variable names matching pattern (NULL for all) */
TclObj *hostVarNames(void *vars, const char *pattern) {
    VarTable *table = vars;
    if (!table) return hostNewString("", 0);

    /* Count and collect names */
    size_t totalLen = 0;
    int count = 0;

    for (int i = 0; i < VAR_TABLE_SIZE; i++) {
        VarEntry *entry = table->buckets[i];
        while (entry) {
            if (!pattern || pattern[0] == '*') {
                totalLen += entry->nameLen + 1; /* +1 for space */
                count++;
            }
            entry = entry->next;
        }
    }

    if (count == 0) {
        return hostNewString("", 0);
    }

    char *buf = malloc(totalLen);
    if (!buf) return hostNewString("", 0);

    char *p = buf;
    int first = 1;
    for (int i = 0; i < VAR_TABLE_SIZE; i++) {
        VarEntry *entry = table->buckets[i];
        while (entry) {
            if (!pattern || pattern[0] == '*') {
                if (!first) *p++ = ' ';
                memcpy(p, entry->name, entry->nameLen);
                p += entry->nameLen;
                first = 0;
            }
            entry = entry->next;
        }
    }

    TclObj *result = hostNewString(buf, p - buf);
    free(buf);
    return result;
}

/* Link a local variable to another variable */
void hostVarLink(void *localVars, const char *localName, size_t localLen,
                 void *targetVars, const char *targetName, size_t targetLen) {
    VarTable *table = localVars;
    if (!table || !targetVars) return;

    /* Find or create the local entry */
    VarEntry *entry = findEntry(table, localName, localLen);
    if (!entry) {
        /* Create new entry */
        entry = malloc(sizeof(VarEntry));
        if (!entry) return;

        entry->name = malloc(localLen + 1);
        if (!entry->name) {
            free(entry);
            return;
        }
        memcpy(entry->name, localName, localLen);
        entry->name[localLen] = '\0';
        entry->nameLen = localLen;
        entry->value = NULL;
        entry->next = NULL;
        entry->linkTable = NULL;
        entry->linkName = NULL;
        entry->linkNameLen = 0;

        /* Insert at head of bucket */
        unsigned int h = hashName(localName, localLen);
        entry->next = table->buckets[h];
        table->buckets[h] = entry;
    }

    /* Clear any existing value (we're now a link) */
    if (entry->value) {
        hostFreeObj(entry->value);
        entry->value = NULL;
    }

    /* Clear any existing link name */
    if (entry->linkName) {
        free(entry->linkName);
    }

    /* Set up the link */
    entry->linkTable = targetVars;
    entry->linkName = malloc(targetLen + 1);
    if (entry->linkName) {
        memcpy(entry->linkName, targetName, targetLen);
        entry->linkName[targetLen] = '\0';
        entry->linkNameLen = targetLen;
    }
}

/* Array operations - store as varName(key) in same table */

void hostArraySet(void *vars, const char *arr, size_t arrLen,
                  const char *key, size_t keyLen, TclObj *val) {
    /* Build name: arr(key) */
    size_t nameLen = arrLen + 1 + keyLen + 1;
    char *name = malloc(nameLen + 1);
    if (!name) return;

    memcpy(name, arr, arrLen);
    name[arrLen] = '(';
    memcpy(name + arrLen + 1, key, keyLen);
    name[arrLen + 1 + keyLen] = ')';
    name[nameLen] = '\0';

    hostVarSet(vars, name, nameLen, val);
    free(name);
}

TclObj *hostArrayGet(void *vars, const char *arr, size_t arrLen,
                     const char *key, size_t keyLen) {
    /* Build name: arr(key) */
    size_t nameLen = arrLen + 1 + keyLen + 1;
    char *name = malloc(nameLen + 1);
    if (!name) return NULL;

    memcpy(name, arr, arrLen);
    name[arrLen] = '(';
    memcpy(name + arrLen + 1, key, keyLen);
    name[arrLen + 1 + keyLen] = ')';
    name[nameLen] = '\0';

    TclObj *result = hostVarGet(vars, name, nameLen);
    free(name);
    return result;
}

int hostArrayExists(void *vars, const char *arr, size_t arrLen,
                    const char *key, size_t keyLen) {
    size_t nameLen = arrLen + 1 + keyLen + 1;
    char *name = malloc(nameLen + 1);
    if (!name) return 0;

    memcpy(name, arr, arrLen);
    name[arrLen] = '(';
    memcpy(name + arrLen + 1, key, keyLen);
    name[arrLen + 1 + keyLen] = ')';
    name[nameLen] = '\0';

    int result = hostVarExists(vars, name, nameLen);
    free(name);
    return result;
}

TclObj *hostArrayNames(void *vars, const char *arr, size_t arrLen,
                       const char *pattern) {
    VarTable *table = vars;
    if (!table) return hostNewString("", 0);

    /* Find all entries starting with arr( */
    size_t totalLen = 0;
    int count = 0;

    for (int i = 0; i < VAR_TABLE_SIZE; i++) {
        VarEntry *entry = table->buckets[i];
        while (entry) {
            if (entry->nameLen > arrLen + 2 &&
                memcmp(entry->name, arr, arrLen) == 0 &&
                entry->name[arrLen] == '(') {
                /* Extract key length (without parentheses) */
                size_t keyLen = entry->nameLen - arrLen - 2;
                totalLen += keyLen + 1;
                count++;
            }
            entry = entry->next;
        }
    }

    if (count == 0) {
        return hostNewString("", 0);
    }

    char *buf = malloc(totalLen);
    if (!buf) return hostNewString("", 0);

    char *p = buf;
    int first = 1;
    for (int i = 0; i < VAR_TABLE_SIZE; i++) {
        VarEntry *entry = table->buckets[i];
        while (entry) {
            if (entry->nameLen > arrLen + 2 &&
                memcmp(entry->name, arr, arrLen) == 0 &&
                entry->name[arrLen] == '(') {
                if (!first) *p++ = ' ';
                /* Copy key (between parentheses) */
                size_t keyLen = entry->nameLen - arrLen - 2;
                memcpy(p, entry->name + arrLen + 1, keyLen);
                p += keyLen;
                first = 0;
            }
            entry = entry->next;
        }
    }

    TclObj *result = hostNewString(buf, p - buf);
    free(buf);
    (void)pattern; /* TODO: pattern matching */
    return result;
}

void hostArrayUnset(void *vars, const char *arr, size_t arrLen,
                    const char *key, size_t keyLen) {
    size_t nameLen = arrLen + 1 + keyLen + 1;
    char *name = malloc(nameLen + 1);
    if (!name) return;

    memcpy(name, arr, arrLen);
    name[arrLen] = '(';
    memcpy(name + arrLen + 1, key, keyLen);
    name[arrLen + 1 + keyLen] = ')';
    name[nameLen] = '\0';

    hostVarUnset(vars, name, nameLen);
    free(name);
}

size_t hostArraySize(void *vars, const char *arr, size_t arrLen) {
    VarTable *table = vars;
    if (!table) return 0;

    size_t count = 0;
    for (int i = 0; i < VAR_TABLE_SIZE; i++) {
        VarEntry *entry = table->buckets[i];
        while (entry) {
            if (entry->nameLen > arrLen + 2 &&
                memcmp(entry->name, arr, arrLen) == 0 &&
                entry->name[arrLen] == '(') {
                count++;
            }
            entry = entry->next;
        }
    }
    return count;
}
