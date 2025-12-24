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
 * tcl_builtin_for implements the TCL 'for' command.
 *
 * Usage:
 *   for start test next command
 *
 * C-style loop: executes start once, then repeatedly evaluates test,
 * executes command if true, then executes next. Catches TCL_BREAK
 * and TCL_CONTINUE from the body.
 */
TclResult tcl_builtin_for(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args);

/**
 * tcl_builtin_foreach implements the TCL 'foreach' command.
 *
 * Usage:
 *   foreach varList list ?varList list ...? command
 *
 * Iterates over one or more lists, setting variables for each iteration.
 * Catches TCL_BREAK and TCL_CONTINUE from the body.
 */
TclResult tcl_builtin_foreach(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args);

/**
 * tcl_builtin_switch implements the TCL 'switch' command.
 *
 * Usage:
 *   switch ?options? string pattern body ?pattern body ...?
 *   switch ?options? string {pattern body ?pattern body ...?}
 *
 * Options: -exact, -glob, -regexp, --
 * Matches string against patterns and executes the corresponding body.
 */
TclResult tcl_builtin_switch(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args);

/**
 * tcl_builtin_tailcall implements the TCL 'tailcall' command.
 *
 * Usage:
 *   tailcall command ?arg ...?
 *
 * Replaces the current procedure invocation with a call to another command.
 * Must be called from within a proc or lambda.
 */
TclResult tcl_builtin_tailcall(const TclHostOps *ops, TclInterp interp,
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

/**
 * tcl_builtin_return implements the TCL 'return' command.
 *
 * Usage:
 *   return ?-code code? ?-level level? ?result?
 *
 * Returns from a procedure with an optional result value.
 * The -code option specifies the return code.
 * The -level option controls when the code takes effect.
 */
TclResult tcl_builtin_return(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args);

/**
 * tcl_builtin_mathfunc_exp implements the tcl::mathfunc::exp function.
 *
 * Usage:
 *   tcl::mathfunc::exp value
 *
 * Returns floor(e^value) for integer value.
 */
TclResult tcl_builtin_mathfunc_exp(const TclHostOps *ops, TclInterp interp,
                                    TclObj cmd, TclObj args);

/**
 * tcl_builtin_error implements the TCL 'error' command.
 *
 * Usage:
 *   error message ?info? ?code?
 *
 * Raises an error with the given message.
 * Optional info is stored in errorInfo.
 * Optional code is stored in errorCode.
 */
TclResult tcl_builtin_error(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_builtin_catch implements the TCL 'catch' command.
 *
 * Usage:
 *   catch script ?resultVar? ?optionsVar?
 *
 * Evaluates script and captures the return code.
 * If resultVar is provided, stores the result/error in it.
 * If optionsVar is provided, stores the return options dictionary.
 * Returns the return code as an integer.
 */
TclResult tcl_builtin_catch(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_builtin_info implements the TCL 'info' command.
 *
 * Usage:
 *   info subcommand ?arg ...?
 *
 * Subcommands:
 *   exists varName      - returns 1 if variable exists, 0 otherwise
 *   level ?number?      - returns current level or frame info at level
 *   commands ?pattern?  - returns list of command names
 *   procs ?pattern?     - returns list of user-defined procedure names
 *   body procname       - returns body of procedure
 *   args procname       - returns argument list of procedure
 */
TclResult tcl_builtin_info(const TclHostOps *ops, TclInterp interp,
                            TclObj cmd, TclObj args);

/**
 * tcl_builtin_upvar implements the TCL 'upvar' command.
 *
 * Usage:
 *   upvar ?level? otherVar myVar ?otherVar myVar ...?
 *
 * Creates a link between a local variable and a variable in another frame.
 * Level defaults to 1 (caller's frame). Can be a number (relative) or #N (absolute).
 */
TclResult tcl_builtin_upvar(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_builtin_uplevel implements the TCL 'uplevel' command.
 *
 * Usage:
 *   uplevel ?level? script ?arg ...?
 *
 * Evaluates script in the context of a calling frame.
 * Level defaults to 1 (caller's frame). Can be a number (relative) or #N (absolute).
 * Multiple script arguments are concatenated with spaces.
 */
TclResult tcl_builtin_uplevel(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args);

/**
 * tcl_builtin_rename implements the TCL 'rename' command.
 *
 * Usage:
 *   rename oldName newName
 *
 * Renames a command from oldName to newName.
 * If newName is an empty string, the command is deleted.
 */
TclResult tcl_builtin_rename(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args);

/**
 * tcl_builtin_namespace implements the TCL 'namespace' command.
 *
 * Usage:
 *   namespace subcommand ?arg ...?
 *
 * Subcommands:
 *   eval ns script      - evaluate script in namespace context
 *   current             - return current namespace path
 *   exists ns           - check if namespace exists (returns 0 or 1)
 *   children ?ns?       - list child namespaces
 *   parent ?ns?         - get parent namespace
 *   delete ns ?ns ...?  - delete namespaces
 */
TclResult tcl_builtin_namespace(const TclHostOps *ops, TclInterp interp,
                                 TclObj cmd, TclObj args);

/**
 * tcl_builtin_variable implements the TCL 'variable' command.
 *
 * Usage:
 *   variable name ?value? ?name value ...?
 *
 * Declares or links namespace variables. When called inside a namespace
 * eval, creates namespace variables. When called inside a proc, links
 * local variables to the proc's namespace variables.
 */
TclResult tcl_builtin_variable(const TclHostOps *ops, TclInterp interp,
                                TclObj cmd, TclObj args);

/**
 * tcl_builtin_throw implements the TCL 'throw' command.
 *
 * Usage:
 *   throw type message
 *
 * Raises an error with the given type (list) as -errorcode.
 * The type must be a non-empty list of words.
 * The message is the human-readable error text.
 */
TclResult tcl_builtin_throw(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_builtin_try implements the TCL 'try' command.
 *
 * Usage:
 *   try body ?handler...? ?finally script?
 *
 * Evaluates body and handles results with optional handlers.
 * Handlers can be:
 *   on code variableList script - matches return codes
 *   trap pattern variableList script - matches -errorcode prefix
 *
 * The finally script always executes, even on error.
 */
TclResult tcl_builtin_try(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args);

/**
 * tcl_builtin_trace implements the TCL 'trace' command.
 *
 * Usage:
 *   trace subcommand ?arg ...?
 *
 * Subcommands:
 *   add type name opList command    - add a trace
 *   remove type name opList command - remove a trace
 *   info type name                  - list traces on name
 *
 * Type is 'variable' or 'command'.
 * For variables, opList is a list of: read, write, unset
 * For commands, opList is a list of: rename, delete
 */
TclResult tcl_builtin_trace(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args);

/**
 * tcl_split_command splits a qualified command name into namespace and simple name.
 *
 * For "::foo::bar": ns_out = "::foo", name_out = "bar"
 * For "::cmd": ns_out = "::", name_out = "cmd"
 * For "cmd" (unqualified): ns_out = nil (0), name_out = "cmd"
 * For "foo::bar" (relative) in ::current: ns_out = "::current::foo", name_out = "bar"
 */
TclResult tcl_split_command(const TclHostOps *ops, TclInterp interp,
                            TclObj qualified, TclObj *ns_out, TclObj *name_out);

#endif
