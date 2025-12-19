/*
 * host_c.c - C wrapper functions for Go host
 *
 * This file contains the TclHost callback table and wrapper functions
 * that call the CGO-exported Go functions.
 */

#include "../../core/tclc.h"
#include "_cgo_export.h"
#include <stdlib.h>
#include <string.h>

// ==================================================================
// C wrapper functions that convert between handles and pointers
// ==================================================================

// Context wrappers
static void* wrapInterpContextNew(void* parentCtx, int safe) {
    return (void*)goInterpContextNew(parentCtx, safe);
}

static void wrapInterpContextFree(void* ctx) {
    goInterpContextFree((GoUintptr)ctx);
}

// Frame wrappers
static TclFrame* wrapFrameAlloc(void* ctx) {
    return (TclFrame*)goFrameAlloc((GoUintptr)ctx);
}

static void wrapFrameFree(void* ctx, TclFrame* frame) {
    goFrameFree((GoUintptr)ctx, (GoUintptr)frame);
}

// Object wrappers - convert between TclObj* and uintptr handles
static TclObj* wrapNewString(const char* s, size_t len) {
    return (TclObj*)(void*)goNewString((char*)s, len);
}

static TclObj* wrapNewInt(int64_t val) {
    return (TclObj*)(void*)goNewInt(val);
}

static TclObj* wrapNewDouble(double val) {
    return (TclObj*)(void*)goNewDouble(val);
}

static TclObj* wrapNewBool(int val) {
    return (TclObj*)(void*)goNewBool(val);
}

static TclObj* wrapNewList(TclObj** elems, size_t count) {
    return (TclObj*)(void*)goNewList((GoUintptr*)elems, count);
}

static TclObj* wrapNewDict(void) {
    return (TclObj*)(void*)goNewDict();
}

static TclObj* wrapDup(TclObj* obj) {
    return (TclObj*)(void*)goDup((GoUintptr)(void*)obj);
}

static const char* wrapGetStringPtr(TclObj* obj, size_t* lenOut) {
    return goGetStringPtr((GoUintptr)(void*)obj, lenOut);
}

static int wrapAsInt(TclObj* obj, int64_t* out) {
    return goAsInt((GoUintptr)(void*)obj, out);
}

static int wrapAsDouble(TclObj* obj, double* out) {
    return goAsDouble((GoUintptr)(void*)obj, out);
}

static int wrapAsBool(TclObj* obj, int* out) {
    return goAsBool((GoUintptr)(void*)obj, out);
}

static int wrapAsList(TclObj* obj, TclObj*** elemsOut, size_t* countOut) {
    GoUintptr* handles = NULL;
    size_t count = 0;
    int ret = goAsList((GoUintptr)(void*)obj, &handles, &count);
    if (ret != 0) {
        *elemsOut = NULL;
        *countOut = 0;
        return ret;
    }
    // Cast handles to TclObj* array (they're the same size on 64-bit)
    *elemsOut = (TclObj**)handles;
    *countOut = count;
    return 0;
}

// List wrappers
static size_t wrapListLength(TclObj* list) {
    return goListLength((GoUintptr)(void*)list);
}

static TclObj* wrapListIndex(TclObj* list, size_t idx) {
    return (TclObj*)(void*)goListIndex((GoUintptr)(void*)list, idx);
}

static TclObj* wrapListRange(TclObj* list, size_t first, size_t last) {
    return wrapNewString("", 0);
}

static TclObj* wrapListSet(TclObj* list, size_t idx, TclObj* val) {
    return NULL;
}

static TclObj* wrapListAppend(TclObj* list, TclObj* elem) {
    return (TclObj*)(void*)goListAppend((GoUintptr)(void*)list, (GoUintptr)(void*)elem);
}

static TclObj* wrapListConcat(TclObj* a, TclObj* b) {
    return NULL;
}

static TclObj* wrapListInsert(TclObj* list, size_t idx, TclObj** elems, size_t count) {
    return NULL;
}

static TclObj* wrapListSort(TclObj* list, int flags) {
    return wrapDup(list);
}

// Dict stubs
static TclObj* wrapDictGet(TclObj* dict, TclObj* key) { return NULL; }
static TclObj* wrapDictSet(TclObj* dict, TclObj* key, TclObj* val) { return NULL; }
static int wrapDictExists(TclObj* dict, TclObj* key) { return 0; }
static TclObj* wrapDictKeys(TclObj* dict, const char* pattern) { return wrapNewString("", 0); }
static TclObj* wrapDictValues(TclObj* dict, const char* pattern) { return wrapNewString("", 0); }
static TclObj* wrapDictRemove(TclObj* dict, TclObj* key) { return NULL; }
static size_t wrapDictSize(TclObj* dict) { return 0; }

// String wrappers
static size_t wrapStringLength(TclObj* str) {
    return goStringLength((GoUintptr)(void*)str);
}

static TclObj* wrapStringIndex(TclObj* str, size_t idx) { return wrapNewString("", 0); }
static TclObj* wrapStringRange(TclObj* str, size_t first, size_t last) { return wrapNewString("", 0); }
static TclObj* wrapStringConcat(TclObj** parts, size_t count) { return wrapNewString("", 0); }

static int wrapStringCompare(TclObj* a, TclObj* b) {
    return goStringCompare((GoUintptr)(void*)a, (GoUintptr)(void*)b);
}

static int wrapStringCompareNocase(TclObj* a, TclObj* b) { return 0; }
static int wrapStringMatch(const char* pattern, TclObj* str, int nocase) { return 0; }
static TclObj* wrapStringToLower(TclObj* str) { return wrapNewString("", 0); }
static TclObj* wrapStringToUpper(TclObj* str) { return wrapNewString("", 0); }
static TclObj* wrapStringTrim(TclObj* str, const char* chars) { return wrapNewString("", 0); }
static TclObj* wrapStringReplace(TclObj* str, size_t first, size_t last, TclObj* rep) { return wrapNewString("", 0); }
static int wrapStringFirst(TclObj* needle, TclObj* haystack, size_t start) { return -1; }
static int wrapStringLast(TclObj* needle, TclObj* haystack, size_t start) { return -1; }

// Arena wrappers
static void* wrapArenaPush(void* ctx) {
    return (void*)goArenaPush((GoUintptr)ctx);
}

static void wrapArenaPop(void* ctx, void* arena) {
    goArenaPop((GoUintptr)ctx, (GoUintptr)arena);
}

static void* wrapArenaAlloc(void* arena, size_t size, size_t align) {
    return goArenaAlloc((GoUintptr)arena, size, align);
}

static char* wrapArenaStrdup(void* arena, const char* s, size_t len) {
    return goArenaStrdup((GoUintptr)arena, (char*)s, len);
}

static size_t wrapArenaMark(void* arena) {
    return goArenaMark((GoUintptr)arena);
}

static void wrapArenaReset(void* arena, size_t mark) {
    goArenaReset((GoUintptr)arena, mark);
}

// Variable wrappers
static void* wrapVarsNew(void* ctx) {
    return (void*)goVarsNew((GoUintptr)ctx);
}

static void wrapVarsFree(void* ctx, void* vars) {
    goVarsFree((GoUintptr)ctx, (GoUintptr)vars);
}

static TclObj* wrapVarGet(void* vars, const char* name, size_t len) {
    return (TclObj*)(void*)goVarGet((GoUintptr)vars, (char*)name, len);
}

static void wrapVarSet(void* vars, const char* name, size_t len, TclObj* val) {
    goVarSet((GoUintptr)vars, (char*)name, len, (GoUintptr)(void*)val);
}

static void wrapVarUnset(void* vars, const char* name, size_t len) {
    goVarUnset((GoUintptr)vars, (char*)name, len);
}

static int wrapVarExists(void* vars, const char* name, size_t len) {
    return goVarExists((GoUintptr)vars, (char*)name, len);
}

static TclObj* wrapVarNames(void* vars, const char* pattern) {
    return (TclObj*)(void*)goVarNames((GoUintptr)vars, (char*)pattern);
}

static void wrapVarLink(void* localVars, const char* localName, size_t localLen,
                        void* targetVars, const char* targetName, size_t targetLen) {
    goVarLink((GoUintptr)localVars, (char*)localName, localLen,
              (GoUintptr)targetVars, (char*)targetName, targetLen);
}

// Array wrappers
static void wrapArraySet(void* vars, const char* arr, size_t arrLen,
                         const char* key, size_t keyLen, TclObj* val) {
    goArraySet((GoUintptr)vars, (char*)arr, arrLen, (char*)key, keyLen, (GoUintptr)(void*)val);
}

static TclObj* wrapArrayGet(void* vars, const char* arr, size_t arrLen,
                            const char* key, size_t keyLen) {
    return (TclObj*)(void*)goArrayGet((GoUintptr)vars, (char*)arr, arrLen, (char*)key, keyLen);
}

static int wrapArrayExists(void* vars, const char* arr, size_t arrLen,
                           const char* key, size_t keyLen) {
    return goArrayExists((GoUintptr)vars, (char*)arr, arrLen, (char*)key, keyLen);
}

static TclObj* wrapArrayNames(void* vars, const char* arr, size_t arrLen,
                              const char* pattern) {
    return (TclObj*)(void*)goArrayNames((GoUintptr)vars, (char*)arr, arrLen, (char*)pattern);
}

static void wrapArrayUnset(void* vars, const char* arr, size_t arrLen,
                           const char* key, size_t keyLen) {
    goArrayUnset((GoUintptr)vars, (char*)arr, arrLen, (char*)key, keyLen);
}

static size_t wrapArraySize(void* vars, const char* arr, size_t arrLen) {
    return goArraySize((GoUintptr)vars, (char*)arr, arrLen);
}

// Trace stubs
static void wrapTraceVarAdd(void* vars, const char* name, size_t len, int ops,
                            TclTraceProc callback, void* clientData) {}
static void wrapTraceVarRemove(void* vars, const char* name, size_t len,
                               TclTraceProc callback, void* clientData) {}

// Command wrappers
static int wrapCmdLookup(void* ctx, const char* name, size_t len, TclCmdInfo* out) {
    return goCmdLookup((GoUintptr)ctx, (char*)name, len, out);
}

static void* wrapProcRegister(void* ctx, const char* name, size_t len,
                              TclObj* argList, TclObj* body) {
    return (void*)goProcRegister((GoUintptr)ctx, (char*)name, len,
                                 (GoUintptr)(void*)argList, (GoUintptr)(void*)body);
}

static int wrapProcGetDef(void* handle, TclObj** argListOut, TclObj** bodyOut) {
    GoUintptr argH, bodyH;
    int ret = goProcGetDef((GoUintptr)handle, &argH, &bodyH);
    *argListOut = (TclObj*)(void*)argH;
    *bodyOut = (TclObj*)(void*)bodyH;
    return ret;
}

static TclResult wrapExtInvoke(TclInterp* interp, void* handle,
                               int objc, TclObj** objv) {
    return TCL_ERROR;
}

static int wrapCmdRename(void* ctx, const char* oldName, size_t oldLen,
                         const char* newName, size_t newLen) { return -1; }
static int wrapCmdDelete(void* ctx, const char* name, size_t len) { return -1; }
static int wrapCmdExists(void* ctx, const char* name, size_t len) { return 0; }
static TclObj* wrapCmdList(void* ctx, const char* pattern) { return wrapNewString("", 0); }
static void wrapCmdHide(void* ctx, const char* name, size_t len) {}
static void wrapCmdExpose(void* ctx, const char* name, size_t len) {}

// Channel wrappers
static TclChannel* wrapChanOpen(void* ctx, const char* name, const char* mode) {
    return (TclChannel*)(void*)goChanOpen((GoUintptr)ctx, (char*)name, (char*)mode);
}

static void wrapChanClose(void* ctx, TclChannel* chan) {
    goChanClose((GoUintptr)ctx, (GoUintptr)(void*)chan);
}

static TclChannel* wrapChanStdin(void* ctx) {
    return (TclChannel*)(void*)goChanStdin((GoUintptr)ctx);
}

static TclChannel* wrapChanStdout(void* ctx) {
    return (TclChannel*)(void*)goChanStdout((GoUintptr)ctx);
}

static TclChannel* wrapChanStderr(void* ctx) {
    return (TclChannel*)(void*)goChanStderr((GoUintptr)ctx);
}

static int wrapChanRead(TclChannel* chan, char* buf, size_t len) {
    return goChanRead((GoUintptr)(void*)chan, buf, len);
}

static int wrapChanWrite(TclChannel* chan, const char* buf, size_t len) {
    return goChanWrite((GoUintptr)(void*)chan, (char*)buf, len);
}

static TclObj* wrapChanGets(TclChannel* chan, int* eofOut) {
    return (TclObj*)(void*)goChanGets((GoUintptr)(void*)chan, eofOut);
}

static int wrapChanFlush(TclChannel* chan) {
    return goChanFlush((GoUintptr)(void*)chan);
}

static int wrapChanSeek(TclChannel* chan, int64_t offset, int whence) {
    return goChanSeek((GoUintptr)(void*)chan, offset, whence);
}

static int64_t wrapChanTell(TclChannel* chan) {
    return goChanTell((GoUintptr)(void*)chan);
}

static int wrapChanEof(TclChannel* chan) {
    return goChanEof((GoUintptr)(void*)chan);
}

static int wrapChanBlocked(TclChannel* chan) {
    return goChanBlocked((GoUintptr)(void*)chan);
}

static int wrapChanConfigure(TclChannel* chan, const char* opt, TclObj* val) {
    return goChanConfigure((GoUintptr)(void*)chan, (char*)opt, (GoUintptr)(void*)val);
}

static TclObj* wrapChanCget(TclChannel* chan, const char* opt) {
    return (TclObj*)(void*)goChanCget((GoUintptr)(void*)chan, (char*)opt);
}

static TclObj* wrapChanNames(void* ctx, const char* pattern) {
    return (TclObj*)(void*)goChanNames((GoUintptr)ctx, (char*)pattern);
}

static void wrapChanShare(void* fromCtx, void* toCtx, TclChannel* chan) {}
static void wrapChanTransfer(void* fromCtx, void* toCtx, TclChannel* chan) {}

// Event loop stubs
static TclTimerToken wrapAfterMs(void* ctx, int ms, TclObj* script) { return NULL; }
static TclTimerToken wrapAfterIdle(void* ctx, TclObj* script) { return NULL; }
static void wrapAfterCancel(void* ctx, TclTimerToken token) {}
static TclObj* wrapAfterInfo(void* ctx, TclTimerToken token) { return wrapNewString("", 0); }
static void wrapFileeventSet(void* ctx, TclChannel* chan, int mask, TclObj* script) {}
static TclObj* wrapFileeventGet(void* ctx, TclChannel* chan, int mask) { return NULL; }
static int wrapDoOneEvent(void* ctx, int flags) { return 0; }

// Process stubs
static TclProcess* wrapProcessSpawn(const char** argv, int argc, int flags,
                                    TclChannel** pipeIn, TclChannel** pipeOut,
                                    TclChannel** pipeErr) { return NULL; }
static int wrapProcessWait(TclProcess* proc, int* exitCode) { return -1; }
static int wrapProcessPid(TclProcess* proc) { return -1; }
static void wrapProcessKill(TclProcess* proc, int signal) {}

// Socket stubs
static TclChannel* wrapSocketOpen(const char* host, int port, int flags) { return NULL; }
static void* wrapSocketListen(const char* addr, int port,
                              TclAcceptProc onAccept, void* clientData) { return NULL; }
static void wrapSocketListenClose(void* listener) {}

// Filesystem stubs
static int wrapFileExists(const char* path) { return 0; }
static int wrapFileIsFile(const char* path) { return 0; }
static int wrapFileIsDir(const char* path) { return 0; }
static int wrapFileReadable(const char* path) { return 0; }
static int wrapFileWritable(const char* path) { return 0; }
static int wrapFileExecutable(const char* path) { return 0; }
static int64_t wrapFileSize(const char* path) { return -1; }
static int64_t wrapFileMtime(const char* path) { return -1; }
static int64_t wrapFileAtime(const char* path) { return -1; }
static int wrapFileDelete(const char* path, int force) { return -1; }
static int wrapFileRename(const char* old, const char* new_, int force) { return -1; }
static int wrapFileMkdir(const char* path) { return -1; }
static int wrapFileCopy(const char* src, const char* dst, int force) { return -1; }
static TclObj* wrapFileDirname(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapFileTail(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapFileExtension(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapFileRootname(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapFileJoin(TclObj** parts, size_t count) { return wrapNewString("", 0); }
static TclObj* wrapFileNormalize(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapFileSplit(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapFileType(const char* path) { return wrapNewString("", 0); }
static TclObj* wrapGlob(const char* pattern, int types, const char* dir) { return wrapNewString("", 0); }

// System stubs
static int wrapChdir(const char* path) { return -1; }
static TclObj* wrapGetcwd(void) { return wrapNewString("", 0); }
static TclObj* wrapSysHostname(void) { return wrapNewString("", 0); }
static TclObj* wrapSysExecutable(void) { return wrapNewString("", 0); }
static int wrapSysPid(void) { return 0; }

// Regex stubs
static TclObj* wrapRegexMatch(const char* pat, size_t patLen,
                              TclObj* str, int flags) { return NULL; }
static TclObj* wrapRegexSubst(const char* pat, size_t patLen,
                              TclObj* str, TclObj* rep, int flags) { return NULL; }

// Clock stubs
static int64_t wrapClockSeconds(void) { return 0; }
static int64_t wrapClockMillis(void) { return 0; }
static int64_t wrapClockMicros(void) { return 0; }
static TclObj* wrapClockFormat(int64_t time, const char* fmt, const char* tz) {
    return wrapNewString("", 0);
}
static int64_t wrapClockScan(const char* str, const char* fmt, const char* tz) { return 0; }

// Encoding stubs
static TclObj* wrapEncodingConvertTo(const char* enc, TclObj* str) { return NULL; }
static TclObj* wrapEncodingConvertFrom(const char* enc, TclObj* bytes) { return NULL; }
static TclObj* wrapEncodingNames(void) { return wrapNewString("", 0); }
static const char* wrapEncodingSystem(void) { return "utf-8"; }

// ==================================================================
// The TclHost callback table
// ==================================================================

const TclHost goHost = {
    // Context
    .interpContextNew = wrapInterpContextNew,
    .interpContextFree = wrapInterpContextFree,

    // Frames
    .frameAlloc = wrapFrameAlloc,
    .frameFree = wrapFrameFree,

    // Objects
    .newString = wrapNewString,
    .newInt = wrapNewInt,
    .newDouble = wrapNewDouble,
    .newBool = wrapNewBool,
    .newList = wrapNewList,
    .newDict = wrapNewDict,
    .dup = wrapDup,
    .getStringPtr = wrapGetStringPtr,
    .asInt = wrapAsInt,
    .asDouble = wrapAsDouble,
    .asBool = wrapAsBool,
    .asList = wrapAsList,

    // Lists
    .listLength = wrapListLength,
    .listIndex = wrapListIndex,
    .listRange = wrapListRange,
    .listSet = wrapListSet,
    .listAppend = wrapListAppend,
    .listConcat = wrapListConcat,
    .listInsert = wrapListInsert,
    .listSort = wrapListSort,

    // Dicts
    .dictGet = wrapDictGet,
    .dictSet = wrapDictSet,
    .dictExists = wrapDictExists,
    .dictKeys = wrapDictKeys,
    .dictValues = wrapDictValues,
    .dictRemove = wrapDictRemove,
    .dictSize = wrapDictSize,

    // Strings
    .stringLength = wrapStringLength,
    .stringIndex = wrapStringIndex,
    .stringRange = wrapStringRange,
    .stringConcat = wrapStringConcat,
    .stringCompare = wrapStringCompare,
    .stringCompareNocase = wrapStringCompareNocase,
    .stringMatch = wrapStringMatch,
    .stringToLower = wrapStringToLower,
    .stringToUpper = wrapStringToUpper,
    .stringTrim = wrapStringTrim,
    .stringReplace = wrapStringReplace,
    .stringFirst = wrapStringFirst,
    .stringLast = wrapStringLast,

    // Arena
    .arenaPush = wrapArenaPush,
    .arenaPop = wrapArenaPop,
    .arenaAlloc = wrapArenaAlloc,
    .arenaStrdup = wrapArenaStrdup,
    .arenaMark = wrapArenaMark,
    .arenaReset = wrapArenaReset,

    // Variables
    .varsNew = wrapVarsNew,
    .varsFree = wrapVarsFree,
    .varGet = wrapVarGet,
    .varSet = wrapVarSet,
    .varUnset = wrapVarUnset,
    .varExists = wrapVarExists,
    .varNames = wrapVarNames,
    .varLink = wrapVarLink,

    // Arrays
    .arraySet = wrapArraySet,
    .arrayGet = wrapArrayGet,
    .arrayExists = wrapArrayExists,
    .arrayNames = wrapArrayNames,
    .arrayUnset = wrapArrayUnset,
    .arraySize = wrapArraySize,

    // Traces
    .traceVarAdd = wrapTraceVarAdd,
    .traceVarRemove = wrapTraceVarRemove,

    // Commands
    .cmdLookup = wrapCmdLookup,
    .procRegister = wrapProcRegister,
    .procGetDef = wrapProcGetDef,
    .extInvoke = wrapExtInvoke,
    .cmdRename = wrapCmdRename,
    .cmdDelete = wrapCmdDelete,
    .cmdExists = wrapCmdExists,
    .cmdList = wrapCmdList,
    .cmdHide = wrapCmdHide,
    .cmdExpose = wrapCmdExpose,

    // Channels
    .chanOpen = wrapChanOpen,
    .chanClose = wrapChanClose,
    .chanStdin = wrapChanStdin,
    .chanStdout = wrapChanStdout,
    .chanStderr = wrapChanStderr,
    .chanRead = wrapChanRead,
    .chanWrite = wrapChanWrite,
    .chanGets = wrapChanGets,
    .chanFlush = wrapChanFlush,
    .chanSeek = wrapChanSeek,
    .chanTell = wrapChanTell,
    .chanEof = wrapChanEof,
    .chanBlocked = wrapChanBlocked,
    .chanConfigure = wrapChanConfigure,
    .chanCget = wrapChanCget,
    .chanNames = wrapChanNames,
    .chanShare = wrapChanShare,
    .chanTransfer = wrapChanTransfer,

    // Event loop
    .afterMs = wrapAfterMs,
    .afterIdle = wrapAfterIdle,
    .afterCancel = wrapAfterCancel,
    .afterInfo = wrapAfterInfo,
    .fileeventSet = wrapFileeventSet,
    .fileeventGet = wrapFileeventGet,
    .doOneEvent = wrapDoOneEvent,

    // Process
    .processSpawn = wrapProcessSpawn,
    .processWait = wrapProcessWait,
    .processPid = wrapProcessPid,
    .processKill = wrapProcessKill,

    // Sockets
    .socketOpen = wrapSocketOpen,
    .socketListen = wrapSocketListen,
    .socketListenClose = wrapSocketListenClose,

    // Filesystem
    .fileExists = wrapFileExists,
    .fileIsFile = wrapFileIsFile,
    .fileIsDir = wrapFileIsDir,
    .fileReadable = wrapFileReadable,
    .fileWritable = wrapFileWritable,
    .fileExecutable = wrapFileExecutable,
    .fileSize = wrapFileSize,
    .fileMtime = wrapFileMtime,
    .fileAtime = wrapFileAtime,
    .fileDelete = wrapFileDelete,
    .fileRename = wrapFileRename,
    .fileMkdir = wrapFileMkdir,
    .fileCopy = wrapFileCopy,
    .fileDirname = wrapFileDirname,
    .fileTail = wrapFileTail,
    .fileExtension = wrapFileExtension,
    .fileRootname = wrapFileRootname,
    .fileJoin = wrapFileJoin,
    .fileNormalize = wrapFileNormalize,
    .fileSplit = wrapFileSplit,
    .fileType = wrapFileType,
    .glob = wrapGlob,

    // System
    .chdir = wrapChdir,
    .getcwd = wrapGetcwd,
    .sysHostname = wrapSysHostname,
    .sysExecutable = wrapSysExecutable,
    .sysPid = wrapSysPid,

    // Regex
    .regexMatch = wrapRegexMatch,
    .regexSubst = wrapRegexSubst,

    // Clock
    .clockSeconds = wrapClockSeconds,
    .clockMillis = wrapClockMillis,
    .clockMicros = wrapClockMicros,
    .clockFormat = wrapClockFormat,
    .clockScan = wrapClockScan,

    // Encoding
    .encodingConvertTo = wrapEncodingConvertTo,
    .encodingConvertFrom = wrapEncodingConvertFrom,
    .encodingNames = wrapEncodingNames,
    .encodingSystem = wrapEncodingSystem,
};

const TclHost* tclGetGoHost(void) {
    return &goHost;
}
