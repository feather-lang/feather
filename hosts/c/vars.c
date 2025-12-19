/*
 * vars.c - Variable Table Implementation for C Host (GLib version)
 *
 * Uses GHashTable for variable storage.
 */

#include "../../core/tclc.h"
#include <glib.h>
#include <string.h>

/* External object functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);
extern TclObj *hostDup(TclObj *obj);
extern void hostFreeObj(TclObj *obj);
extern const char *hostGetStringPtr(TclObj *obj, size_t *lenOut);

/* Forward declaration */
typedef struct VarTable VarTable;

/* Variable entry - stores either a value or a link */
typedef struct VarEntry {
    TclObj   *value;         /* Variable value (NULL if linked) */
    VarTable *linkTable;     /* Target table (NULL if not linked) */
    gchar    *linkName;      /* Target variable name */
} VarEntry;

/* Variable table using GHashTable */
struct VarTable {
    GHashTable *vars;        /* Maps gchar* -> VarEntry* */
};

/* Free a variable entry */
static void varEntryFree(gpointer data) {
    VarEntry *entry = data;
    if (entry) {
        if (entry->value) hostFreeObj(entry->value);
        g_free(entry->linkName);
        g_free(entry);
    }
}

/* Create new variable table */
void *hostVarsNew(void *ctx) {
    (void)ctx;
    VarTable *table = g_new0(VarTable, 1);
    if (!table) return NULL;

    table->vars = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, varEntryFree);
    return table;
}

/* Free variable table */
void hostVarsFree(void *ctx, void *vars) {
    (void)ctx;
    VarTable *table = vars;
    if (!table) return;

    g_hash_table_destroy(table->vars);
    g_free(table);
}

/* Get variable value (follows links) */
TclObj *hostVarGet(void *vars, const char *name, size_t len) {
    VarTable *table = vars;
    if (!table) return NULL;

    gchar *key = g_strndup(name, len);
    VarEntry *entry = g_hash_table_lookup(table->vars, key);
    g_free(key);

    if (!entry) return NULL;

    /* Follow link if this is a linked variable */
    if (entry->linkTable) {
        return hostVarGet(entry->linkTable, entry->linkName, strlen(entry->linkName));
    }

    return entry->value;
}

/* Set variable value (follows links) */
void hostVarSet(void *vars, const char *name, size_t len, TclObj *val) {
    VarTable *table = vars;
    if (!table) return;

    gchar *key = g_strndup(name, len);
    VarEntry *entry = g_hash_table_lookup(table->vars, key);

    if (entry) {
        /* Follow link if this is a linked variable */
        if (entry->linkTable) {
            g_free(key);
            hostVarSet(entry->linkTable, entry->linkName, strlen(entry->linkName), val);
            return;
        }
        /* Replace existing value */
        if (entry->value) hostFreeObj(entry->value);
        entry->value = val;
        g_free(key);
    } else {
        /* Create new entry */
        entry = g_new0(VarEntry, 1);
        entry->value = val;
        entry->linkTable = NULL;
        entry->linkName = NULL;

        g_hash_table_insert(table->vars, key, entry);
    }
}

/* Unset variable */
void hostVarUnset(void *vars, const char *name, size_t len) {
    VarTable *table = vars;
    if (!table) return;

    gchar *key = g_strndup(name, len);
    g_hash_table_remove(table->vars, key);
    g_free(key);
}

/* Check if variable exists (follows links) */
int hostVarExists(void *vars, const char *name, size_t len) {
    VarTable *table = vars;
    if (!table) return 0;

    gchar *key = g_strndup(name, len);
    VarEntry *entry = g_hash_table_lookup(table->vars, key);
    g_free(key);

    if (!entry) return 0;

    /* Follow link if this is a linked variable */
    if (entry->linkTable) {
        return hostVarExists(entry->linkTable, entry->linkName, strlen(entry->linkName));
    }

    return entry->value != NULL;
}

/* Helper for collecting variable names */
typedef struct {
    GPtrArray *names;
    const gchar *pattern;
    gsize patLen;
    gboolean skipLinked;  /* If TRUE, skip linked variables */
} VarNamesData;

/* Simple glob pattern matching (supports * at end) */
static gboolean varPatternMatch(const gchar *pattern, gsize patLen, const gchar *name) {
    if (!pattern || patLen == 0) return TRUE;  /* NULL/empty matches all */
    if (patLen == 1 && pattern[0] == '*') return TRUE;  /* "*" matches all */

    /* Check for wildcard at end */
    if (pattern[patLen - 1] == '*') {
        gsize prefixLen = patLen - 1;
        return strncmp(name, pattern, prefixLen) == 0;
    }

    /* Exact match */
    return strcmp(name, pattern) == 0;
}

static void collectVarNames(gpointer key, gpointer value, gpointer userData) {
    VarNamesData *data = userData;
    const gchar *name = key;
    VarEntry *entry = value;

    /* Skip linked variables if requested */
    if (data->skipLinked && entry && entry->linkTable) {
        return;
    }

    if (varPatternMatch(data->pattern, data->patLen, name)) {
        g_ptr_array_add(data->names, g_strdup(name));
    }
}

/* Internal function to get variable names with option to skip linked */
static TclObj *varNamesInternal(void *vars, const char *pattern, gboolean skipLinked) {
    VarTable *table = vars;
    if (!table) return hostNewString("", 0);

    VarNamesData data = {
        .names = g_ptr_array_new_with_free_func(g_free),
        .pattern = pattern,
        .patLen = pattern ? strlen(pattern) : 0,
        .skipLinked = skipLinked
    };

    g_hash_table_foreach(table->vars, collectVarNames, &data);

    if (data.names->len == 0) {
        g_ptr_array_free(data.names, TRUE);
        return hostNewString("", 0);
    }

    /* Build space-separated list */
    GString *result = g_string_new(NULL);
    for (guint i = 0; i < data.names->len; i++) {
        if (i > 0) g_string_append_c(result, ' ');
        g_string_append(result, g_ptr_array_index(data.names, i));
    }

    g_ptr_array_free(data.names, TRUE);

    TclObj *obj = hostNewString(result->str, result->len);
    g_string_free(result, TRUE);
    return obj;
}

/* Get list of variable names matching pattern (NULL for all) */
TclObj *hostVarNames(void *vars, const char *pattern) {
    return varNamesInternal(vars, pattern, FALSE);
}

/* Get list of local variable names (excludes linked variables) */
TclObj *hostVarNamesLocal(void *vars, const char *pattern) {
    return varNamesInternal(vars, pattern, TRUE);
}

/* Link a local variable to another variable */
void hostVarLink(void *localVars, const char *localName, size_t localLen,
                 void *targetVars, const char *targetName, size_t targetLen) {
    VarTable *table = localVars;
    if (!table || !targetVars) return;

    gchar *key = g_strndup(localName, localLen);
    VarEntry *entry = g_hash_table_lookup(table->vars, key);

    if (!entry) {
        /* Create new entry */
        entry = g_new0(VarEntry, 1);
        g_hash_table_insert(table->vars, key, entry);
    } else {
        /* Clear any existing value (we're now a link) */
        if (entry->value) {
            hostFreeObj(entry->value);
            entry->value = NULL;
        }
        g_free(key);
    }

    /* Clear any existing link name */
    g_free(entry->linkName);

    /* Set up the link */
    entry->linkTable = targetVars;
    entry->linkName = g_strndup(targetName, targetLen);
}

/* Array operations - store as varName(key) in same table */

void hostArraySet(void *vars, const char *arr, size_t arrLen,
                  const char *key, size_t keyLen, TclObj *val) {
    /* Build name: arr(key) */
    gchar *name = g_strdup_printf("%.*s(%.*s)", (int)arrLen, arr, (int)keyLen, key);
    hostVarSet(vars, name, strlen(name), val);
    g_free(name);
}

TclObj *hostArrayGet(void *vars, const char *arr, size_t arrLen,
                     const char *key, size_t keyLen) {
    /* Build name: arr(key) */
    gchar *name = g_strdup_printf("%.*s(%.*s)", (int)arrLen, arr, (int)keyLen, key);
    TclObj *result = hostVarGet(vars, name, strlen(name));
    g_free(name);
    return result;
}

int hostArrayExists(void *vars, const char *arr, size_t arrLen,
                    const char *key, size_t keyLen) {
    gchar *name = g_strdup_printf("%.*s(%.*s)", (int)arrLen, arr, (int)keyLen, key);
    int result = hostVarExists(vars, name, strlen(name));
    g_free(name);
    return result;
}

/* Helper for collecting array names */
typedef struct {
    GPtrArray *names;
    const gchar *prefix;
    gsize prefixLen;
} ArrayNamesData;

static void collectArrayNames(gpointer key, gpointer value, gpointer userData) {
    (void)value;
    ArrayNamesData *data = userData;
    const gchar *name = key;
    gsize nameLen = strlen(name);

    /* Check if name starts with prefix( */
    if (nameLen > data->prefixLen + 2 &&
        strncmp(name, data->prefix, data->prefixLen) == 0 &&
        name[data->prefixLen] == '(') {
        /* Extract key (between parentheses) */
        const gchar *keyStart = name + data->prefixLen + 1;
        const gchar *keyEnd = name + nameLen - 1;
        if (*keyEnd == ')') {
            g_ptr_array_add(data->names, g_strndup(keyStart, keyEnd - keyStart));
        }
    }
}

TclObj *hostArrayNames(void *vars, const char *arr, size_t arrLen,
                       const char *pattern) {
    (void)pattern;  /* TODO: pattern matching */
    VarTable *table = vars;
    if (!table) return hostNewString("", 0);

    gchar *prefix = g_strndup(arr, arrLen);
    ArrayNamesData data = {
        .names = g_ptr_array_new_with_free_func(g_free),
        .prefix = prefix,
        .prefixLen = arrLen
    };

    g_hash_table_foreach(table->vars, collectArrayNames, &data);
    g_free(prefix);

    if (data.names->len == 0) {
        g_ptr_array_free(data.names, TRUE);
        return hostNewString("", 0);
    }

    /* Build space-separated list */
    GString *result = g_string_new(NULL);
    for (guint i = 0; i < data.names->len; i++) {
        if (i > 0) g_string_append_c(result, ' ');
        g_string_append(result, g_ptr_array_index(data.names, i));
    }

    g_ptr_array_free(data.names, TRUE);

    TclObj *obj = hostNewString(result->str, result->len);
    g_string_free(result, TRUE);
    return obj;
}

void hostArrayUnset(void *vars, const char *arr, size_t arrLen,
                    const char *key, size_t keyLen) {
    gchar *name = g_strdup_printf("%.*s(%.*s)", (int)arrLen, arr, (int)keyLen, key);
    hostVarUnset(vars, name, strlen(name));
    g_free(name);
}

/* Helper for counting array size */
typedef struct {
    const gchar *prefix;
    gsize prefixLen;
    gsize count;
} ArraySizeData;

static void countArrayElements(gpointer key, gpointer value, gpointer userData) {
    (void)value;
    ArraySizeData *data = userData;
    const gchar *name = key;
    gsize nameLen = strlen(name);

    if (nameLen > data->prefixLen + 2 &&
        strncmp(name, data->prefix, data->prefixLen) == 0 &&
        name[data->prefixLen] == '(') {
        data->count++;
    }
}

size_t hostArraySize(void *vars, const char *arr, size_t arrLen) {
    VarTable *table = vars;
    if (!table) return 0;

    gchar *prefix = g_strndup(arr, arrLen);
    ArraySizeData data = {
        .prefix = prefix,
        .prefixLen = arrLen,
        .count = 0
    };

    g_hash_table_foreach(table->vars, countArrayElements, &data);
    g_free(prefix);

    return data.count;
}
