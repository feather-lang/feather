/*
 * internal.h - TCL Core Internal Declarations
 *
 * This header defines types and functions shared between TCL core modules.
 * It is NOT part of the public API - use tclc.h for the host interface.
 */

#ifndef TCLC_INTERNAL_H
#define TCLC_INTERNAL_H

#include "tclc.h"

/* ========================================================================
 * Word Types and Token Representation
 * ======================================================================== */

typedef enum TclWordType {
    TCL_WORD_BARE,      /* Unquoted word */
    TCL_WORD_BRACES,    /* {braced} - no substitution */
    TCL_WORD_QUOTES,    /* "quoted" - substitution enabled */
} TclWordType;

/* A single word/token in a command */
typedef struct TclWord {
    const char  *start;     /* Pointer into source buffer */
    size_t       len;       /* Length in bytes */
    TclWordType  type;      /* Quoting type */
    int          line;      /* Source line number */
} TclWord;

/* A parsed command (array of words) */
typedef struct TclParsedCmd {
    TclWord    *words;      /* Array of words (arena-allocated) */
    int         wordCount;  /* Number of words */
    int         lineStart;  /* Line where command starts */
    int         lineEnd;    /* Line where command ends */
} TclParsedCmd;

/* ========================================================================
 * Lexer State
 * ======================================================================== */

typedef struct TclLexer {
    const char *script;     /* Full script being parsed */
    const char *pos;        /* Current position */
    const char *end;        /* End of script */
    int         line;       /* Current line number */
} TclLexer;

/* ========================================================================
 * Parser State
 * ======================================================================== */

typedef struct TclParser {
    TclLexer    lex;        /* Embedded lexer */
    TclInterp  *interp;     /* Interpreter for error reporting */
    void       *arena;      /* Arena for temporary allocations */
    size_t      arenaMark;  /* Mark for resetting arena */
} TclParser;

/* ========================================================================
 * Substitution Flags (use values from tclc.h)
 * ======================================================================== */

/* Use the TclSubstFlag enum from tclc.h:
 * TCL_SUBST_COMMANDS  (1 << 0)
 * TCL_SUBST_VARIABLES (1 << 1)
 * TCL_SUBST_BACKSLASH (1 << 2)
 * TCL_SUBST_ALL       (TCL_SUBST_COMMANDS | TCL_SUBST_VARIABLES | TCL_SUBST_BACKSLASH)
 */

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

struct TclAstNode;

/* ========================================================================
 * Builtin Command Entry
 * ======================================================================== */

typedef TclResult (*TclBuiltinProc)(TclInterp *interp, int objc, TclObj **objv);

typedef struct TclBuiltinEntry {
    const char     *name;
    TclBuiltinProc  proc;
} TclBuiltinEntry;

/* ========================================================================
 * Lexer Functions (lexer.c)
 * ======================================================================== */

/* Initialize lexer state */
void tclLexerInit(TclLexer *lex, const char *script, size_t len);

/* Skip whitespace (not newlines), handle backslash-newline */
void tclLexerSkipSpace(TclLexer *lex);

/* Skip to end of line (for comments) */
void tclLexerSkipLine(TclLexer *lex);

/* Check if at command terminator (newline, semicolon, EOF) */
int tclLexerAtCommandEnd(TclLexer *lex);

/* Check if at comment start (# at command start) */
int tclLexerAtComment(TclLexer *lex);

/* Check if at end of input */
int tclLexerAtEnd(TclLexer *lex);

/* Parse a single word, returns 0 on success, -1 on error */
int tclLexerNextWord(TclLexer *lex, TclWord *word, TclInterp *interp);

/* ========================================================================
 * Parser Functions (parser.c)
 * ======================================================================== */

/* Initialize parser state */
void tclParserInit(TclParser *parser, TclInterp *interp,
                   const char *script, size_t len);

/* Clean up parser state */
void tclParserCleanup(TclParser *parser);

/* Parse next command, returns 0 on success, -1 on error, 1 on EOF */
int tclParserNextCommand(TclParser *parser, TclParsedCmd *cmd);

/* ========================================================================
 * Substitution Functions (subst.c)
 * ======================================================================== */

/* Perform substitution on a word, returns substituted TclObj */
TclObj *tclSubstWord(TclInterp *interp, TclWord *word, int flags);

/* Perform substitution on raw string */
TclObj *tclSubstString(TclInterp *interp, const char *str, size_t len, int flags);

/* Process backslash escape, returns number of input chars consumed */
int tclSubstBackslashChar(const char *src, const char *end, char *out);

/* ========================================================================
 * Eval Functions (eval.c)
 * ======================================================================== */

/* Execute a complete script (flags: TCL_EVAL_GLOBAL) */
TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len, int flags);

/* Execute pre-parsed command arguments (flags: TCL_EVAL_GLOBAL) */
TclResult tclEvalObjv(TclInterp *interp, int objc, TclObj **objv, int flags);

/* Execute command substitution [cmd] - always in current scope */
TclResult tclEvalBracketed(TclInterp *interp, const char *cmd, size_t len);

/* ========================================================================
 * Builtin Functions (builtins.c)
 * ======================================================================== */

/* Look up builtin command by name, returns index or -1 */
int tclBuiltinLookup(const char *name, size_t len);

/* Get builtin table entry */
const TclBuiltinEntry *tclBuiltinGet(int index);

/* Get number of builtins */
int tclBuiltinCount(void);

/* I/O commands (builtin_chan.c) */
TclResult tclCmdPuts(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdOpen(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdClose(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdGets(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdRead(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdChan(TclInterp *interp, int objc, TclObj **objv);

TclResult tclCmdString(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdExpr(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdSubst(TclInterp *interp, int objc, TclObj **objv);

/* Scope commands (builtin_global.c, builtin_upvar.c, builtin_uplevel.c) */
TclResult tclCmdGlobal(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdUpvar(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdUplevel(TclInterp *interp, int objc, TclObj **objv);

/* Dict command (builtin_dict.c) */
TclResult tclCmdDict(TclInterp *interp, int objc, TclObj **objv);

/* Exec command (builtin_exec.c) */
TclResult tclCmdExec(TclInterp *interp, int objc, TclObj **objv);

/* File command (builtin_file.c) */
TclResult tclCmdFile(TclInterp *interp, int objc, TclObj **objv);

/* Break/continue commands (builtin_break.c) */
TclResult tclCmdBreak(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdContinue(TclInterp *interp, int objc, TclObj **objv);

/* List commands (builtin_list.c) */
TclResult tclCmdList(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLlength(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLindex(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLrange(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLappend(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLassign(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLedit(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLinsert(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLpop(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLremove(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLrepeat(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLreplace(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLseq(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLset(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdJoin(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdSplit(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLsort(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLreverse(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLsearch(TclInterp *interp, int objc, TclObj **objv);

/* Error handling commands (builtin_error.c) */
TclResult tclCmdError(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdCatch(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdThrow(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdTry(TclInterp *interp, int objc, TclObj **objv);

/* Procedure commands (builtin_proc.c) */
TclResult tclCmdApply(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdProc(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdRename(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdReturn(TclInterp *interp, int objc, TclObj **objv);

/* Variable commands (builtin_var.c) */
TclResult tclCmdSet(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdIncr(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdAppend(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdUnset(TclInterp *interp, int objc, TclObj **objv);

/* Array command (builtin_array.c) */
TclResult tclCmdArray(TclInterp *interp, int objc, TclObj **objv);

/* Info command (builtin_info.c) */
TclResult tclCmdInfo(TclInterp *interp, int objc, TclObj **objv);

/* Control flow commands (builtin_control.c) */
TclResult tclCmdIf(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdWhile(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdFor(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdForeach(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdLmap(TclInterp *interp, int objc, TclObj **objv);

/* Event loop commands (builtin_event.c) */
TclResult tclCmdAfter(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdVwait(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdUpdate(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdFileevent(TclInterp *interp, int objc, TclObj **objv);

/* Regexp commands (builtin_regexp.c) */
TclResult tclCmdRegexp(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdRegsub(TclInterp *interp, int objc, TclObj **objv);

/* Coroutine commands (builtin_coroutine.c) */
TclResult tclCmdCoroutine(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdYield(TclInterp *interp, int objc, TclObj **objv);
TclResult tclCmdYieldto(TclInterp *interp, int objc, TclObj **objv);

/* ========================================================================
 * Loop State for Coroutine Suspend/Resume
 * ======================================================================== */

typedef enum {
    LOOP_PHASE_TEST,      /* Evaluating condition */
    LOOP_PHASE_BODY,      /* Executing body */
    LOOP_PHASE_NEXT,      /* Executing increment (for) */
    LOOP_PHASE_DONE       /* Loop complete */
} TclLoopPhase;

typedef enum {
    LOOP_TYPE_WHILE,
    LOOP_TYPE_FOR,
    LOOP_TYPE_FOREACH
} TclLoopType;

typedef struct TclLoopState {
    TclLoopType     type;
    TclLoopPhase    phase;

    /* Common fields - using TclObj* enables AST caching across yields */
    TclObj         *bodyObj;           /* Loop body script */
    size_t          bodyResumeOffset;  /* Offset into body to resume from after yield */

    /* For while/for: condition */
    TclObj         *testObj;           /* Condition expression */

    /* For for: next clause */
    TclObj         *nextObj;           /* Increment expression */

    /* For foreach: iteration state */
    TclObj        **elems;        /* List elements */
    size_t          elemCount;
    size_t          currentIndex; /* Which element we're on */
    const char     *varName;      /* Variable name */
    size_t          varNameLen;

    /* Link to parent loop (for nested loops) */
    struct TclLoopState *parent;
} TclLoopState;

/* Coroutine support */
typedef struct TclCoroutine TclCoroutine;
TclCoroutine *tclCoroLookup(const char *name, size_t len);
TclResult tclCoroInvoke(TclInterp *interp, TclCoroutine *coro, int objc, TclObj **objv);
const char *tclCoroCurrentName(size_t *lenOut);
int tclCoroYieldPending(void);
void tclCoroClearYield(void);
TclCoroutine *tclCoroGetCurrent(void);

/* Loop state management for coroutines */
TclLoopState *tclCoroLoopPush(TclLoopType type);
void tclCoroLoopPop(void);
TclLoopState *tclCoroLoopCurrent(void);

/* Yield offset tracking for resuming within loop body */
void tclCoroSetYieldOffset(size_t offset);
size_t tclCoroGetYieldOffset(void);

/* ========================================================================
 * Interpreter Functions (from tclc.h, implemented in eval.c)
 * ======================================================================== */

/* These are declared in tclc.h but we note them here for reference:
 *
 * TclInterp* tclInterpNew(const TclHost *host, void *hostCtx);
 * void tclInterpFree(TclInterp *interp);
 * TclResult tclEvalObj(TclInterp *interp, TclObj *script, int flags);
 * TclResult tclEvalObjv(TclInterp *interp, int objc, TclObj **objv, int flags);
 * TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len, int flags);
 * TclObj* tclGetResult(TclInterp *interp);
 * void tclSetResult(TclInterp *interp, TclObj *result);
 */

/* ========================================================================
 * Helper Macros
 * ======================================================================== */

/* String comparison helpers for command lookup */
#define TCL_STREQ(a, alen, b) \
    ((alen) == sizeof(b) - 1 && tclStrncmp((a), (b), (alen)) == 0)

/* Internal string comparison (no libc) */
int tclStrncmp(const char *a, const char *b, size_t n);
size_t tclStrlen(const char *s);

#endif /* TCLC_INTERNAL_H */
