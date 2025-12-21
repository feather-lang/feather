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

#endif
