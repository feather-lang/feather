/*
 * ast.h - TCL Abstract Syntax Tree Definitions
 *
 * Defines the AST node types for representing parsed TCL scripts.
 * The AST enables tree-walking evaluation which properly supports
 * coroutine suspend/resume by saving tree position instead of text offset.
 */

#ifndef TCLC_AST_H
#define TCLC_AST_H

#include <stddef.h>

/* Forward declarations */
struct TclAstNode;
typedef struct TclAstNode TclAstNode;

/* ========================================================================
 * Node Types
 * ======================================================================== */

typedef enum TclNodeType {
    TCL_NODE_SCRIPT,        /* List of commands */
    TCL_NODE_COMMAND,       /* Command with arguments (words) */
    TCL_NODE_WORD,          /* Composite word (may contain substitutions) */
    TCL_NODE_LITERAL,       /* Literal string (no substitution needed) */
    TCL_NODE_VAR_SIMPLE,    /* $var or ${var} - simple variable */
    TCL_NODE_VAR_ARRAY,     /* $arr(index) - array element */
    TCL_NODE_CMD_SUBST,     /* [cmd] - command substitution */
    TCL_NODE_BACKSLASH,     /* \x - backslash escape (pre-resolved) */
    TCL_NODE_EXPAND,        /* {*}word - list expansion */
} TclNodeType;

/* ========================================================================
 * AST Node Structure
 *
 * Uses a tagged union to represent different node types.
 * All nodes are arena-allocated - no individual freeing needed.
 * ======================================================================== */

struct TclAstNode {
    TclNodeType type;
    int line;               /* Source line number for error messages */

    union {
        /* TCL_NODE_SCRIPT: sequence of commands */
        struct {
            TclAstNode **cmds;  /* Array of command nodes */
            int count;          /* Number of commands */
        } script;

        /* TCL_NODE_COMMAND: command invocation with arguments */
        struct {
            TclAstNode **words; /* Array of word nodes (first is command name) */
            int count;          /* Number of words */
        } command;

        /* TCL_NODE_WORD: composite word with parts */
        struct {
            TclAstNode **parts; /* Array of literal/var/cmd nodes */
            int count;          /* Number of parts */
        } word;

        /* TCL_NODE_LITERAL: literal string value */
        struct {
            const char *value;  /* String data (points into source or arena) */
            size_t len;         /* Length in bytes */
        } literal;

        /* TCL_NODE_VAR_SIMPLE: simple variable reference */
        struct {
            const char *name;   /* Variable name */
            size_t len;         /* Name length */
        } varSimple;

        /* TCL_NODE_VAR_ARRAY: array variable reference */
        struct {
            const char *name;   /* Array name */
            size_t nameLen;     /* Name length */
            TclAstNode *index;  /* Index expression (word node) */
        } varArray;

        /* TCL_NODE_CMD_SUBST: command substitution */
        struct {
            TclAstNode *script; /* Script to evaluate */
        } cmdSubst;

        /* TCL_NODE_BACKSLASH: pre-resolved escape sequence */
        struct {
            const char *value;  /* Resolved character(s) */
            size_t len;         /* Length (usually 1, but can be more for \uXXXX) */
        } backslash;

        /* TCL_NODE_EXPAND: {*} list expansion */
        struct {
            TclAstNode *word;   /* Word to expand as list */
        } expand;
    } u;
};

/* ========================================================================
 * AST Construction API
 *
 * All allocation happens from the provided arena.
 * The arena pointer is obtained from host->arenaPush().
 * ======================================================================== */

/* Forward declare TclInterp for allocation */
struct TclInterp;

/* Allocate a new node of the given type */
TclAstNode *tclAstNew(struct TclInterp *interp, void *arena, TclNodeType type, int line);

/* Create specific node types (convenience functions) */
TclAstNode *tclAstScript(struct TclInterp *interp, void *arena, int line);
TclAstNode *tclAstCommand(struct TclInterp *interp, void *arena, int line);
TclAstNode *tclAstWord(struct TclInterp *interp, void *arena, int line);
TclAstNode *tclAstLiteral(struct TclInterp *interp, void *arena, const char *value, size_t len, int line);
TclAstNode *tclAstVarSimple(struct TclInterp *interp, void *arena, const char *name, size_t len, int line);
TclAstNode *tclAstVarArray(struct TclInterp *interp, void *arena, const char *name, size_t nameLen, TclAstNode *index, int line);
TclAstNode *tclAstCmdSubst(struct TclInterp *interp, void *arena, TclAstNode *script, int line);
TclAstNode *tclAstBackslash(struct TclInterp *interp, void *arena, const char *value, size_t len, int line);
TclAstNode *tclAstExpand(struct TclInterp *interp, void *arena, TclAstNode *word, int line);

/* Add child nodes to container nodes */
int tclAstScriptAddCmd(struct TclInterp *interp, void *arena, TclAstNode *script, TclAstNode *cmd);
int tclAstCommandAddWord(struct TclInterp *interp, void *arena, TclAstNode *command, TclAstNode *word);
int tclAstWordAddPart(struct TclInterp *interp, void *arena, TclAstNode *word, TclAstNode *part);

/* ========================================================================
 * Parsing API
 *
 * Parse a script string into an AST.
 * ======================================================================== */

/* Parse a complete script into an AST */
TclAstNode *tclAstParse(struct TclInterp *interp, void *arena, const char *script, size_t len);

/* Parse a single word (for substitution context) */
TclAstNode *tclAstParseWord(struct TclInterp *interp, void *arena, const char *word, size_t len, int quoted);

#endif /* TCLC_AST_H */
