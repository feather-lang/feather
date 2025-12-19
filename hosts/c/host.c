/*
 * host.c - TclHost Callback Table for C Host
 *
 * Assembles all host callbacks into the TclHost structure.
 */

#include "../../core/tclc.h"
#include <stdlib.h>
#include <string.h>

/* External functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);
extern TclObj *hostNewInt(int64_t val);
extern TclObj *hostNewDouble(double val);
extern TclObj *hostNewBool(int val);
extern TclObj *hostNewList(TclObj **elems, size_t count);
extern TclObj *hostNewDict(void);
extern TclObj *hostDup(TclObj *obj);
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

/* ========================================================================
 * Proc Storage
 * ======================================================================== */

typedef struct ProcDef {
    char    *name;      /* Procedure name */
    size_t   nameLen;   /* Name length */
    TclObj  *argList;   /* Argument list */
    TclObj  *body;      /* Procedure body */
    struct ProcDef *next;
} ProcDef;

/* ========================================================================
 * Interpreter Context
 * ======================================================================== */

typedef struct HostContext {
    void    *globalVars;   /* Global variable table */
    ProcDef *procs;        /* Linked list of procedures */
} HostContext;

static void *hostInterpContextNew(void *parentCtx, int safe) {
    (void)parentCtx;
    (void)safe;

    HostContext *ctx = calloc(1, sizeof(HostContext));
    if (!ctx) return NULL;

    ctx->globalVars = hostVarsNew(ctx);
    ctx->procs = NULL;
    return ctx;
}

static void hostInterpContextFree(void *ctxPtr) {
    HostContext *ctx = ctxPtr;
    if (!ctx) return;

    /* Free all procedures */
    ProcDef *proc = ctx->procs;
    while (proc) {
        ProcDef *next = proc->next;
        free(proc->name);
        /* Note: argList and body are TclObj - would need proper cleanup */
        free(proc);
        proc = next;
    }

    hostVarsFree(ctx, ctx->globalVars);
    free(ctx);
}

/* ========================================================================
 * Frame Allocation
 * ======================================================================== */

static TclFrame *hostFrameAlloc(void *ctx) {
    (void)ctx;
    TclFrame *frame = calloc(1, sizeof(TclFrame));
    if (frame) {
        frame->varsHandle = hostVarsNew(ctx);
    }
    return frame;
}

static void hostFrameFree(void *ctx, TclFrame *frame) {
    if (frame) {
        hostVarsFree(ctx, frame->varsHandle);
        free(frame);
    }
}

/* ========================================================================
 * Command Lookup - finds procedures registered via proc command
 * ======================================================================== */

static ProcDef *findProc(HostContext *ctx, const char *name, size_t len) {
    ProcDef *proc = ctx->procs;
    while (proc) {
        if (proc->nameLen == len && memcmp(proc->name, name, len) == 0) {
            return proc;
        }
        proc = proc->next;
    }
    return NULL;
}

static int hostCmdLookup(void *ctxPtr, const char *name, size_t len, TclCmdInfo *out) {
    HostContext *ctx = ctxPtr;

    /* Look for a proc with this name */
    ProcDef *proc = findProc(ctx, name, len);
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

    /* Check if proc already exists */
    ProcDef *existing = findProc(ctx, name, len);
    if (existing) {
        /* Replace the existing definition */
        existing->argList = hostDup(argList);
        existing->body = hostDup(body);
        return existing;
    }

    /* Create new proc definition */
    ProcDef *proc = malloc(sizeof(ProcDef));
    if (!proc) return NULL;

    proc->name = malloc(len + 1);
    if (!proc->name) {
        free(proc);
        return NULL;
    }
    memcpy(proc->name, name, len);
    proc->name[len] = '\0';
    proc->nameLen = len;
    proc->argList = hostDup(argList);
    proc->body = hostDup(body);

    /* Add to front of list */
    proc->next = ctx->procs;
    ctx->procs = proc;

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

static TclObj *hostCmdList(void *ctx, const char *pattern) {
    (void)ctx;
    (void)pattern;
    return hostNewString("", 0);
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
    (void)list;
    (void)first;
    (void)last;
    return hostNewString("", 0);
}

static TclObj *hostListSet(TclObj *list, size_t idx, TclObj *val) {
    (void)list;
    (void)idx;
    (void)val;
    return NULL;
}

static TclObj *hostListAppend(TclObj *list, TclObj *elem) {
    (void)list;
    (void)elem;
    return NULL;
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
    (void)a;
    (void)b;
    return 0;
}

static int hostStringMatch(const char *pattern, TclObj *str, int nocase) {
    (void)pattern;
    (void)str;
    (void)nocase;
    return 0;
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

static TclProcess *hostProcessSpawn(const char **argv, int argc, int flags,
                                    TclChannel **pipeIn, TclChannel **pipeOut,
                                    TclChannel **pipeErr) {
    (void)argv; (void)argc; (void)flags;
    (void)pipeIn; (void)pipeOut; (void)pipeErr;
    return NULL;
}

static int hostProcessWait(TclProcess *proc, int *exitCode) {
    (void)proc; (void)exitCode;
    return -1;
}

static int hostProcessPid(TclProcess *proc) {
    (void)proc;
    return -1;
}

static void hostProcessKill(TclProcess *proc, int signal) {
    (void)proc; (void)signal;
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

static int hostFileExists(const char *path) { (void)path; return 0; }
static int hostFileIsFile(const char *path) { (void)path; return 0; }
static int hostFileIsDir(const char *path) { (void)path; return 0; }
static int hostFileReadable(const char *path) { (void)path; return 0; }
static int hostFileWritable(const char *path) { (void)path; return 0; }
static int hostFileExecutable(const char *path) { (void)path; return 0; }
static int64_t hostFileSize(const char *path) { (void)path; return -1; }
static int64_t hostFileMtime(const char *path) { (void)path; return -1; }
static int64_t hostFileAtime(const char *path) { (void)path; return -1; }
static int hostFileDelete(const char *path, int force) { (void)path; (void)force; return -1; }
static int hostFileRename(const char *old, const char *new_, int force) {
    (void)old; (void)new_; (void)force; return -1;
}
static int hostFileMkdir(const char *path) { (void)path; return -1; }
static int hostFileCopy(const char *src, const char *dst, int force) {
    (void)src; (void)dst; (void)force; return -1;
}
static TclObj *hostFileDirname(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostFileTail(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostFileExtension(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostFileRootname(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostFileJoin(TclObj **parts, size_t count) { (void)parts; (void)count; return hostNewString("", 0); }
static TclObj *hostFileNormalize(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostFileSplit(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostFileType(const char *path) { (void)path; return hostNewString("", 0); }
static TclObj *hostGlob(const char *pattern, int types, const char *dir) {
    (void)pattern; (void)types; (void)dir; return hostNewString("", 0);
}

static int hostChdir(const char *path) { (void)path; return -1; }
static TclObj *hostGetcwd(void) { return hostNewString("", 0); }
static TclObj *hostSysHostname(void) { return hostNewString("", 0); }
static TclObj *hostSysExecutable(void) { return hostNewString("", 0); }
static int hostSysPid(void) { return 0; }

static TclObj *hostRegexMatch(const char *pat, size_t patLen, TclObj *str, int flags) {
    (void)pat; (void)patLen; (void)str; (void)flags; return NULL;
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
    .varLink = hostVarLink,

    /* Arrays */
    .arraySet = hostArraySet,
    .arrayGet = hostArrayGet,
    .arrayExists = hostArrayExists,
    .arrayNames = hostArrayNames,
    .arrayUnset = hostArrayUnset,
    .arraySize = hostArraySize,

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
