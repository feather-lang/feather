/*
 * tclc.h - TCL Core Host Interface
 *
 * This header defines the interface between the TCL core (implemented in C)
 * and the host language (Go). The C core handles parsing, evaluation, and
 * control flow. The host provides memory management, data structures, I/O,
 * and OS services.
 *
 * Design principles:
 *   - C core is allocation-free and stdlib-free (except math.h)
 *   - All dynamic storage managed by host via callbacks
 *   - Host provides GC, so no explicit reference counting in C
 *   - Multiple interpreters supported via context handles
 *   - Coroutines supported via non-recursive eval (trampoline)
 */

#ifndef TCLC_H
#define TCLC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

typedef struct TclObj TclObj;           /* Opaque value handle (host-managed) */
typedef struct TclChannel TclChannel;   /* Opaque I/O channel (host-managed) */
typedef struct TclProcess TclProcess;   /* Opaque subprocess (host-managed) */
typedef struct TclInterp TclInterp;     /* Interpreter state (C-managed) */
typedef struct TclFrame TclFrame;       /* Activation record (host-allocated) */
typedef struct TclHost TclHost;         /* Host callback table */
typedef struct TclCmdInfo TclCmdInfo;   /* Command lookup result */
typedef void *TclTimerToken;            /* Event loop timer handle */

/* ========================================================================
 * Result Codes
 * ======================================================================== */

typedef enum TclResult {
    TCL_OK       = 0,   /* Normal completion */
    TCL_ERROR    = 1,   /* Error occurred */
    TCL_RETURN   = 2,   /* return command */
    TCL_BREAK    = 3,   /* break command */
    TCL_CONTINUE = 4,   /* continue command */
} TclResult;

/* ========================================================================
 * Command Dispatch
 * ======================================================================== */

/* Result of command lookup */
typedef enum TclCmdType {
    TCL_CMD_NOT_FOUND = 0,
    TCL_CMD_BUILTIN,      /* Implemented in C, use builtinId */
    TCL_CMD_PROC,         /* TCL proc, use procHandle */
    TCL_CMD_EXTENSION,    /* Host-implemented, call through extInvoke */
    TCL_CMD_ALIAS,        /* Cross-interpreter alias */
} TclCmdType;

struct TclCmdInfo {
    TclCmdType type;
    union {
        int    builtinId;     /* TCL_CMD_BUILTIN: index into C builtin table */
        void  *procHandle;    /* TCL_CMD_PROC: opaque handle to proc def */
        void  *extHandle;     /* TCL_CMD_EXTENSION: opaque handle for callback */
        void  *aliasHandle;   /* TCL_CMD_ALIAS: alias data */
    } u;
};

/* Command procedure signature (builtins and extensions) */
typedef TclResult (*TclCmdProc)(
    TclInterp *interp,
    void      *clientData,
    int        objc,
    TclObj   **objv
);

/* Socket accept callback */
typedef void (*TclAcceptProc)(
    void       *clientData,
    TclChannel *chan,
    const char *addr,
    int         port
);

/* Variable trace callback */
typedef void (*TclTraceProc)(
    void       *clientData,
    const char *name,
    int         op
);

/* Trace operation flags */
typedef enum TclTraceOp {
    TCL_TRACE_READ   = (1 << 0),
    TCL_TRACE_WRITE  = (1 << 1),
    TCL_TRACE_UNSET  = (1 << 2),
} TclTraceOp;

/* Event loop flags */
typedef enum TclEventFlag {
    TCL_EVENT_FILE   = (1 << 0),
    TCL_EVENT_TIMER  = (1 << 1),
    TCL_EVENT_IDLE   = (1 << 2),
    TCL_EVENT_ALL    = 0xFF,
    TCL_EVENT_NOWAIT = (1 << 8),
} TclEventFlag;

/* Channel event mask */
typedef enum TclChannelMask {
    TCL_READABLE = (1 << 0),
    TCL_WRITABLE = (1 << 1),
} TclChannelMask;

/* Process spawn flags */
typedef enum TclProcessFlag {
    TCL_PROCESS_PIPE_STDIN  = (1 << 0),
    TCL_PROCESS_PIPE_STDOUT = (1 << 1),
    TCL_PROCESS_PIPE_STDERR = (1 << 2),
    TCL_PROCESS_BACKGROUND  = (1 << 3),
} TclProcessFlag;

/* Socket flags */
typedef enum TclSocketFlag {
    TCL_SOCKET_ASYNC = (1 << 0),
} TclSocketFlag;

/* Glob type flags */
typedef enum TclGlobType {
    TCL_GLOB_TYPE_FILE = (1 << 0),
    TCL_GLOB_TYPE_DIR  = (1 << 1),
    TCL_GLOB_TYPE_LINK = (1 << 2),
} TclGlobType;

/* Seek whence */
typedef enum TclSeekWhence {
    TCL_SEEK_SET = 0,
    TCL_SEEK_CUR = 1,
    TCL_SEEK_END = 2,
} TclSeekWhence;

/* Path type (for file pathtype) */
typedef enum TclPathType {
    TCL_PATH_ABSOLUTE = 0,
    TCL_PATH_RELATIVE = 1,
    TCL_PATH_VOLUMERELATIVE = 2,
} TclPathType;

/* Link type (for file link) */
typedef enum TclLinkType {
    TCL_LINK_SYMBOLIC = 0,
    TCL_LINK_HARD = 1,
} TclLinkType;

/* ========================================================================
 * Activation Record (Frame)
 *
 * Host allocates these (for coroutine support - frames outlive arena LIFO).
 * C core defines the structure.
 * ======================================================================== */

typedef enum TclFrameFlag {
    TCL_FRAME_PROC      = (1 << 0),  /* Frame is a proc call */
    TCL_FRAME_GLOBAL    = (1 << 1),  /* Global frame */
    TCL_FRAME_NAMESPACE = (1 << 2),  /* Namespace eval frame */
    TCL_FRAME_COROUTINE = (1 << 3),  /* Coroutine base frame */
} TclFrameFlag;

struct TclFrame {
    TclFrame   *parent;         /* Calling frame (NULL for global) */
    void       *varsHandle;     /* Host-managed variable table */
    void       *nsHandle;       /* Namespace handle (NULL = current) */
    const char *procName;       /* Proc name for stack traces, or NULL */
    TclObj    **invocationObjs; /* Command invocation for info level */
    int         invocationCount;/* Number of invocation objects */
    uint32_t    level;          /* Call depth from global */
    uint32_t    flags;          /* TCL_FRAME_* flags */

    /* Coroutine resume state (valid when frame is suspended) */
    int         resumeCmdIndex; /* Command index to resume at */
    int         resumePhase;    /* Parse phase to resume at */
    void       *resumeState;    /* Opaque parse/eval state */
};

/* ========================================================================
 * Interpreter State
 *
 * Managed by C core. Contains current execution state.
 * ======================================================================== */

struct TclInterp {
    const TclHost *host;        /* Host callback table (shared) */
    void          *hostCtx;     /* Per-interp host context */

    TclFrame      *globalFrame; /* Bottom of frame stack */
    TclFrame      *currentFrame;/* Current execution frame */

    TclObj        *result;      /* Command result / return value */
    TclResult      resultCode;  /* TCL_OK, TCL_ERROR, etc. */

    /* Error information */
    TclObj        *errorInfo;   /* Stack trace */
    TclObj        *errorCode;   /* Machine-readable error code */
    int            errorLine;   /* Line number of error */

    /* Return options (for return -code, -level) */
    int            returnCode;
    int            returnLevel;

    /* Coroutine state */
    void          *currentCoro; /* Current coroutine, or NULL */

    /* Script tracking (for info script) */
    const char    *scriptFile;  /* Current source file, or NULL */
    int            scriptLine;  /* Current line number */

    /* Parent interpreter (for interp create) */
    TclInterp     *parent;
    int            isSafe;      /* Safe interpreter flag */
};

/* ========================================================================
 * Host Callback Interface
 *
 * The host (Go) provides this table of function pointers.
 * All callbacks that access per-interpreter state take a context handle.
 * ======================================================================== */

struct TclHost {

    /* ==================================================================
     * Interpreter Context
     *
     * Each interpreter has an opaque host context. Child interpreters
     * reference their parent's context for channel sharing, aliases, etc.
     * ================================================================== */

    void* (*interpContextNew)(void *parentCtx, int safe);
    void  (*interpContextFree)(void *ctx);

    /* ==================================================================
     * Frame Allocation
     *
     * Frames must outlive LIFO arena ordering (for coroutines).
     * Host allocates from GC-managed memory.
     * ================================================================== */

    TclFrame* (*frameAlloc)(void *ctx);
    void      (*frameFree)(void *ctx, TclFrame *frame);

    /* ==================================================================
     * Objects (Values)
     *
     * All TCL values are host-managed. The host handles:
     *   - String representation caching (shimmering)
     *   - Type conversions
     *   - Garbage collection
     * ================================================================== */

    /* Constructors */
    TclObj* (*newString)(const char *s, size_t len);
    TclObj* (*newInt)(int64_t val);
    TclObj* (*newDouble)(double val);
    TclObj* (*newBool)(int val);
    TclObj* (*newList)(TclObj **elems, size_t count);
    TclObj* (*newDict)(void);
    TclObj* (*dup)(TclObj *obj);

    /* String representation (always available) */
    const char* (*getStringPtr)(TclObj *obj, size_t *lenOut);

    /* Type conversions (may fail, return 0 on success) */
    int (*asInt)(TclObj *obj, int64_t *out);
    int (*asDouble)(TclObj *obj, double *out);
    int (*asBool)(TclObj *obj, int *out);
    int (*asList)(TclObj *obj, TclObj ***elemsOut, size_t *countOut);

    /* ==================================================================
     * List Operations
     * ================================================================== */

    size_t  (*listLength)(TclObj *list);
    TclObj* (*listIndex)(TclObj *list, size_t idx);
    TclObj* (*listRange)(TclObj *list, size_t first, size_t last);
    TclObj* (*listSet)(TclObj *list, size_t idx, TclObj *val);
    TclObj* (*listAppend)(TclObj *list, TclObj *elem);
    TclObj* (*listConcat)(TclObj *a, TclObj *b);
    TclObj* (*listInsert)(TclObj *list, size_t idx, TclObj **elems, size_t count);
    TclObj* (*listSort)(TclObj *list, int flags);  /* flags: 1=decreasing, 2=integer */

    /* ==================================================================
     * Dict Operations
     * ================================================================== */

    TclObj* (*dictGet)(TclObj *dict, TclObj *key);
    TclObj* (*dictSet)(TclObj *dict, TclObj *key, TclObj *val);
    int     (*dictExists)(TclObj *dict, TclObj *key);
    TclObj* (*dictKeys)(TclObj *dict, const char *pattern);
    TclObj* (*dictValues)(TclObj *dict, const char *pattern);
    TclObj* (*dictRemove)(TclObj *dict, TclObj *key);
    size_t  (*dictSize)(TclObj *dict);

    /* ==================================================================
     * String Operations
     *
     * String length/index are in characters (Unicode code points),
     * not bytes. Host handles UTF-8 encoding.
     * ================================================================== */

    size_t  (*stringLength)(TclObj *str);
    TclObj* (*stringIndex)(TclObj *str, size_t idx);
    TclObj* (*stringRange)(TclObj *str, size_t first, size_t last);
    TclObj* (*stringConcat)(TclObj **parts, size_t count);
    int     (*stringCompare)(TclObj *a, TclObj *b);
    int     (*stringCompareNocase)(TclObj *a, TclObj *b);
    int     (*stringMatch)(const char *pattern, TclObj *str, int nocase);
    TclObj* (*stringToLower)(TclObj *str);
    TclObj* (*stringToUpper)(TclObj *str);
    TclObj* (*stringTrim)(TclObj *str, const char *chars);
    TclObj* (*stringReplace)(TclObj *str, size_t first, size_t last, TclObj *rep);
    int     (*stringFirst)(TclObj *needle, TclObj *haystack, size_t start);
    int     (*stringLast)(TclObj *needle, TclObj *haystack, size_t start);

    /* ==================================================================
     * Arena Allocation
     *
     * Arenas provide bump-pointer allocation for parse/eval temporaries.
     * LIFO discipline: must pop in reverse order of push.
     * ================================================================== */

    void*  (*arenaPush)(void *ctx);
    void   (*arenaPop)(void *ctx, void *arena);
    void*  (*arenaAlloc)(void *arena, size_t size, size_t align);
    char*  (*arenaStrdup)(void *arena, const char *s, size_t len);
    size_t (*arenaMark)(void *arena);
    void   (*arenaReset)(void *arena, size_t mark);

    /* ==================================================================
     * Variables (Scalars)
     *
     * Each frame has a varsHandle for local variables.
     * The host special-cases "env" array to route to environment.
     * ================================================================== */

    void*   (*varsNew)(void *ctx);
    void    (*varsFree)(void *ctx, void *vars);
    TclObj* (*varGet)(void *vars, const char *name, size_t len);
    void    (*varSet)(void *vars, const char *name, size_t len, TclObj *val);
    void    (*varUnset)(void *vars, const char *name, size_t len);
    int     (*varExists)(void *vars, const char *name, size_t len);
    TclObj* (*varNames)(void *vars, const char *pattern);
    TclObj* (*varNamesLocal)(void *vars, const char *pattern);  /* Excludes linked vars */
    void    (*varLink)(void *localVars, const char *localName, size_t localLen,
                       void *targetVars, const char *targetName, size_t targetLen);

    /* ==================================================================
     * Arrays
     *
     * TCL arrays are associative: $arr(key) syntax.
     * Distinct from lists and dicts.
     * ================================================================== */

    void    (*arraySet)(void *vars, const char *arr, size_t arrLen,
                        const char *key, size_t keyLen, TclObj *val);
    TclObj* (*arrayGet)(void *vars, const char *arr, size_t arrLen,
                        const char *key, size_t keyLen);
    int     (*arrayExists)(void *vars, const char *arr, size_t arrLen,
                           const char *key, size_t keyLen);
    TclObj* (*arrayNames)(void *vars, const char *arr, size_t arrLen,
                          const char *pattern);
    void    (*arrayUnset)(void *vars, const char *arr, size_t arrLen,
                          const char *key, size_t keyLen);
    size_t  (*arraySize)(void *vars, const char *arr, size_t arrLen);

    /* Array search/iteration */
    TclObj* (*arrayStartSearch)(void *vars, const char *arr, size_t arrLen);
    int     (*arrayAnymore)(const char *searchId);
    TclObj* (*arrayNextElement)(const char *searchId);
    void    (*arrayDoneSearch)(const char *searchId);

    /* ==================================================================
     * Variable Traces
     *
     * For vwait, trace command, etc.
     * ================================================================== */

    void (*traceVarAdd)(void *vars, const char *name, size_t len, int ops,
                        TclTraceProc callback, void *clientData);
    void (*traceVarRemove)(void *vars, const char *name, size_t len,
                           TclTraceProc callback, void *clientData);

    /* ==================================================================
     * Commands
     *
     * Command lookup returns type + handle. C has static builtin table.
     * Host manages procs, extensions, aliases.
     * ================================================================== */

    int       (*cmdLookup)(void *ctx, const char *name, size_t len,
                           TclCmdInfo *out);
    void*     (*procRegister)(void *ctx, const char *name, size_t len,
                              TclObj *argList, TclObj *body);
    int       (*procGetDef)(void *handle, TclObj **argListOut, TclObj **bodyOut);
    TclResult (*extInvoke)(TclInterp *interp, void *handle,
                           int objc, TclObj **objv);
    int       (*cmdRename)(void *ctx, const char *oldName, size_t oldLen,
                           const char *newName, size_t newLen);
    int       (*cmdDelete)(void *ctx, const char *name, size_t len);
    int       (*cmdExists)(void *ctx, const char *name, size_t len);
    TclObj*   (*cmdList)(void *ctx, const char *pattern);
    void      (*cmdHide)(void *ctx, const char *name, size_t len);
    void      (*cmdExpose)(void *ctx, const char *name, size_t len);

    /* ==================================================================
     * Channels (I/O)
     *
     * Abstract I/O handles. Host manages buffering, encoding.
     * Channels can be shared between interpreters.
     * ================================================================== */

    TclChannel* (*chanOpen)(void *ctx, const char *name, const char *mode);
    void        (*chanClose)(void *ctx, TclChannel *chan);
    TclChannel* (*chanStdin)(void *ctx);
    TclChannel* (*chanStdout)(void *ctx);
    TclChannel* (*chanStderr)(void *ctx);
    int         (*chanRead)(TclChannel *chan, char *buf, size_t len);
    int         (*chanWrite)(TclChannel *chan, const char *buf, size_t len);
    TclObj*     (*chanGets)(TclChannel *chan, int *eofOut);
    int         (*chanFlush)(TclChannel *chan);
    int         (*chanSeek)(TclChannel *chan, int64_t offset, int whence);
    int64_t     (*chanTell)(TclChannel *chan);
    int         (*chanEof)(TclChannel *chan);
    int         (*chanBlocked)(TclChannel *chan);
    int         (*chanConfigure)(TclChannel *chan, const char *opt, TclObj *val);
    TclObj*     (*chanCget)(TclChannel *chan, const char *opt);
    TclObj*     (*chanNames)(void *ctx, const char *pattern);
    void        (*chanShare)(void *fromCtx, void *toCtx, TclChannel *chan);
    void        (*chanTransfer)(void *fromCtx, void *toCtx, TclChannel *chan);
    int         (*chanTruncate)(TclChannel *chan, int64_t length);
    int64_t     (*chanCopy)(TclChannel *src, TclChannel *dst, int64_t size);
    int64_t     (*chanPending)(TclChannel *chan, int input);
    int         (*chanPipe)(void *ctx, TclChannel **readChan, TclChannel **writeChan);

    /* ==================================================================
     * Event Loop
     *
     * Timers, channel events, idle callbacks.
     * Host owns the event loop; C commands register handlers.
     * ================================================================== */

    TclTimerToken (*afterMs)(void *ctx, int ms, TclObj *script);
    TclTimerToken (*afterIdle)(void *ctx, TclObj *script);
    void          (*afterCancel)(void *ctx, TclTimerToken token);
    TclObj*       (*afterInfo)(void *ctx, TclTimerToken token);
    void          (*fileeventSet)(void *ctx, TclChannel *chan,
                                  int mask, TclObj *script);
    TclObj*       (*fileeventGet)(void *ctx, TclChannel *chan, int mask);
    int           (*doOneEvent)(void *ctx, int flags);

    /* ==================================================================
     * Subprocess Execution
     * ================================================================== */

    TclProcess* (*processSpawn)(const char **argv, int argc, int flags,
                                TclChannel **pipeIn,
                                TclChannel **pipeOut,
                                TclChannel **pipeErr);
    int   (*processWait)(TclProcess *proc, int *exitCode);
    int   (*processPid)(TclProcess *proc);
    void  (*processKill)(TclProcess *proc, int signal);

    /* ==================================================================
     * Sockets
     * ================================================================== */

    TclChannel* (*socketOpen)(const char *host, int port, int flags);
    void*       (*socketListen)(const char *addr, int port,
                                TclAcceptProc onAccept, void *clientData);
    void        (*socketListenClose)(void *listener);

    /* ==================================================================
     * Filesystem
     * ================================================================== */

    int     (*fileExists)(const char *path);
    int     (*fileIsFile)(const char *path);
    int     (*fileIsDir)(const char *path);
    int     (*fileReadable)(const char *path);
    int     (*fileWritable)(const char *path);
    int     (*fileExecutable)(const char *path);
    int64_t (*fileSize)(const char *path);
    int64_t (*fileMtime)(const char *path);
    int64_t (*fileAtime)(const char *path);
    int     (*fileDelete)(const char *path, int force);
    int     (*fileRename)(const char *oldPath, const char *newPath, int force);
    int     (*fileMkdir)(const char *path);
    int     (*fileCopy)(const char *src, const char *dst, int force);
    TclObj* (*fileDirname)(const char *path);
    TclObj* (*fileTail)(const char *path);
    TclObj* (*fileExtension)(const char *path);
    TclObj* (*fileRootname)(const char *path);
    TclObj* (*fileJoin)(TclObj **parts, size_t count);
    TclObj* (*fileNormalize)(const char *path);
    TclObj* (*fileSplit)(const char *path);
    TclObj* (*fileType)(const char *path);
    TclObj* (*glob)(const char *pattern, int types, const char *directory);
    int     (*filePathtype)(const char *path);  /* Returns TclPathType */
    TclObj* (*fileSeparator)(void);
    TclObj* (*fileStat)(const char *path);      /* Returns dict with atime,ctime,dev,gid,ino,mode,mtime,nlink,size,type,uid */
    TclObj* (*fileLstat)(const char *path);     /* Like fileStat but doesn't follow symlinks */
    TclObj* (*fileNativename)(const char *path);
    int     (*fileOwned)(const char *path);
    TclObj* (*fileTempfile)(void *ctx, const char *template, TclObj **pathOut);  /* Returns channel name, sets *pathOut to file path */
    TclObj* (*fileTempdir)(const char *template);
    TclObj* (*fileHome)(const char *user);      /* NULL for current user */
    int     (*fileLink)(const char *linkName, const char *target, int linkType);  /* linkType: TclLinkType */
    TclObj* (*fileReadlink)(const char *linkName);
    TclObj* (*fileSystem)(const char *path);    /* Returns list: {fstype ?detail?} */
    TclObj* (*fileVolumes)(void);               /* Returns list of volume mount points */
    TclObj* (*fileAttributes)(const char *path, const char *option);  /* NULL option = get all */
    int     (*fileAttributesSet)(const char *path, const char *option, TclObj *value);

    /* ==================================================================
     * System
     * ================================================================== */

    int     (*chdir)(const char *path);
    TclObj* (*getcwd)(void);
    TclObj* (*sysHostname)(void);
    TclObj* (*sysExecutable)(void);
    int     (*sysPid)(void);

    /* ==================================================================
     * Regular Expressions
     *
     * Returns list of match indices, or NULL on no match.
     * ================================================================== */

    TclObj* (*regexMatch)(const char *pattern, size_t patLen,
                          TclObj *str, int flags);
    TclObj* (*regexSubst)(const char *pattern, size_t patLen,
                          TclObj *str, TclObj *replacement, int flags);

    /* ==================================================================
     * Clock / Time
     * ================================================================== */

    int64_t (*clockSeconds)(void);
    int64_t (*clockMillis)(void);
    int64_t (*clockMicros)(void);
    TclObj* (*clockFormat)(int64_t time, const char *fmt, const char *tz);
    int64_t (*clockScan)(const char *str, const char *fmt, const char *tz);

    /* ==================================================================
     * Encoding
     *
     * TCL strings are Unicode. These convert at I/O boundaries.
     * ================================================================== */

    TclObj* (*encodingConvertTo)(const char *encoding, TclObj *str);
    TclObj* (*encodingConvertFrom)(const char *encoding, TclObj *bytes);
    TclObj* (*encodingNames)(void);
    const char* (*encodingSystem)(void);

};

/* ========================================================================
 * C Core API
 *
 * Functions implemented by the C core, called by the host.
 * ======================================================================== */

/* Initialize interpreter with host callbacks */
TclInterp* tclInterpNew(const TclHost *host, void *hostCtx);
void       tclInterpFree(TclInterp *interp);

/* Evaluate script */
TclResult tclEval(TclInterp *interp, TclObj *script);
TclResult tclEvalStr(TclInterp *interp, const char *script, size_t len);

/* Get/set result */
TclObj*   tclGetResult(TclInterp *interp);
void      tclSetResult(TclInterp *interp, TclObj *result);
void      tclSetResultStr(TclInterp *interp, const char *s, size_t len);

/* Error handling */
void      tclSetError(TclInterp *interp, const char *msg, size_t len);
void      tclSetErrorCode(TclInterp *interp, TclObj *code);
void      tclAddErrorInfo(TclInterp *interp, const char *info, size_t len);

/* Coroutine support */
void*     tclCoroCreate(TclInterp *interp, TclObj *name, TclObj *cmd);
TclResult tclCoroResume(TclInterp *interp, void *coro, TclObj *value);
TclResult tclCoroYield(TclInterp *interp, TclObj *value);
void      tclCoroDestroy(TclInterp *interp, void *coro);
int       tclCoroExists(TclInterp *interp, TclObj *name);

/* Child interpreter support */
TclInterp* tclInterpCreateChild(TclInterp *parent, const char *name, int safe);
TclResult  tclInterpEval(TclInterp *child, TclObj *script);
void       tclInterpAlias(TclInterp *child, const char *childCmd,
                          TclInterp *target, const char *targetCmd,
                          int objc, TclObj **prefixObjs);

/* Expression evaluation (for expr command) */
TclResult tclExprEval(TclInterp *interp, TclObj *expr, TclObj **resultOut);
TclResult tclExprBool(TclInterp *interp, TclObj *expr, int *resultOut);

/* Builtin command enumeration (for info commands, rename support) */
int tclBuiltinCount(void);
const char *tclBuiltinName(int index);
int tclBuiltinLookup(const char *name, size_t len);

/* Substitution (for subst command) */
TclResult tclSubst(TclInterp *interp, TclObj *str, int flags, TclObj **resultOut);

typedef enum TclSubstFlag {
    TCL_SUBST_COMMANDS  = (1 << 0),
    TCL_SUBST_VARIABLES = (1 << 1),
    TCL_SUBST_BACKSLASH = (1 << 2),
    TCL_SUBST_ALL       = (TCL_SUBST_COMMANDS | TCL_SUBST_VARIABLES | TCL_SUBST_BACKSLASH),
} TclSubstFlag;

#ifdef __cplusplus
}
#endif

#endif /* TCLC_H */
