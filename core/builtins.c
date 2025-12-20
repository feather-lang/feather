/*
 * builtins.c - TCL Built-in Commands
 *
 * Static table of built-in commands and their implementations.
 */

#include "internal.h"


/* ========================================================================
 * string Command - Implementation in builtin_string.c
 * ======================================================================== */

/* tclCmdString is defined in builtin_string.c */

/* ========================================================================
 * expr Command - Implementation in builtin_expr.c
 * ======================================================================== */

/* tclCmdExpr is defined in builtin_expr.c */



/* ========================================================================
 * Builtin Table
 * ======================================================================== */

/* Sorted alphabetically for binary search */
static const TclBuiltinEntry builtinTable[] = {
    {"after",    tclCmdAfter},
    {"append",   tclCmdAppend},
    {"array",    tclCmdArray},
    {"break",    tclCmdBreak},
    {"catch",    tclCmdCatch},
    {"chan",     tclCmdChan},
    {"close",    tclCmdClose},
    {"continue", tclCmdContinue},
    {"coroutine", tclCmdCoroutine},
    {"dict",     tclCmdDict},
    {"error",    tclCmdError},
    {"exec",     tclCmdExec},
    {"expr",     tclCmdExpr},
    {"file",     tclCmdFile},
    {"fileevent", tclCmdFileevent},
    {"for",      tclCmdFor},
    {"foreach",  tclCmdForeach},
    {"gets",     tclCmdGets},
    {"global",   tclCmdGlobal},
    {"if",       tclCmdIf},
    {"incr",     tclCmdIncr},
    {"info",     tclCmdInfo},
    {"join",     tclCmdJoin},
    {"lappend",  tclCmdLappend},
    {"lassign",  tclCmdLassign},
    {"ledit",    tclCmdLedit},
    {"lindex",   tclCmdLindex},
    {"linsert",  tclCmdLinsert},
    {"list",     tclCmdList},
    {"llength",  tclCmdLlength},
    {"lmap",     tclCmdLmap},
    {"lpop",     tclCmdLpop},
    {"lrange",   tclCmdLrange},
    {"lremove",  tclCmdLremove},
    {"lrepeat",  tclCmdLrepeat},
    {"lreplace", tclCmdLreplace},
    {"lreverse", tclCmdLreverse},
    {"lsearch",  tclCmdLsearch},
    {"lseq",     tclCmdLseq},
    {"lset",     tclCmdLset},
    {"lsort",    tclCmdLsort},
    {"open",     tclCmdOpen},
    {"proc",     tclCmdProc},
    {"puts",     tclCmdPuts},
    {"read",     tclCmdRead},
    {"regexp",   tclCmdRegexp},
    {"regsub",   tclCmdRegsub},
    {"return",   tclCmdReturn},
    {"set",      tclCmdSet},
    {"split",    tclCmdSplit},
    {"string",   tclCmdString},
    {"subst",    tclCmdSubst},
    {"throw",    tclCmdThrow},
    {"try",      tclCmdTry},
    {"unset",    tclCmdUnset},
    {"update",   tclCmdUpdate},
    {"uplevel",  tclCmdUplevel},
    {"upvar",    tclCmdUpvar},
    {"vwait",    tclCmdVwait},
    {"while",    tclCmdWhile},
    {"yield",    tclCmdYield},
    {"yieldto",  tclCmdYieldto},
};

static const int builtinCount = sizeof(builtinTable) / sizeof(builtinTable[0]);

/* Binary search for builtin command */
int tclBuiltinLookup(const char *name, size_t len) {
    int lo = 0;
    int hi = builtinCount - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const char *midName = builtinTable[mid].name;
        size_t midLen = tclStrlen(midName);

        /* Compare */
        int cmp;
        size_t minLen = len < midLen ? len : midLen;
        cmp = tclStrncmp(name, midName, minLen);
        if (cmp == 0) {
            if (len < midLen) cmp = -1;
            else if (len > midLen) cmp = 1;
        }

        if (cmp < 0) {
            hi = mid - 1;
        } else if (cmp > 0) {
            lo = mid + 1;
        } else {
            return mid;
        }
    }

    return -1;  /* Not found */
}

const TclBuiltinEntry *tclBuiltinGet(int index) {
    if (index < 0 || index >= builtinCount) {
        return NULL;
    }
    return &builtinTable[index];
}

int tclBuiltinCount(void) {
    return builtinCount;
}

const char *tclBuiltinName(int index) {
    if (index < 0 || index >= builtinCount) {
        return NULL;
    }
    return builtinTable[index].name;
}
