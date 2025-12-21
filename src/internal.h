#ifndef INCLUDE_TCLC_INTERNAL
#define INCLUDE_TCLC_INTERNAL

#include "tclc.h"

// Internal forward declarations go here

/**
 * tcl_lookup_builtin looks up a builtin command by name.
 * Returns NULL if no builtin with that name exists.
 */
TclBuiltinCmd tcl_lookup_builtin(const char *name, size_t len);

/**
 * tcl_builtin_proc implements the TCL 'proc' command.
 *
 * Usage:
 *   proc name args body
 *
 * Defines a new procedure with the given name, parameter list, and body.
 */
TclResult tcl_builtin_proc(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args);

/**
 * tcl_invoke_proc invokes a user-defined procedure.
 *
 * Handles frame push/pop, parameter binding, and body evaluation.
 */
TclResult tcl_invoke_proc(const TclHostOps *ops, TclInterp interp,
                          TclObj name, TclObj args);

/**
 * tcl_builtin_if implements the TCL 'if' command.
 *
 * Usage:
 *   if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?
 */
TclResult tcl_builtin_if(const TclHostOps *ops, TclInterp interp,
                          TclObj cmd, TclObj args);

/**
 * tcl_builtin_while implements the TCL 'while' command.
 *
 * Usage:
 *   while test command
 *
 * Catches TCL_BREAK and TCL_CONTINUE from the body.
 */
TclResult tcl_builtin_while(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_builtin_break implements the TCL 'break' command.
 *
 * Usage:
 *   break
 *
 * Returns TCL_BREAK to exit the enclosing loop.
 */
TclResult tcl_builtin_break(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_builtin_continue implements the TCL 'continue' command.
 *
 * Usage:
 *   continue
 *
 * Returns TCL_CONTINUE to skip to the next loop iteration.
 */
TclResult tcl_builtin_continue(const TclHostOps *ops, TclInterp interp,
                                TclObj cmd, TclObj args);

/**
 * tcl_builtin_incr implements the TCL 'incr' command.
 *
 * Usage:
 *   incr varName ?increment?
 *
 * Increments varName by increment (default 1) and returns new value.
 */
TclResult tcl_builtin_incr(const TclHostOps *ops, TclInterp interp,
                            TclObj cmd, TclObj args);

/**
 * tcl_builtin_llength implements the TCL 'llength' command.
 *
 * Usage:
 *   llength list
 *
 * Returns the number of elements in list.
 */
TclResult tcl_builtin_llength(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args);

/**
 * tcl_builtin_lindex implements the TCL 'lindex' command.
 *
 * Usage:
 *   lindex list index
 *
 * Returns the element at index in list.
 */
TclResult tcl_builtin_lindex(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args);

// Test command for debugging
TclResult tcl_builtin_run(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args);

#endif
