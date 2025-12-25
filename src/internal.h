#ifndef INCLUDE_FEATHER_INTERNAL
#define INCLUDE_FEATHER_INTERNAL

#include "feather.h"

// Internal forward declarations go here

/**
 * feather_str_eq compares a length-delimited string against a null-terminated literal.
 *
 * Returns 1 if equal, 0 otherwise.
 */
int feather_str_eq(const char *s, size_t len, const char *lit);

/**
 * feather_lookup_builtin looks up a builtin command by name.
 * Returns NULL if no builtin with that name exists.
 */
FeatherBuiltinCmd feather_lookup_builtin(const char *name, size_t len);

/**
 * feather_builtin_proc implements the TCL 'proc' command.
 *
 * Usage:
 *   proc name args body
 *
 * Defines a new procedure with the given name, parameter list, and body.
 */
FeatherResult feather_builtin_proc(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args);

/**
 * feather_invoke_proc invokes a user-defined procedure.
 *
 * Handles frame push/pop, parameter binding, and body evaluation.
 */
FeatherResult feather_invoke_proc(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj name, FeatherObj args);

/**
 * feather_builtin_if implements the TCL 'if' command.
 *
 * Usage:
 *   if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?
 */
FeatherResult feather_builtin_if(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_while implements the TCL 'while' command.
 *
 * Usage:
 *   while test command
 *
 * Catches TCL_BREAK and TCL_CONTINUE from the body.
 */
FeatherResult feather_builtin_while(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_for implements the TCL 'for' command.
 *
 * Usage:
 *   for start test next command
 *
 * C-style loop: executes start once, then repeatedly evaluates test,
 * executes command if true, then executes next. Catches TCL_BREAK
 * and TCL_CONTINUE from the body.
 */
FeatherResult feather_builtin_for(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_foreach implements the TCL 'foreach' command.
 *
 * Usage:
 *   foreach varList list ?varList list ...? command
 *
 * Iterates over one or more lists, setting variables for each iteration.
 * Catches TCL_BREAK and TCL_CONTINUE from the body.
 */
FeatherResult feather_builtin_foreach(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_switch implements the TCL 'switch' command.
 *
 * Usage:
 *   switch ?options? string pattern body ?pattern body ...?
 *   switch ?options? string {pattern body ?pattern body ...?}
 *
 * Options: -exact, -glob, -regexp, --
 * Matches string against patterns and executes the corresponding body.
 */
FeatherResult feather_builtin_switch(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_tailcall implements the TCL 'tailcall' command.
 *
 * Usage:
 *   tailcall command ?arg ...?
 *
 * Replaces the current procedure invocation with a call to another command.
 * Must be called from within a proc or lambda.
 */
FeatherResult feather_builtin_tailcall(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_break implements the TCL 'break' command.
 *
 * Usage:
 *   break
 *
 * Returns TCL_BREAK to exit the enclosing loop.
 */
FeatherResult feather_builtin_break(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_continue implements the TCL 'continue' command.
 *
 * Usage:
 *   continue
 *
 * Returns TCL_CONTINUE to skip to the next loop iteration.
 */
FeatherResult feather_builtin_continue(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_incr implements the TCL 'incr' command.
 *
 * Usage:
 *   incr varName ?increment?
 *
 * Increments varName by increment (default 1) and returns new value.
 */
FeatherResult feather_builtin_incr(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_llength implements the TCL 'llength' command.
 *
 * Usage:
 *   llength list
 *
 * Returns the number of elements in list.
 */
FeatherResult feather_builtin_llength(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lindex implements the TCL 'lindex' command.
 *
 * Usage:
 *   lindex list index
 *
 * Returns the element at index in list.
 */
FeatherResult feather_builtin_lindex(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_return implements the TCL 'return' command.
 *
 * Usage:
 *   return ?-code code? ?-level level? ?result?
 *
 * Returns from a procedure with an optional result value.
 * The -code option specifies the return code.
 * The -level option controls when the code takes effect.
 */
FeatherResult feather_builtin_return(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_mathfunc_exp implements the tcl::mathfunc::exp function.
 *
 * Usage:
 *   tcl::mathfunc::exp value
 *
 * Returns floor(e^value) for integer value.
 */
FeatherResult feather_builtin_mathfunc_exp(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_error implements the TCL 'error' command.
 *
 * Usage:
 *   error message ?info? ?code?
 *
 * Raises an error with the given message.
 * Optional info is stored in errorInfo.
 * Optional code is stored in errorCode.
 */
FeatherResult feather_builtin_error(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_catch implements the TCL 'catch' command.
 *
 * Usage:
 *   catch script ?resultVar? ?optionsVar?
 *
 * Evaluates script and captures the return code.
 * If resultVar is provided, stores the result/error in it.
 * If optionsVar is provided, stores the return options dictionary.
 * Returns the return code as an integer.
 */
FeatherResult feather_builtin_catch(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_info implements the TCL 'info' command.
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
FeatherResult feather_builtin_info(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_upvar implements the TCL 'upvar' command.
 *
 * Usage:
 *   upvar ?level? otherVar myVar ?otherVar myVar ...?
 *
 * Creates a link between a local variable and a variable in another frame.
 * Level defaults to 1 (caller's frame). Can be a number (relative) or #N (absolute).
 */
FeatherResult feather_builtin_upvar(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_uplevel implements the TCL 'uplevel' command.
 *
 * Usage:
 *   uplevel ?level? script ?arg ...?
 *
 * Evaluates script in the context of a calling frame.
 * Level defaults to 1 (caller's frame). Can be a number (relative) or #N (absolute).
 * Multiple script arguments are concatenated with spaces.
 */
FeatherResult feather_builtin_uplevel(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_rename implements the TCL 'rename' command.
 *
 * Usage:
 *   rename oldName newName
 *
 * Renames a command from oldName to newName.
 * If newName is an empty string, the command is deleted.
 */
FeatherResult feather_builtin_rename(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_namespace implements the TCL 'namespace' command.
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
FeatherResult feather_builtin_namespace(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_variable implements the TCL 'variable' command.
 *
 * Usage:
 *   variable name ?value? ?name value ...?
 *
 * Declares or links namespace variables. When called inside a namespace
 * eval, creates namespace variables. When called inside a proc, links
 * local variables to the proc's namespace variables.
 */
FeatherResult feather_builtin_variable(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_throw implements the TCL 'throw' command.
 *
 * Usage:
 *   throw type message
 *
 * Raises an error with the given type (list) as -errorcode.
 * The type must be a non-empty list of words.
 * The message is the human-readable error text.
 */
FeatherResult feather_builtin_throw(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_try implements the TCL 'try' command.
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
FeatherResult feather_builtin_try(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_trace implements the TCL 'trace' command.
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
FeatherResult feather_builtin_trace(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_split_command splits a qualified command name into namespace and simple name.
 *
 * For "::foo::bar": ns_out = "::foo", name_out = "bar"
 * For "::cmd": ns_out = "::", name_out = "cmd"
 * For "cmd" (unqualified): ns_out = nil (0), name_out = "cmd"
 * For "foo::bar" (relative) in ::current: ns_out = "::current::foo", name_out = "bar"
 */
FeatherResult feather_split_command(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj qualified, FeatherObj *ns_out, FeatherObj *name_out);

// M15: List operations

/**
 * feather_builtin_list implements the TCL 'list' command.
 *
 * Usage:
 *   list ?arg ...?
 *
 * Returns a list containing the arguments.
 */
FeatherResult feather_builtin_list(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lrange implements the TCL 'lrange' command.
 *
 * Usage:
 *   lrange list first last
 *
 * Returns a sublist from index first to last (inclusive).
 */
FeatherResult feather_builtin_lrange(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lappend implements the TCL 'lappend' command.
 *
 * Usage:
 *   lappend varName ?value ...?
 *
 * Appends values to list variable and returns new list.
 */
FeatherResult feather_builtin_lappend(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lset implements the TCL 'lset' command.
 *
 * Usage:
 *   lset varName index ?index ...? value
 *
 * Sets element at index in list variable.
 */
FeatherResult feather_builtin_lset(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lreplace implements the TCL 'lreplace' command.
 *
 * Usage:
 *   lreplace list first last ?element ...?
 *
 * Replaces elements in a list.
 */
FeatherResult feather_builtin_lreplace(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lsort implements the TCL 'lsort' command.
 *
 * Usage:
 *   lsort ?options? list
 *
 * Sorts list according to options.
 */
FeatherResult feather_builtin_lsort(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lsearch implements the TCL 'lsearch' command.
 *
 * Usage:
 *   lsearch ?options? list pattern
 *
 * Searches list for pattern.
 */
FeatherResult feather_builtin_lsearch(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

// M15: String operations

/**
 * feather_builtin_string implements the TCL 'string' command.
 *
 * Usage:
 *   string subcommand ?arg ...?
 *
 * String manipulation subcommands.
 */
FeatherResult feather_builtin_string(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_split implements the TCL 'split' command.
 *
 * Usage:
 *   split string ?splitChars?
 *
 * Splits string into list.
 */
FeatherResult feather_builtin_split(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_join implements the TCL 'join' command.
 *
 * Usage:
 *   join list ?joinString?
 *
 * Joins list elements into string.
 */
FeatherResult feather_builtin_join(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_concat implements the TCL 'concat' command.
 *
 * Usage:
 *   concat ?arg ...?
 *
 * Concatenates arguments with space, trimming whitespace.
 */
FeatherResult feather_builtin_concat(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_append implements the TCL 'append' command.
 *
 * Usage:
 *   append varName ?value ...?
 *
 * Appends values to string variable.
 */
FeatherResult feather_builtin_append(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_unset implements the TCL 'unset' command.
 *
 * Usage:
 *   unset ?-nocomplain? ?--? ?name ...?
 *
 * Removes variables.
 */
FeatherResult feather_builtin_unset(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args);

// M16: Dictionary support

/**
 * feather_builtin_dict implements the TCL 'dict' command.
 *
 * Usage:
 *   dict subcommand ?arg ...?
 *
 * Dictionary manipulation subcommands: create, get, set, exists, keys, values,
 * size, remove, replace, merge, append, incr, lappend, unset, for, info, getdef.
 */
FeatherResult feather_builtin_dict(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args);

#endif
