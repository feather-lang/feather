/*
 * host.c - TclHost Callback Table for C Host (GLib version)
 *
 * Assembles all host callbacks into the TclHost structure using GLib.
 */

#include "../../core/tclc.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

/* External functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);
extern TclObj *hostNewInt(int64_t val);
extern TclObj *hostNewDouble(double val);
extern TclObj *hostNewBool(int val);
extern TclObj *hostNewList(TclObj **elems, size_t count);
extern TclObj *hostNewDict(void);
extern TclObj *hostDup(TclObj *obj);
extern void hostFreeObj(TclObj *obj);
extern const char *hostGetStringPtr(TclObj *obj, size_t *lenOut);
extern int hostAsInt(TclObj *obj, int64_t *out);
extern int hostAsDouble(TclObj *obj, double *out);
extern int hostAsBool(TclObj *obj, int *out);
extern int hostAsList(TclObj *obj, TclObj ***elemsOut, size_t *countOut);
extern size_t hostStringLength(TclObj *str);
extern int hostStringCompare(TclObj *a, TclObj *b);
extern size_t hostListLengthImpl(TclObj *list);
extern TclObj *hostListIndexImpl(TclObj *list, size_t idx);

/* External functions from vars.c */
extern void *hostVarsNew(void *ctx);
extern void hostVarsFree(void *ctx, void *vars);
extern TclObj *hostVarGet(void *vars, const char *name, size_t len);
extern void hostVarSet(void *vars, const char *name, size_t len, TclObj *val);
extern void hostVarUnset(void *vars, const char *name, size_t len);
extern int hostVarExists(void *vars, const char *name, size_t len);
extern TclObj *hostVarNames(void *vars, const char *pattern);
extern TclObj *hostVarNamesLocal(void *vars, const char *pattern);
extern void hostVarLink(void *localVars, const char *localName, size_t localLen,
                        void *targetVars, const char *targetName, size_t targetLen);
extern void hostArraySet(void *vars, const char *arr, size_t arrLen,
                         const char *key, size_t keyLen, TclObj *val);
extern TclObj *hostArrayGet(void *vars, const char *arr, size_t arrLen,
                            const char *key, size_t keyLen);
extern int hostArrayExists(void *vars, const char *arr, size_t arrLen,
                           const char *key, size_t keyLen);
extern TclObj *hostArrayNames(void *vars, const char *arr, size_t arrLen,
                              const char *pattern);
extern void hostArrayUnset(void *vars, const char *arr, size_t arrLen,
                           const char *key, size_t keyLen);
extern size_t hostArraySize(void *vars, const char *arr, size_t arrLen);

/* Array search functions */
extern TclObj *hostArrayStartSearch(void *vars, const char *arr, size_t arrLen);
extern int hostArrayAnymore(const char *searchId);
extern TclObj *hostArrayNextElement(const char *searchId);
extern void hostArrayDoneSearch(const char *searchId);

/* External functions from arena.c */
extern void *hostArenaPush(void *ctx);
extern void hostArenaPop(void *ctx, void *arena);
extern void *hostArenaAlloc(void *arena, size_t size, size_t align);
extern char *hostArenaStrdup(void *arena, const char *s, size_t len);
extern size_t hostArenaMark(void *arena);
extern void hostArenaReset(void *arena, size_t mark);

/* External functions from channel.c */
extern TclChannel *hostChanOpen(void *ctx, const char *name, const char *mode);
extern void hostChanClose(void *ctx, TclChannel *chan);
extern TclChannel *hostChanStdin(void *ctx);
extern TclChannel *hostChanStdout(void *ctx);
extern TclChannel *hostChanStderr(void *ctx);
extern int hostChanRead(TclChannel *chan, char *buf, size_t len);
extern int hostChanWrite(TclChannel *chan, const char *buf, size_t len);
extern TclObj *hostChanGets(TclChannel *chan, int *eofOut);
extern int hostChanFlush(TclChannel *chan);
extern int hostChanSeek(TclChannel *chan, int64_t offset, int whence);
extern int64_t hostChanTell(TclChannel *chan);
extern int hostChanEof(TclChannel *chan);
extern int hostChanBlocked(TclChannel *chan);
extern int hostChanConfigure(TclChannel *chan, const char *opt, TclObj *val);
extern TclObj *hostChanCget(TclChannel *chan, const char *opt);
extern TclObj *hostChanNames(void *ctx, const char *pattern);
extern void hostChanShare(void *fromCtx, void *toCtx, TclChannel *chan);
extern void hostChanTransfer(void *fromCtx, void *toCtx, TclChannel *chan);
extern int hostChanTruncate(TclChannel *chan, int64_t length);
extern int64_t hostChanCopy(TclChannel *src, TclChannel *dst, int64_t size);
extern int64_t hostChanPending(TclChannel *chan, int input);
extern int hostChanPipe(void *ctx, TclChannel **readChan, TclChannel **writeChan);
extern TclChannel *hostChanLookup(void *ctx, const char *name);
extern const char *hostChanGetName(TclChannel *chan);

/* ========================================================================
 * Proc Storage
 * ======================================================================== */

typedef struct ProcDef {
    gchar   *name;      /* Procedure name */
    gsize    nameLen;   /* Name length */
    TclObj  *argList;   /* Argument list */
    TclObj  *body;      /* Procedure body */
} ProcDef;

/* ========================================================================
 * Interpreter Context
 * ======================================================================== */

typedef struct HostContext {
    void       *globalVars;   /* Global variable table */
    GHashTable *procs;        /* Procedure definitions: name -> ProcDef* */
} HostContext;

/* Free a proc definition */
static void procDefFree(gpointer data) {
    ProcDef *proc = data;
    if (proc) {
        g_free(proc->name);
        hostFreeObj(proc->argList);
        hostFreeObj(proc->body);
        g_free(proc);
    }
}

static void *hostInterpContextNew(void *parentCtx, int safe) {
    (void)parentCtx;
    (void)safe;

    HostContext *ctx = g_new0(HostContext, 1);
    if (!ctx) return NULL;

    ctx->globalVars = hostVarsNew(ctx);
    ctx->procs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, procDefFree);
    return ctx;
}

static void hostInterpContextFree(void *ctxPtr) {
    HostContext *ctx = ctxPtr;
    if (!ctx) return;

    g_hash_table_destroy(ctx->procs);
    hostVarsFree(ctx, ctx->globalVars);
    g_free(ctx);
}

/* ========================================================================
 * Frame Allocation
 * ======================================================================== */

static TclFrame *hostFrameAlloc(void *ctx) {
    TclFrame *frame = g_new0(TclFrame, 1);
    if (frame) {
        frame->varsHandle = hostVarsNew(ctx);
    }
    return frame;
}

static void hostFrameFree(void *ctx, TclFrame *frame) {
    if (frame) {
        hostVarsFree(ctx, frame->varsHandle);
        g_free(frame);
    }
}

/* ========================================================================
 * Command Lookup - finds procedures registered via proc command
 * ======================================================================== */

static int hostCmdLookup(void *ctxPtr, const char *name, size_t len, TclCmdInfo *out) {
    HostContext *ctx = ctxPtr;

    gchar *key = g_strndup(name, len);
    ProcDef *proc = g_hash_table_lookup(ctx->procs, key);
    g_free(key);

    if (proc) {
        out->type = TCL_CMD_PROC;
        out->u.procHandle = proc;
        return 0;
    }

    out->type = TCL_CMD_NOT_FOUND;
    return 0;
}

static void *hostProcRegister(void *ctxPtr, const char *name, size_t len,
                              TclObj *argList, TclObj *body) {
    HostContext *ctx = ctxPtr;

    gchar *key = g_strndup(name, len);
    ProcDef *existing = g_hash_table_lookup(ctx->procs, key);

    if (existing) {
        /* Replace the existing definition */
        hostFreeObj(existing->argList);
        hostFreeObj(existing->body);
        existing->argList = hostDup(argList);
        existing->body = hostDup(body);
        g_free(key);
        return existing;
    }

    /* Create new proc definition */
    ProcDef *proc = g_new0(ProcDef, 1);
    proc->name = g_strndup(name, len);
    proc->nameLen = len;
    proc->argList = hostDup(argList);
    proc->body = hostDup(body);

    g_hash_table_insert(ctx->procs, key, proc);
    return proc;
}

static int hostProcGetDef(void *handle, TclObj **argListOut, TclObj **bodyOut) {
    ProcDef *proc = handle;
    if (!proc) return -1;

    *argListOut = proc->argList;
    *bodyOut = proc->body;
    return 0;
}

static TclResult hostExtInvoke(TclInterp *interp, void *handle,
                               int objc, TclObj **objv) {
    (void)interp;
    (void)handle;
    (void)objc;
    (void)objv;
    return TCL_ERROR;
}

static int hostCmdRename(void *ctx, const char *oldName, size_t oldLen,
                         const char *newName, size_t newLen) {
    (void)ctx;
    (void)oldName;
    (void)oldLen;
    (void)newName;
    (void)newLen;
    return -1;
}

static int hostCmdDelete(void *ctx, const char *name, size_t len) {
    (void)ctx;
    (void)name;
    (void)len;
    return -1;
}

static int hostCmdExists(void *ctx, const char *name, size_t len) {
    (void)ctx;
    (void)name;
    (void)len;
    return 0;
}

/* Helper struct for command collection */
typedef struct {
    GPtrArray *cmds;
    const gchar *pattern;
    gsize patLen;
} CmdCollectData;

/* Helper for pattern matching (simple glob with * at end) */
static gboolean patternMatch(const gchar *pattern, gsize patLen, const gchar *name) {
    if (!pattern || patLen == 0) return TRUE;  /* NULL/empty matches all */

    /* Check for wildcard at end */
    if (pattern[patLen - 1] == '*') {
        gsize prefixLen = patLen - 1;
        return strncmp(name, pattern, prefixLen) == 0;
    }

    /* Exact match */
    return strcmp(name, pattern) == 0;
}

/* Callback to collect proc names */
static void collectProcName(gpointer key, gpointer value, gpointer userData) {
    (void)value;
    CmdCollectData *data = userData;
    const gchar *name = key;

    if (patternMatch(data->pattern, data->patLen, name)) {
        g_ptr_array_add(data->cmds, hostNewString(name, strlen(name)));
    }
}

static TclObj *hostCmdList(void *ctxPtr, const char *pattern) {
    HostContext *ctx = ctxPtr;

    GPtrArray *cmds = g_ptr_array_new();
    gsize patLen = pattern ? strlen(pattern) : 0;

    /* Add all registered procedures that match pattern */
    CmdCollectData collectData = { cmds, pattern, patLen };
    g_hash_table_foreach(ctx->procs, collectProcName, &collectData);

    /* Add all built-in commands that match pattern */
    int count = tclBuiltinCount();
    for (int i = 0; i < count; i++) {
        const char *name = tclBuiltinName(i);
        if (name && patternMatch(pattern, patLen, name)) {
            g_ptr_array_add(cmds, hostNewString(name, strlen(name)));
        }
    }

    /* Build result list */
    TclObj *result;
    if (cmds->len == 0) {
        result = hostNewString("", 0);
    } else {
        result = hostNewList((TclObj**)cmds->pdata, cmds->len);
    }

    /* Free array (not elements, they're in the result list) */
    g_ptr_array_free(cmds, TRUE);
    return result;
}

static void hostCmdHide(void *ctx, const char *name, size_t len) {
    (void)ctx;
    (void)name;
    (void)len;
}

static void hostCmdExpose(void *ctx, const char *name, size_t len) {
    (void)ctx;
    (void)name;
    (void)len;
}

/* ========================================================================
 * List Operations
 * ======================================================================== */

static size_t hostListLength(TclObj *list) {
    return hostListLengthImpl(list);
}

static TclObj *hostListIndex(TclObj *list, size_t idx) {
    return hostListIndexImpl(list, idx);
}

static TclObj *hostListRange(TclObj *list, size_t first, size_t last) {
    if (!list || first > last) {
        return hostNewString("", 0);
    }

    size_t listLen = hostListLengthImpl(list);
    if (first >= listLen) {
        return hostNewString("", 0);
    }
    if (last >= listLen) {
        last = listLen - 1;
    }

    size_t count = last - first + 1;
    TclObj **elems = g_new(TclObj*, count);
    if (!elems) return hostNewString("", 0);

    for (size_t i = 0; i < count; i++) {
        elems[i] = hostListIndexImpl(list, first + i);
    }

    TclObj *result = hostNewList(elems, count);
    g_free(elems);
    return result;
}

static TclObj *hostListSet(TclObj *list, size_t idx, TclObj *val) {
    (void)list;
    (void)idx;
    (void)val;
    return NULL;
}

static TclObj *hostListAppend(TclObj *list, TclObj *elem) {
    if (!elem) return list ? hostDup(list) : hostNewString("", 0);

    size_t listLen = list ? hostListLengthImpl(list) : 0;
    size_t newCount = listLen + 1;

    TclObj **elems = g_new(TclObj*, newCount);
    if (!elems) return list ? hostDup(list) : hostNewString("", 0);

    /* Copy existing elements */
    for (size_t i = 0; i < listLen; i++) {
        elems[i] = hostListIndexImpl(list, i);
    }
    /* Add new element */
    elems[listLen] = elem;

    TclObj *result = hostNewList(elems, newCount);
    g_free(elems);
    return result;
}

static TclObj *hostListConcat(TclObj *a, TclObj *b) {
    (void)a;
    (void)b;
    return NULL;
}

static TclObj *hostListInsert(TclObj *list, size_t idx, TclObj **elems, size_t count) {
    (void)list;
    (void)idx;
    (void)elems;
    (void)count;
    return NULL;
}

/* Compare function for qsort - ascending string */
static gint cmpStrAsc(gconstpointer a, gconstpointer b) {
    TclObj *oa = *(TclObj **)a;
    TclObj *ob = *(TclObj **)b;
    return hostStringCompare(oa, ob);
}

/* Compare function for qsort - descending string */
static gint cmpStrDesc(gconstpointer a, gconstpointer b) {
    return -cmpStrAsc(a, b);
}

/* Compare function for qsort - ascending string nocase */
static gint cmpStrNocaseAsc(gconstpointer a, gconstpointer b) {
    TclObj *oa = *(TclObj **)a;
    TclObj *ob = *(TclObj **)b;
    size_t lenA, lenB;
    const char *sa = hostGetStringPtr(oa, &lenA);
    const char *sb = hostGetStringPtr(ob, &lenB);
    return g_ascii_strcasecmp(sa, sb);
}

/* Compare function for qsort - descending string nocase */
static gint cmpStrNocaseDesc(gconstpointer a, gconstpointer b) {
    return -cmpStrNocaseAsc(a, b);
}

/* Compare function for qsort - ascending integer */
static gint cmpIntAsc(gconstpointer a, gconstpointer b) {
    TclObj *oa = *(TclObj **)a;
    TclObj *ob = *(TclObj **)b;
    int64_t ia, ib;
    hostAsInt(oa, &ia);
    hostAsInt(ob, &ib);
    if (ia < ib) return -1;
    if (ia > ib) return 1;
    return 0;
}

/* Compare function for qsort - descending integer */
static gint cmpIntDesc(gconstpointer a, gconstpointer b) {
    return -cmpIntAsc(a, b);
}

/* Compare function for qsort - ascending real */
static gint cmpRealAsc(gconstpointer a, gconstpointer b) {
    TclObj *oa = *(TclObj **)a;
    TclObj *ob = *(TclObj **)b;
    double da, db;
    hostAsDouble(oa, &da);
    hostAsDouble(ob, &db);
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/* Compare function for qsort - descending real */
static gint cmpRealDesc(gconstpointer a, gconstpointer b) {
    return -cmpRealAsc(a, b);
}

/* Dictionary comparison - case insensitive with embedded numbers */
static gint dictcmp(const gchar *a, const gchar *b) {
    while (*a && *b) {
        /* Check if both are digits */
        if (g_ascii_isdigit(*a) && g_ascii_isdigit(*b)) {
            /* Compare as numbers */
            glong na = 0, nb = 0;
            while (g_ascii_isdigit(*a)) { na = na * 10 + (*a - '0'); a++; }
            while (g_ascii_isdigit(*b)) { nb = nb * 10 + (*b - '0'); b++; }
            if (na != nb) return (na < nb) ? -1 : 1;
        } else {
            /* Compare as case-insensitive chars */
            gchar ca = g_ascii_tolower(*a);
            gchar cb = g_ascii_tolower(*b);
            if (ca != cb) return (guchar)ca - (guchar)cb;
            a++; b++;
        }
    }
    return (guchar)*a - (guchar)*b;
}

/* Compare function for qsort - ascending dictionary */
static gint cmpDictAsc(gconstpointer a, gconstpointer b) {
    TclObj *oa = *(TclObj **)a;
    TclObj *ob = *(TclObj **)b;
    size_t lenA, lenB;
    const char *sa = hostGetStringPtr(oa, &lenA);
    const char *sb = hostGetStringPtr(ob, &lenB);
    return dictcmp(sa, sb);
}

/* Compare function for qsort - descending dictionary */
static gint cmpDictDesc(gconstpointer a, gconstpointer b) {
    return -cmpDictAsc(a, b);
}

static TclObj *hostListSort(TclObj *list, int flags) {
    if (!list) return hostNewString("", 0);

    size_t listLen = hostListLengthImpl(list);
    if (listLen == 0) return hostNewString("", 0);
    if (listLen == 1) return hostDup(list);

    /* Get all elements */
    TclObj **elems = g_new(TclObj*, listLen);
    if (!elems) return hostDup(list);

    for (size_t i = 0; i < listLen; i++) {
        elems[i] = hostListIndexImpl(list, i);
    }

    /* flags: 1=decreasing, 2=integer, 4=nocase, 8=unique, 16=dictionary, 32=real */
    gboolean decreasing = flags & 1;
    gboolean integer = flags & 2;
    gboolean nocase = flags & 4;
    gboolean unique = flags & 8;
    gboolean dictionary = flags & 16;
    gboolean real = flags & 32;

    /* Select comparison function */
    GCompareFunc cmpfn;
    if (integer) {
        cmpfn = decreasing ? cmpIntDesc : cmpIntAsc;
    } else if (real) {
        cmpfn = decreasing ? cmpRealDesc : cmpRealAsc;
    } else if (dictionary) {
        cmpfn = decreasing ? cmpDictDesc : cmpDictAsc;
    } else if (nocase) {
        cmpfn = decreasing ? cmpStrNocaseDesc : cmpStrNocaseAsc;
    } else {
        cmpfn = decreasing ? cmpStrDesc : cmpStrAsc;
    }

    qsort(elems, listLen, sizeof(TclObj*), cmpfn);

    /* Apply -unique if requested */
    size_t resultLen = listLen;
    if (unique && listLen > 1) {
        /* Remove duplicates in-place - use same comparison for equality */
        size_t writeIdx = 1;
        for (size_t i = 1; i < listLen; i++) {
            gboolean same = FALSE;
            if (integer) {
                int64_t a, b;
                hostAsInt(elems[writeIdx-1], &a);
                hostAsInt(elems[i], &b);
                same = (a == b);
            } else if (real) {
                double a, b;
                hostAsDouble(elems[writeIdx-1], &a);
                hostAsDouble(elems[i], &b);
                same = (a == b);
            } else if (dictionary || nocase) {
                size_t lenA, lenB;
                const char *sa = hostGetStringPtr(elems[writeIdx-1], &lenA);
                const char *sb = hostGetStringPtr(elems[i], &lenB);
                same = (g_ascii_strcasecmp(sa, sb) == 0);
            } else {
                same = (hostStringCompare(elems[writeIdx-1], elems[i]) == 0);
            }
            if (!same) {
                if (writeIdx != i) {
                    hostFreeObj(elems[writeIdx]);
                    elems[writeIdx] = elems[i];
                    elems[i] = NULL;
                }
                writeIdx++;
            } else {
                hostFreeObj(elems[i]);
                elems[i] = NULL;
            }
        }
        resultLen = writeIdx;
    }

    TclObj *result = hostNewList(elems, resultLen);

    /* Free remaining elements */
    for (size_t i = 0; i < listLen; i++) {
        if (elems[i]) hostFreeObj(elems[i]);
    }
    g_free(elems);
    return result;
}

/* ========================================================================
 * Dict Operations (stubs)
 * ======================================================================== */

static TclObj *hostDictGet(TclObj *dict, TclObj *key) {
    (void)dict;
    (void)key;
    return NULL;
}

static TclObj *hostDictSet(TclObj *dict, TclObj *key, TclObj *val) {
    (void)dict;
    (void)key;
    (void)val;
    return NULL;
}

static int hostDictExists(TclObj *dict, TclObj *key) {
    (void)dict;
    (void)key;
    return 0;
}

static TclObj *hostDictKeys(TclObj *dict, const char *pattern) {
    (void)dict;
    (void)pattern;
    return hostNewString("", 0);
}

static TclObj *hostDictValues(TclObj *dict, const char *pattern) {
    (void)dict;
    (void)pattern;
    return hostNewString("", 0);
}

static TclObj *hostDictRemove(TclObj *dict, TclObj *key) {
    (void)dict;
    (void)key;
    return NULL;
}

static size_t hostDictSize(TclObj *dict) {
    (void)dict;
    return 0;
}

/* ========================================================================
 * More String Operations (stubs)
 * ======================================================================== */

static TclObj *hostStringIndex(TclObj *str, size_t idx) {
    (void)str;
    (void)idx;
    return hostNewString("", 0);
}

static TclObj *hostStringRange(TclObj *str, size_t first, size_t last) {
    (void)str;
    (void)first;
    (void)last;
    return hostNewString("", 0);
}

static TclObj *hostStringConcat(TclObj **parts, size_t count) {
    (void)parts;
    (void)count;
    return hostNewString("", 0);
}

static int hostStringCompareNocase(TclObj *a, TclObj *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    size_t lenA, lenB;
    const char *sa = hostGetStringPtr(a, &lenA);
    const char *sb = hostGetStringPtr(b, &lenB);
    return g_ascii_strcasecmp(sa, sb);
}

/* Helper for glob pattern matching */
static gboolean globMatch(const gchar *pat, gsize patLen, const gchar *str, gsize strLen, gboolean nocase) {
    gsize p = 0, s = 0;
    gsize starP = (gsize)-1, starS = (gsize)-1;

    while (s < strLen) {
        if (p < patLen && pat[p] == '*') {
            /* Remember position for backtracking */
            starP = p++;
            starS = s;
        } else if (p < patLen && pat[p] == '?') {
            /* Match any single character */
            p++;
            s++;
        } else if (p < patLen && pat[p] == '[') {
            /* Character class */
            p++;
            gboolean invert = FALSE;
            if (p < patLen && pat[p] == '!') {
                invert = TRUE;
                p++;
            }
            gboolean matched = FALSE;
            gchar sc = nocase ? g_ascii_tolower(str[s]) : str[s];
            while (p < patLen && pat[p] != ']') {
                gchar c1 = nocase ? g_ascii_tolower(pat[p]) : pat[p];
                if (p + 2 < patLen && pat[p + 1] == '-' && pat[p + 2] != ']') {
                    gchar c2 = nocase ? g_ascii_tolower(pat[p + 2]) : pat[p + 2];
                    if (sc >= c1 && sc <= c2) matched = TRUE;
                    p += 3;
                } else {
                    if (sc == c1) matched = TRUE;
                    p++;
                }
            }
            if (p < patLen) p++; /* skip ] */
            if (matched == invert) {
                /* No match, try backtracking */
                if (starP == (gsize)-1) return FALSE;
                p = starP + 1;
                s = ++starS;
            } else {
                s++;
            }
        } else if (p < patLen && pat[p] == '\\' && p + 1 < patLen) {
            /* Escaped character */
            p++;
            gchar pc = pat[p];
            gchar sc = str[s];
            if (nocase) {
                pc = g_ascii_tolower(pc);
                sc = g_ascii_tolower(sc);
            }
            if (pc == sc) {
                p++;
                s++;
            } else if (starP != (gsize)-1) {
                p = starP + 1;
                s = ++starS;
            } else {
                return FALSE;
            }
        } else if (p < patLen) {
            /* Literal character */
            gchar pc = pat[p];
            gchar sc = str[s];
            if (nocase) {
                pc = g_ascii_tolower(pc);
                sc = g_ascii_tolower(sc);
            }
            if (pc == sc) {
                p++;
                s++;
            } else if (starP != (gsize)-1) {
                p = starP + 1;
                s = ++starS;
            } else {
                return FALSE;
            }
        } else if (starP != (gsize)-1) {
            p = starP + 1;
            s = ++starS;
        } else {
            return FALSE;
        }
    }

    /* Skip trailing stars */
    while (p < patLen && pat[p] == '*') p++;

    return p == patLen;
}

static int hostStringMatch(const char *pattern, TclObj *str, int nocase) {
    size_t strLen;
    const char *strPtr = hostGetStringPtr(str, &strLen);
    gsize patLen = strlen(pattern);
    return globMatch(pattern, patLen, strPtr, strLen, nocase);
}

static TclObj *hostStringToLower(TclObj *str) {
    (void)str;
    return hostNewString("", 0);
}

static TclObj *hostStringToUpper(TclObj *str) {
    (void)str;
    return hostNewString("", 0);
}

static TclObj *hostStringTrim(TclObj *str, const char *chars) {
    (void)str;
    (void)chars;
    return hostNewString("", 0);
}

static TclObj *hostStringReplace(TclObj *str, size_t first, size_t last, TclObj *rep) {
    (void)str;
    (void)first;
    (void)last;
    (void)rep;
    return hostNewString("", 0);
}

static int hostStringFirst(TclObj *needle, TclObj *haystack, size_t start) {
    (void)needle;
    (void)haystack;
    (void)start;
    return -1;
}

static int hostStringLast(TclObj *needle, TclObj *haystack, size_t start) {
    (void)needle;
    (void)haystack;
    (void)start;
    return -1;
}

/* ========================================================================
 * Trace Operations (stubs)
 * ======================================================================== */

static void hostTraceVarAdd(void *vars, const char *name, size_t len, int ops,
                            TclTraceProc callback, void *clientData) {
    (void)vars;
    (void)name;
    (void)len;
    (void)ops;
    (void)callback;
    (void)clientData;
}

static void hostTraceVarRemove(void *vars, const char *name, size_t len,
                               TclTraceProc callback, void *clientData) {
    (void)vars;
    (void)name;
    (void)len;
    (void)callback;
    (void)clientData;
}

/* ========================================================================
 * Event Loop (stubs)
 * ======================================================================== */

static TclTimerToken hostAfterMs(void *ctx, int ms, TclObj *script) {
    (void)ctx;
    (void)ms;
    (void)script;
    return NULL;
}

static TclTimerToken hostAfterIdle(void *ctx, TclObj *script) {
    (void)ctx;
    (void)script;
    return NULL;
}

static void hostAfterCancel(void *ctx, TclTimerToken token) {
    (void)ctx;
    (void)token;
}

static TclObj *hostAfterInfo(void *ctx, TclTimerToken token) {
    (void)ctx;
    (void)token;
    return hostNewString("", 0);
}

static void hostFileeventSet(void *ctx, TclChannel *chan, int mask, TclObj *script) {
    (void)ctx;
    (void)chan;
    (void)mask;
    (void)script;
}

static TclObj *hostFileeventGet(void *ctx, TclChannel *chan, int mask) {
    (void)ctx;
    (void)chan;
    (void)mask;
    return NULL;
}

static int hostDoOneEvent(void *ctx, int flags) {
    (void)ctx;
    (void)flags;
    return 0;
}

/* ========================================================================
 * Process/Socket/File stubs
 * ======================================================================== */

/* Process structure */
typedef struct {
    GPid pid;
    gint exit_status;
    gboolean exited;
} HostProcess;

/* External channel creation from channel.c */
extern TclChannel *hostChanFromFd(int fd, gboolean readable, gboolean writable);

static TclProcess *hostProcessSpawn(const char **argv, int argc, int flags,
                                    TclChannel **pipeIn, TclChannel **pipeOut,
                                    TclChannel **pipeErr) {
    (void)argc;

    GError *error = NULL;
    GPid child_pid;
    gint stdin_fd = -1, stdout_fd = -1, stderr_fd = -1;

    GSpawnFlags spawn_flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;

    gboolean success = g_spawn_async_with_pipes(
        NULL,           /* working directory (inherit) */
        (gchar **)argv, /* argv */
        NULL,           /* envp (inherit) */
        spawn_flags,
        NULL,           /* child setup */
        NULL,           /* user data */
        &child_pid,
        (flags & TCL_PROCESS_PIPE_STDIN) ? &stdin_fd : NULL,
        (flags & TCL_PROCESS_PIPE_STDOUT) ? &stdout_fd : NULL,
        (flags & TCL_PROCESS_PIPE_STDERR) ? &stderr_fd : NULL,
        &error
    );

    if (!success) {
        if (error) {
            g_error_free(error);
        }
        return NULL;
    }

    HostProcess *proc = g_new0(HostProcess, 1);
    proc->pid = child_pid;
    proc->exit_status = 0;
    proc->exited = FALSE;

    /* Create pipe channels if requested */
    if (pipeIn && stdin_fd >= 0) {
        *pipeIn = hostChanFromFd(stdin_fd, FALSE, TRUE);
    }

    if (pipeOut && stdout_fd >= 0) {
        *pipeOut = hostChanFromFd(stdout_fd, TRUE, FALSE);
    }

    if (pipeErr && stderr_fd >= 0) {
        *pipeErr = hostChanFromFd(stderr_fd, TRUE, FALSE);
    }

    return (TclProcess *)proc;
}

static int hostProcessWait(TclProcess *tproc, int *exitCode) {
    HostProcess *proc = (HostProcess *)tproc;
    if (!proc) return -1;

    if (proc->exited) {
        if (exitCode) *exitCode = proc->exit_status;
        return 0;
    }

    gint status;
    GPid result = waitpid(proc->pid, &status, 0);
    if (result == proc->pid) {
        proc->exited = TRUE;
        if (WIFEXITED(status)) {
            proc->exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            proc->exit_status = 128 + WTERMSIG(status);
        } else {
            proc->exit_status = -1;
        }
        if (exitCode) *exitCode = proc->exit_status;
        g_spawn_close_pid(proc->pid);
        return 0;
    }

    return -1;
}

static int hostProcessPid(TclProcess *tproc) {
    HostProcess *proc = (HostProcess *)tproc;
    if (!proc) return -1;
    return (int)proc->pid;
}

static void hostProcessKill(TclProcess *tproc, int signal) {
    HostProcess *proc = (HostProcess *)tproc;
    if (!proc || proc->exited) return;
    kill(proc->pid, signal);
}

static TclChannel *hostSocketOpen(const char *host, int port, int flags) {
    (void)host; (void)port; (void)flags;
    return NULL;
}

static void *hostSocketListen(const char *addr, int port,
                              TclAcceptProc onAccept, void *clientData) {
    (void)addr; (void)port; (void)onAccept; (void)clientData;
    return NULL;
}

static void hostSocketListenClose(void *listener) {
    (void)listener;
}

static int hostFileExists(const char *path) {
    return g_file_test(path, G_FILE_TEST_EXISTS) ? 1 : 0;
}

static int hostFileIsFile(const char *path) {
    return g_file_test(path, G_FILE_TEST_IS_REGULAR) ? 1 : 0;
}

static int hostFileIsDir(const char *path) {
    return g_file_test(path, G_FILE_TEST_IS_DIR) ? 1 : 0;
}

static int hostFileReadable(const char *path) {
    return g_access(path, R_OK) == 0 ? 1 : 0;
}

static int hostFileWritable(const char *path) {
    return g_access(path, W_OK) == 0 ? 1 : 0;
}

static int hostFileExecutable(const char *path) {
    return g_access(path, X_OK) == 0 ? 1 : 0;
}

static int64_t hostFileSize(const char *path) {
    GStatBuf buf;
    if (g_stat(path, &buf) != 0) return -1;
    return (int64_t)buf.st_size;
}

static int64_t hostFileMtime(const char *path) {
    GStatBuf buf;
    if (g_stat(path, &buf) != 0) return -1;
    return (int64_t)buf.st_mtime;
}

static int64_t hostFileAtime(const char *path) {
    GStatBuf buf;
    if (g_stat(path, &buf) != 0) return -1;
    return (int64_t)buf.st_atime;
}

static int deleteRecursive(const char *path);

static int hostFileDelete(const char *path, int force) {
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        return force ? 0 : -1;
    }
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        if (force) {
            return deleteRecursive(path);
        }
        return g_rmdir(path) == 0 ? 0 : -1;
    }
    return g_unlink(path) == 0 ? 0 : -1;
}

static int deleteRecursive(const char *path) {
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) {
        return -1;
    }
    const char *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        char *child = g_build_filename(path, name, NULL);
        if (g_file_test(child, G_FILE_TEST_IS_DIR)) {
            deleteRecursive(child);
        } else {
            g_unlink(child);
        }
        g_free(child);
    }
    g_dir_close(dir);
    return g_rmdir(path) == 0 ? 0 : -1;
}

static int hostFileRename(const char *old, const char *new_, int force) {
    if (!force && g_file_test(new_, G_FILE_TEST_EXISTS)) {
        return -1;
    }
    return g_rename(old, new_) == 0 ? 0 : -1;
}

static int hostFileMkdir(const char *path) {
    return g_mkdir_with_parents(path, 0755) == 0 ? 0 : -1;
}

static int hostFileCopy(const char *src, const char *dst, int force) {
    if (!force && g_file_test(dst, G_FILE_TEST_EXISTS)) {
        return -1;
    }
    gchar *contents = NULL;
    gsize len = 0;
    GError *err = NULL;
    if (!g_file_get_contents(src, &contents, &len, &err)) {
        if (err) g_error_free(err);
        return -1;
    }
    gboolean ok = g_file_set_contents(dst, contents, len, &err);
    g_free(contents);
    if (!ok) {
        if (err) g_error_free(err);
        return -1;
    }
    return 0;
}

static TclObj *hostFileDirname(const char *path) {
    gchar *dir = g_path_get_dirname(path);
    TclObj *result = hostNewString(dir, strlen(dir));
    g_free(dir);
    return result;
}

static TclObj *hostFileTail(const char *path) {
    gchar *base = g_path_get_basename(path);
    TclObj *result = hostNewString(base, strlen(base));
    g_free(base);
    return result;
}

static TclObj *hostFileExtension(const char *path) {
    /* Get the base filename first */
    gchar *base = g_path_get_basename(path);
    const char *dot = strrchr(base, '.');
    TclObj *result;
    if (dot) {
        /* Tcl returns everything from the last dot, even for ".bashrc" */
        result = hostNewString(dot, strlen(dot));
    } else {
        result = hostNewString("", 0);
    }
    g_free(base);
    return result;
}

static TclObj *hostFileRootname(const char *path) {
    /* Find last dot in the path (not in directory) */
    const char *base = strrchr(path, '/');
    if (base) base++; else base = path;
    const char *dot = strrchr(base, '.');
    if (dot) {
        /* Return everything before the last dot */
        size_t len = dot - path;
        return hostNewString(path, len);
    }
    return hostNewString(path, strlen(path));
}

static TclObj *hostFileJoin(TclObj **parts, size_t count) {
    if (count == 0) return hostNewString("", 0);

    GString *result = g_string_new("");
    for (size_t i = 0; i < count; i++) {
        size_t len;
        const char *s = hostGetStringPtr(parts[i], &len);

        /* If part is absolute path, start fresh */
        if (len > 0 && s[0] == '/') {
            g_string_truncate(result, 0);
            g_string_append_len(result, s, len);
        } else if (result->len == 0) {
            g_string_append_len(result, s, len);
        } else {
            /* Add separator if needed */
            if (result->str[result->len - 1] != '/') {
                g_string_append_c(result, '/');
            }
            g_string_append_len(result, s, len);
        }
    }

    TclObj *obj = hostNewString(result->str, result->len);
    g_string_free(result, TRUE);
    return obj;
}

static TclObj *hostFileNormalize(const char *path) {
    gchar *abs = g_canonicalize_filename(path, NULL);
    TclObj *result = hostNewString(abs, strlen(abs));
    g_free(abs);
    return result;
}

static TclObj *hostFileSplit(const char *path) {
    GPtrArray *parts = g_ptr_array_new_with_free_func(g_free);

    const char *p = path;

    /* Handle leading / */
    if (*p == '/') {
        g_ptr_array_add(parts, g_strdup("/"));
        p++;
    }

    /* Split by / */
    while (*p) {
        while (*p == '/') p++;  /* Skip consecutive slashes */
        if (!*p) break;

        const char *start = p;
        while (*p && *p != '/') p++;

        g_ptr_array_add(parts, g_strndup(start, p - start));
    }

    /* Build list result */
    TclObj **elems = g_new(TclObj*, parts->len);
    for (guint i = 0; i < parts->len; i++) {
        const char *s = g_ptr_array_index(parts, i);
        elems[i] = hostNewString(s, strlen(s));
    }
    TclObj *result = hostNewList(elems, parts->len);
    g_free(elems);
    g_ptr_array_free(parts, TRUE);
    return result;
}

static TclObj *hostFileType(const char *path) {
    GStatBuf buf;
    if (g_lstat(path, &buf) != 0) return NULL;

    const char *type;
    if (S_ISREG(buf.st_mode)) type = "file";
    else if (S_ISDIR(buf.st_mode)) type = "directory";
    else if (S_ISLNK(buf.st_mode)) type = "link";
    else if (S_ISCHR(buf.st_mode)) type = "characterSpecial";
    else if (S_ISBLK(buf.st_mode)) type = "blockSpecial";
    else if (S_ISFIFO(buf.st_mode)) type = "fifo";
    else if (S_ISSOCK(buf.st_mode)) type = "socket";
    else type = "unknown";

    return hostNewString(type, strlen(type));
}

static TclObj *hostGlob(const char *pattern, int types, const char *dir) {
    (void)pattern; (void)types; (void)dir;
    /* Glob is complex - return empty list for now */
    return hostNewList(NULL, 0);
}

static int hostFilePathtype(const char *path) {
    if (!path || !*path) return TCL_PATH_RELATIVE;
    if (path[0] == '/') return TCL_PATH_ABSOLUTE;
    return TCL_PATH_RELATIVE;
}

static TclObj *hostFileSeparator(void) {
    return hostNewString("/", 1);
}

static TclObj *hostFileStat(const char *path) {
    GStatBuf buf;
    if (g_stat(path, &buf) != 0) return NULL;

    TclObj *dict = hostNewDict();
    extern void hostDictSetInternal(TclObj *dict, const char *key, TclObj *val);
    hostDictSetInternal(dict, "atime", hostNewInt((int64_t)buf.st_atime));
    hostDictSetInternal(dict, "ctime", hostNewInt((int64_t)buf.st_ctime));
    hostDictSetInternal(dict, "dev", hostNewInt((int64_t)buf.st_dev));
    hostDictSetInternal(dict, "gid", hostNewInt((int64_t)buf.st_gid));
    hostDictSetInternal(dict, "ino", hostNewInt((int64_t)buf.st_ino));
    hostDictSetInternal(dict, "mode", hostNewInt((int64_t)buf.st_mode));
    hostDictSetInternal(dict, "mtime", hostNewInt((int64_t)buf.st_mtime));
    hostDictSetInternal(dict, "nlink", hostNewInt((int64_t)buf.st_nlink));
    hostDictSetInternal(dict, "size", hostNewInt((int64_t)buf.st_size));
    hostDictSetInternal(dict, "uid", hostNewInt((int64_t)buf.st_uid));

    const char *type;
    if (S_ISREG(buf.st_mode)) type = "file";
    else if (S_ISDIR(buf.st_mode)) type = "directory";
    else if (S_ISLNK(buf.st_mode)) type = "link";
    else if (S_ISCHR(buf.st_mode)) type = "characterSpecial";
    else if (S_ISBLK(buf.st_mode)) type = "blockSpecial";
    else if (S_ISFIFO(buf.st_mode)) type = "fifo";
    else if (S_ISSOCK(buf.st_mode)) type = "socket";
    else type = "unknown";
    hostDictSetInternal(dict, "type", hostNewString(type, strlen(type)));

    return dict;
}

static TclObj *hostFileLstat(const char *path) {
    GStatBuf buf;
    if (g_lstat(path, &buf) != 0) return NULL;

    TclObj *dict = hostNewDict();
    extern void hostDictSetInternal(TclObj *dict, const char *key, TclObj *val);
    hostDictSetInternal(dict, "atime", hostNewInt((int64_t)buf.st_atime));
    hostDictSetInternal(dict, "ctime", hostNewInt((int64_t)buf.st_ctime));
    hostDictSetInternal(dict, "dev", hostNewInt((int64_t)buf.st_dev));
    hostDictSetInternal(dict, "gid", hostNewInt((int64_t)buf.st_gid));
    hostDictSetInternal(dict, "ino", hostNewInt((int64_t)buf.st_ino));
    hostDictSetInternal(dict, "mode", hostNewInt((int64_t)buf.st_mode));
    hostDictSetInternal(dict, "mtime", hostNewInt((int64_t)buf.st_mtime));
    hostDictSetInternal(dict, "nlink", hostNewInt((int64_t)buf.st_nlink));
    hostDictSetInternal(dict, "size", hostNewInt((int64_t)buf.st_size));
    hostDictSetInternal(dict, "uid", hostNewInt((int64_t)buf.st_uid));

    const char *type;
    if (S_ISREG(buf.st_mode)) type = "file";
    else if (S_ISDIR(buf.st_mode)) type = "directory";
    else if (S_ISLNK(buf.st_mode)) type = "link";
    else if (S_ISCHR(buf.st_mode)) type = "characterSpecial";
    else if (S_ISBLK(buf.st_mode)) type = "blockSpecial";
    else if (S_ISFIFO(buf.st_mode)) type = "fifo";
    else if (S_ISSOCK(buf.st_mode)) type = "socket";
    else type = "unknown";
    hostDictSetInternal(dict, "type", hostNewString(type, strlen(type)));

    return dict;
}

static TclObj *hostFileNativename(const char *path) {
    /* On Unix, native name is the same as the path */
    return hostNewString(path, strlen(path));
}

static int hostFileOwned(const char *path) {
    GStatBuf buf;
    if (g_stat(path, &buf) != 0) return 0;
    return buf.st_uid == getuid() ? 1 : 0;
}

static TclObj *hostFileTempfile(void *ctx, const char *tmpl, TclObj **pathOut) {
    (void)ctx;
    gchar *template;
    if (tmpl && *tmpl) {
        template = g_strdup_printf("%s/XXXXXX", tmpl);
    } else {
        template = g_strdup_printf("%s/tcl_XXXXXX", g_get_tmp_dir());
    }

    int fd = g_mkstemp(template);
    if (fd < 0) {
        g_free(template);
        return NULL;
    }

    /* Create channel from the file descriptor */
    TclChannel *chan = hostChanFromFd(fd, TRUE, TRUE);
    if (!chan) {
        close(fd);
        g_unlink(template);
        g_free(template);
        return NULL;
    }

    /* Set path output */
    if (pathOut) {
        *pathOut = hostNewString(template, strlen(template));
    }

    /* Return channel name */
    const char *chanName = hostChanGetName(chan);
    TclObj *result = hostNewString(chanName, strlen(chanName));
    g_free(template);
    return result;
}

static TclObj *hostFileTempdir(const char *tmpl) {
    gchar *template;
    if (tmpl && *tmpl) {
        template = g_strdup_printf("%s/XXXXXX", tmpl);
    } else {
        template = g_strdup_printf("%s/tclXXXXXX", g_get_tmp_dir());
    }

    gchar *result = g_mkdtemp(template);
    if (!result) {
        g_free(template);
        return NULL;
    }
    TclObj *obj = hostNewString(result, strlen(result));
    g_free(template);
    return obj;
}

static TclObj *hostFileHome(const char *user) {
    if (user && *user) {
        /* Looking up another user's home - not fully supported */
        return NULL;
    }
    const gchar *home = g_get_home_dir();
    if (!home) return NULL;
    return hostNewString(home, strlen(home));
}

static int hostFileLink(const char *linkName, const char *target, int linkType) {
    if (linkType == TCL_LINK_SYMBOLIC) {
        return symlink(target, linkName) == 0 ? 0 : -1;
    } else {
        return link(target, linkName) == 0 ? 0 : -1;
    }
}

static TclObj *hostFileReadlink(const char *linkName) {
    gchar *target = g_file_read_link(linkName, NULL);
    if (!target) return NULL;
    TclObj *result = hostNewString(target, strlen(target));
    g_free(target);
    return result;
}

static TclObj *hostFileSystem(const char *path) {
    (void)path;
    /* Return a simple "native" filesystem type */
    TclObj *elems[1];
    elems[0] = hostNewString("native", 6);
    return hostNewList(elems, 1);
}

static TclObj *hostFileVolumes(void) {
    /* On Unix, there's just root */
    TclObj *elems[1];
    elems[0] = hostNewString("/", 1);
    return hostNewList(elems, 1);
}

static TclObj *hostFileAttributes(const char *path, const char *option) {
    (void)path; (void)option;
    /* File attributes are complex and platform-specific */
    return NULL;
}

static int hostFileAttributesSet(const char *path, const char *option, TclObj *value) {
    (void)path; (void)option; (void)value;
    return -1;
}

static int hostChdir(const char *path) { (void)path; return -1; }
static TclObj *hostGetcwd(void) { return hostNewString("", 0); }
static TclObj *hostSysHostname(void) { return hostNewString("", 0); }
static TclObj *hostSysExecutable(void) { return hostNewString("", 0); }
static int hostSysPid(void) { return 0; }

static TclObj *hostRegexMatch(const char *pat, size_t patLen, TclObj *str, int flags) {
    (void)flags;
    if (!pat || !str) return NULL;

    /* Get string to match */
    size_t strLen;
    const char *strPtr = hostGetStringPtr(str, &strLen);
    if (!strPtr) return NULL;

    /* Create null-terminated pattern */
    gchar *pattern = g_strndup(pat, patLen);
    gchar *subject = g_strndup(strPtr, strLen);

    /* Use GLib regex for matching */
    gboolean matched = g_regex_match_simple(pattern, subject, 0, 0);

    g_free(pattern);
    g_free(subject);

    if (matched) {
        /* Return a simple result indicating match - just return 1 */
        return hostNewInt(1);
    }
    return NULL;
}
static TclObj *hostRegexSubst(const char *pat, size_t patLen, TclObj *str, TclObj *rep, int flags) {
    (void)pat; (void)patLen; (void)str; (void)rep; (void)flags; return NULL;
}

static int64_t hostClockSeconds(void) { return 0; }
static int64_t hostClockMillis(void) { return 0; }
static int64_t hostClockMicros(void) { return 0; }
static TclObj *hostClockFormat(int64_t time, const char *fmt, const char *tz) {
    (void)time; (void)fmt; (void)tz; return hostNewString("", 0);
}
static int64_t hostClockScan(const char *str, const char *fmt, const char *tz) {
    (void)str; (void)fmt; (void)tz; return 0;
}

static TclObj *hostEncodingConvertTo(const char *enc, TclObj *str) {
    (void)enc; (void)str; return NULL;
}
static TclObj *hostEncodingConvertFrom(const char *enc, TclObj *bytes) {
    (void)enc; (void)bytes; return NULL;
}
static TclObj *hostEncodingNames(void) { return hostNewString("", 0); }
static const char *hostEncodingSystem(void) { return "utf-8"; }

/* ========================================================================
 * The TclHost Callback Table
 * ======================================================================== */

const TclHost cHost = {
    /* Context */
    .interpContextNew = hostInterpContextNew,
    .interpContextFree = hostInterpContextFree,

    /* Frames */
    .frameAlloc = hostFrameAlloc,
    .frameFree = hostFrameFree,

    /* Objects */
    .newString = hostNewString,
    .newInt = hostNewInt,
    .newDouble = hostNewDouble,
    .newBool = hostNewBool,
    .newList = hostNewList,
    .newDict = hostNewDict,
    .dup = hostDup,
    .getStringPtr = hostGetStringPtr,
    .asInt = hostAsInt,
    .asDouble = hostAsDouble,
    .asBool = hostAsBool,
    .asList = hostAsList,

    /* Lists */
    .listLength = hostListLength,
    .listIndex = hostListIndex,
    .listRange = hostListRange,
    .listSet = hostListSet,
    .listAppend = hostListAppend,
    .listConcat = hostListConcat,
    .listInsert = hostListInsert,
    .listSort = hostListSort,

    /* Dicts */
    .dictGet = hostDictGet,
    .dictSet = hostDictSet,
    .dictExists = hostDictExists,
    .dictKeys = hostDictKeys,
    .dictValues = hostDictValues,
    .dictRemove = hostDictRemove,
    .dictSize = hostDictSize,

    /* Strings */
    .stringLength = hostStringLength,
    .stringIndex = hostStringIndex,
    .stringRange = hostStringRange,
    .stringConcat = hostStringConcat,
    .stringCompare = hostStringCompare,
    .stringCompareNocase = hostStringCompareNocase,
    .stringMatch = hostStringMatch,
    .stringToLower = hostStringToLower,
    .stringToUpper = hostStringToUpper,
    .stringTrim = hostStringTrim,
    .stringReplace = hostStringReplace,
    .stringFirst = hostStringFirst,
    .stringLast = hostStringLast,

    /* Arena */
    .arenaPush = hostArenaPush,
    .arenaPop = hostArenaPop,
    .arenaAlloc = hostArenaAlloc,
    .arenaStrdup = hostArenaStrdup,
    .arenaMark = hostArenaMark,
    .arenaReset = hostArenaReset,

    /* Variables */
    .varsNew = hostVarsNew,
    .varsFree = hostVarsFree,
    .varGet = hostVarGet,
    .varSet = hostVarSet,
    .varUnset = hostVarUnset,
    .varExists = hostVarExists,
    .varNames = hostVarNames,
    .varNamesLocal = hostVarNamesLocal,
    .varLink = hostVarLink,

    /* Arrays */
    .arraySet = hostArraySet,
    .arrayGet = hostArrayGet,
    .arrayExists = hostArrayExists,
    .arrayNames = hostArrayNames,
    .arrayUnset = hostArrayUnset,
    .arraySize = hostArraySize,
    .arrayStartSearch = hostArrayStartSearch,
    .arrayAnymore = hostArrayAnymore,
    .arrayNextElement = hostArrayNextElement,
    .arrayDoneSearch = hostArrayDoneSearch,

    /* Traces */
    .traceVarAdd = hostTraceVarAdd,
    .traceVarRemove = hostTraceVarRemove,

    /* Commands */
    .cmdLookup = hostCmdLookup,
    .procRegister = hostProcRegister,
    .procGetDef = hostProcGetDef,
    .extInvoke = hostExtInvoke,
    .cmdRename = hostCmdRename,
    .cmdDelete = hostCmdDelete,
    .cmdExists = hostCmdExists,
    .cmdList = hostCmdList,
    .cmdHide = hostCmdHide,
    .cmdExpose = hostCmdExpose,

    /* Channels */
    .chanOpen = hostChanOpen,
    .chanClose = hostChanClose,
    .chanStdin = hostChanStdin,
    .chanStdout = hostChanStdout,
    .chanStderr = hostChanStderr,
    .chanRead = hostChanRead,
    .chanWrite = hostChanWrite,
    .chanGets = hostChanGets,
    .chanFlush = hostChanFlush,
    .chanSeek = hostChanSeek,
    .chanTell = hostChanTell,
    .chanEof = hostChanEof,
    .chanBlocked = hostChanBlocked,
    .chanConfigure = hostChanConfigure,
    .chanCget = hostChanCget,
    .chanNames = hostChanNames,
    .chanShare = hostChanShare,
    .chanTransfer = hostChanTransfer,
    .chanTruncate = hostChanTruncate,
    .chanCopy = hostChanCopy,
    .chanPending = hostChanPending,
    .chanPipe = hostChanPipe,

    /* Event loop */
    .afterMs = hostAfterMs,
    .afterIdle = hostAfterIdle,
    .afterCancel = hostAfterCancel,
    .afterInfo = hostAfterInfo,
    .fileeventSet = hostFileeventSet,
    .fileeventGet = hostFileeventGet,
    .doOneEvent = hostDoOneEvent,

    /* Process */
    .processSpawn = hostProcessSpawn,
    .processWait = hostProcessWait,
    .processPid = hostProcessPid,
    .processKill = hostProcessKill,

    /* Sockets */
    .socketOpen = hostSocketOpen,
    .socketListen = hostSocketListen,
    .socketListenClose = hostSocketListenClose,

    /* Filesystem */
    .fileExists = hostFileExists,
    .fileIsFile = hostFileIsFile,
    .fileIsDir = hostFileIsDir,
    .fileReadable = hostFileReadable,
    .fileWritable = hostFileWritable,
    .fileExecutable = hostFileExecutable,
    .fileSize = hostFileSize,
    .fileMtime = hostFileMtime,
    .fileAtime = hostFileAtime,
    .fileDelete = hostFileDelete,
    .fileRename = hostFileRename,
    .fileMkdir = hostFileMkdir,
    .fileCopy = hostFileCopy,
    .fileDirname = hostFileDirname,
    .fileTail = hostFileTail,
    .fileExtension = hostFileExtension,
    .fileRootname = hostFileRootname,
    .fileJoin = hostFileJoin,
    .fileNormalize = hostFileNormalize,
    .fileSplit = hostFileSplit,
    .fileType = hostFileType,
    .glob = hostGlob,
    .filePathtype = hostFilePathtype,
    .fileSeparator = hostFileSeparator,
    .fileStat = hostFileStat,
    .fileLstat = hostFileLstat,
    .fileNativename = hostFileNativename,
    .fileOwned = hostFileOwned,
    .fileTempfile = hostFileTempfile,
    .fileTempdir = hostFileTempdir,
    .fileHome = hostFileHome,
    .fileLink = hostFileLink,
    .fileReadlink = hostFileReadlink,
    .fileSystem = hostFileSystem,
    .fileVolumes = hostFileVolumes,
    .fileAttributes = hostFileAttributes,
    .fileAttributesSet = hostFileAttributesSet,

    /* System */
    .chdir = hostChdir,
    .getcwd = hostGetcwd,
    .sysHostname = hostSysHostname,
    .sysExecutable = hostSysExecutable,
    .sysPid = hostSysPid,

    /* Regex */
    .regexMatch = hostRegexMatch,
    .regexSubst = hostRegexSubst,

    /* Clock */
    .clockSeconds = hostClockSeconds,
    .clockMillis = hostClockMillis,
    .clockMicros = hostClockMicros,
    .clockFormat = hostClockFormat,
    .clockScan = hostClockScan,

    /* Encoding */
    .encodingConvertTo = hostEncodingConvertTo,
    .encodingConvertFrom = hostEncodingConvertFrom,
    .encodingNames = hostEncodingNames,
    .encodingSystem = hostEncodingSystem,
};

/* Export the host table */
const TclHost *tclGetCHost(void) {
    return &cHost;
}
