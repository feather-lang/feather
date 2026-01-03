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
 * feather_obj_eq_literal compares a FeatherObj's string representation
 * against a null-terminated literal using ops->string.equal().
 *
 * This avoids calling ops->string.get() for simple string comparisons.
 * Returns 1 if equal, 0 otherwise.
 */
static inline int feather_obj_eq_literal(const FeatherHostOps *ops, FeatherInterp interp,
                                         FeatherObj obj, const char *lit) {
    FeatherObj litObj = ops->string.intern(interp, lit, feather_strlen(lit));
    return ops->string.equal(interp, obj, litObj);
}

/**
 * feather_obj_is_qualified checks if an object's string value contains "::".
 *
 * Uses byte-at-a-time access to avoid ops->string.get().
 * Returns 1 if qualified (contains "::"), 0 otherwise.
 */
static inline int feather_obj_is_qualified(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj obj) {
    size_t len = ops->string.byte_length(interp, obj);
    for (size_t i = 0; i + 1 < len; i++) {
        int c1 = ops->string.byte_at(interp, obj, i);
        if (c1 == ':') {
            int c2 = ops->string.byte_at(interp, obj, i + 1);
            if (c2 == ':') {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * feather_obj_contains_char checks if an object's string value contains a character.
 *
 * Uses byte-at-a-time access. Returns 1 if found, 0 otherwise.
 */
static inline int feather_obj_contains_char(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj obj, int ch) {
    size_t len = ops->string.byte_length(interp, obj);
    for (size_t i = 0; i < len; i++) {
        if (ops->string.byte_at(interp, obj, i) == ch) {
            return 1;
        }
    }
    return 0;
}

/**
 * feather_obj_starts_with_char checks if an object's string value starts with a character.
 *
 * Uses byte-at-a-time access. Returns 1 if starts with ch, 0 otherwise.
 */
static inline int feather_obj_starts_with_char(const FeatherHostOps *ops, FeatherInterp interp,
                                               FeatherObj obj, int ch) {
    return ops->string.byte_at(interp, obj, 0) == ch;
}

/**
 * feather_obj_is_pure_digits checks if an object's string value is all digits 0-9.
 *
 * Uses byte-at-a-time access. Returns 1 if all digits (and non-empty), 0 otherwise.
 */
static inline int feather_obj_is_pure_digits(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj obj) {
    size_t len = ops->string.byte_length(interp, obj);
    if (len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        int c = ops->string.byte_at(interp, obj, i);
        if (c < '0' || c > '9') {
            return 0;
        }
    }
    return 1;
}

/**
 * feather_obj_to_bool_literal attempts to parse boolean literal values.
 *
 * Checks for "true", "false", "yes", "no" using ops->string.equal().
 * If matched, sets *result to 0 or 1 and returns 1.
 * If not matched, returns 0 (caller should try integer conversion).
 */
static inline int feather_obj_to_bool_literal(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj obj, int *result) {
    if (feather_obj_eq_literal(ops, interp, obj, "true")) {
        *result = 1;
        return 1;
    }
    if (feather_obj_eq_literal(ops, interp, obj, "false")) {
        *result = 0;
        return 1;
    }
    if (feather_obj_eq_literal(ops, interp, obj, "yes")) {
        *result = 1;
        return 1;
    }
    if (feather_obj_eq_literal(ops, interp, obj, "no")) {
        *result = 0;
        return 1;
    }
    return 0;
}

/**
 * feather_eval_bool_condition evaluates an expression and converts to boolean.
 *
 * Calls expr builtin, then checks for boolean literals (true/false/yes/no)
 * or converts integer result to boolean (0 = false, non-zero = true).
 *
 * On success, stores 0 or 1 in *result and returns TCL_OK.
 * On error (invalid boolean), sets error message and returns TCL_ERROR.
 */
FeatherResult feather_eval_bool_condition(const FeatherHostOps *ops,
                                           FeatherInterp interp,
                                           FeatherObj condition,
                                           int *result);

/**
 * feather_error_expected constructs an error message of the form:
 * "expected <type> but got \"<value>\""
 *
 * Sets the interpreter result to the error message.
 */
static inline void feather_error_expected(const FeatherHostOps *ops,
                                           FeatherInterp interp,
                                           const char *type,
                                           FeatherObj got) {
    // Build "expected <type> but got \""
    FeatherObj prefix_obj = ops->string.builder_new(interp, 64);
    const char *p1 = "expected ";
    while (*p1) {
        ops->string.builder_append_byte(interp, prefix_obj, *p1++);
    }
    const char *t = type;
    while (*t) {
        ops->string.builder_append_byte(interp, prefix_obj, *t++);
    }
    const char *p2 = " but got \"";
    while (*p2) {
        ops->string.builder_append_byte(interp, prefix_obj, *p2++);
    }
    FeatherObj prefix = ops->string.builder_finish(interp, prefix_obj);

    FeatherObj suffix = ops->string.intern(interp, "\"", 1);
    FeatherObj msg = ops->string.concat(interp, prefix, got);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
}

/**
 * feather_obj_is_args_param checks if an object equals "args" (variadic param).
 *
 * Uses ops->string.equal() for comparison.
 */
static inline int feather_obj_is_args_param(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj obj) {
    return feather_obj_eq_literal(ops, interp, obj, "args");
}

/**
 * feather_obj_is_global_ns checks if an object equals "::" (global namespace).
 *
 * Uses ops->string.equal() for comparison.
 */
static inline int feather_obj_is_global_ns(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj obj) {
    return feather_obj_eq_literal(ops, interp, obj, "::");
}

/**
 * feather_obj_glob_match performs glob pattern matching using byte-at-a-time access.
 *
 * This is the object-based version of feather_glob_match that avoids ops->string.get().
 * Returns 1 if pattern matches string, 0 otherwise.
 * Supports: * (any sequence), ? (any single char), [...] (character class),
 *           \ (escape), and literal characters.
 */
int feather_obj_glob_match(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj pattern, FeatherObj string);

/**
 * feather_obj_resolve_variable resolves a variable name object to namespace + local parts.
 *
 * Object-based version of feather_resolve_variable that avoids ops->string.get().
 * Three cases:
 *   1. Unqualified ("x") - ns_out = nil, local_out = x
 *   2. Absolute ("::foo::x") - ns_out = "::foo", local_out = "x"
 *   3. Relative ("foo::x") - prepends current namespace
 */
FeatherResult feather_obj_resolve_variable(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj name,
                                           FeatherObj *ns_out, FeatherObj *local_out);

/**
 * feather_obj_split_command splits a qualified command name into namespace + simple name.
 *
 * Object-based version of feather_split_command that avoids ops->string.get().
 */
FeatherResult feather_obj_split_command(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj qualified,
                                        FeatherObj *ns_out, FeatherObj *name_out);

/**
 * feather_obj_find_last_colons finds the position of the last "::" in an object.
 *
 * Returns the position of the first ':' of the last "::" sequence, or -1 if not found.
 * Uses byte-at-a-time access.
 */
static inline long feather_obj_find_last_colons(const FeatherHostOps *ops, FeatherInterp interp,
                                                FeatherObj obj) {
    size_t len = ops->string.byte_length(interp, obj);
    long lastSep = -1;
    for (size_t i = 0; i + 1 < len; i++) {
        int c1 = ops->string.byte_at(interp, obj, i);
        if (c1 == ':') {
            int c2 = ops->string.byte_at(interp, obj, i + 1);
            if (c2 == ':') {
                lastSep = (long)i;
            }
        }
    }
    return lastSep;
}

/**
 * feather_obj_matches_at checks if 'pattern' matches at position 'pos' in 'str'.
 *
 * Returns 1 if all bytes of pattern match str starting at pos, 0 otherwise.
 * Uses byte-at-a-time access.
 */
static inline int feather_obj_matches_at(const FeatherHostOps *ops, FeatherInterp interp,
                                          FeatherObj str, size_t pos, FeatherObj pattern) {
    size_t strLen = ops->string.byte_length(interp, str);
    size_t patLen = ops->string.byte_length(interp, pattern);
    if (pos + patLen > strLen) return 0;
    for (size_t i = 0; i < patLen; i++) {
        int c1 = ops->string.byte_at(interp, str, pos + i);
        int c2 = ops->string.byte_at(interp, pattern, i);
        if (c1 != c2) return 0;
    }
    return 1;
}

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
 * feather_builtin_lmap implements the TCL 'lmap' command.
 *
 * Usage:
 *   lmap varList list ?varList list ...? command
 *
 * Like foreach, but collects results from each iteration into a list.
 * break stops the loop, continue skips adding to the result.
 */
FeatherResult feather_builtin_lmap(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_lassign implements the TCL 'lassign' command.
 *
 * Usage:
 *   lassign list ?varName ...?
 *
 * Assigns successive elements from list to the named variables.
 * Variables beyond list length are set to empty string.
 * Returns list of unassigned elements (if more elements than vars).
 */
FeatherResult feather_builtin_lassign(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_linsert implements the TCL 'linsert' command.
 *
 * Usage:
 *   linsert list index ?element ...?
 *
 * Inserts elements into list before the index position.
 * Returns a new list with elements inserted.
 * Index less than 0 inserts at beginning; greater than length inserts at end.
 * Supports end-relative indexing (end, end-N, end+N) and integer arithmetic.
 */
FeatherResult feather_builtin_linsert(const FeatherHostOps *ops, FeatherInterp interp,
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

/* Math functions - tcl::mathfunc::* */
FeatherResult feather_builtin_mathfunc_sqrt(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_exp(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_log(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_log10(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_sin(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_cos(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_tan(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_asin(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_acos(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_atan(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_sinh(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_cosh(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_tanh(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_floor(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_ceil(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_round(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_abs(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_pow(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_atan2(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_fmod(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_hypot(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_double(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_int(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_wide(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_isnan(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_isinf(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_isfinite(const FeatherHostOps *ops, FeatherInterp interp,
                                                FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_isnormal(const FeatherHostOps *ops, FeatherInterp interp,
                                                FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_issubnormal(const FeatherHostOps *ops, FeatherInterp interp,
                                                   FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_isunordered(const FeatherHostOps *ops, FeatherInterp interp,
                                                   FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_bool(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_entier(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_max(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args);
FeatherResult feather_builtin_mathfunc_min(const FeatherHostOps *ops, FeatherInterp interp,
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
 * feather_builtin_global implements the TCL 'global' command.
 *
 * Usage:
 *   global ?varname ...?
 *
 * Creates local variables linked to corresponding global variables.
 * Only has effect when executed inside a proc body.
 * If varname contains namespace qualifiers, the local variable's name
 * is the unqualified name (namespace tail).
 */
FeatherResult feather_builtin_global(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_apply implements the TCL 'apply' command.
 *
 * Usage:
 *   apply lambdaExpr ?arg1 arg2 ...?
 *
 * Applies an anonymous function (lambda) to arguments.
 * lambdaExpr is a list of 2 or 3 elements: {args body ?namespace?}
 * - args: formal parameter list (like proc)
 * - body: script to execute
 * - namespace: optional namespace context for execution
 */
FeatherResult feather_builtin_apply(const FeatherHostOps *ops, FeatherInterp interp,
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
 * feather_builtin_lreverse implements the TCL 'lreverse' command.
 *
 * Usage:
 *   lreverse list
 *
 * Returns a list with elements in reverse order.
 */
FeatherResult feather_builtin_lreverse(const FeatherHostOps *ops, FeatherInterp interp,
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
 * feather_builtin_lrepeat implements the TCL 'lrepeat' command.
 *
 * Usage:
 *   lrepeat count ?value ...?
 *
 * Creates a list by repeating elements count times.
 */
FeatherResult feather_builtin_lrepeat(const FeatherHostOps *ops, FeatherInterp interp,
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

/**
 * feather_builtin_format implements the TCL 'format' command.
 *
 * Usage:
 *   format formatString ?arg ...?
 *
 * Generates a formatted string similar to C sprintf.
 * Supports: %d, %i, %u, %o, %x, %X, %b, %c, %s, %f, %e, %E, %g, %G, %%
 * Plus flags (-, +, space, 0, #), width, precision, and positional specifiers.
 */
FeatherResult feather_builtin_format(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_scan implements the TCL 'scan' command.
 *
 * Usage:
 *   scan string format ?varName ...?
 *
 * Parses string using conversion specifiers similar to C sscanf.
 * Returns count of conversions (with varNames) or list of values (inline mode).
 * Supports: %d, %i, %u, %o, %x, %X, %b, %c, %s, %f, %e, %g, %n, %[...], %[^...]
 * Plus width specifiers, assignment suppression (*), and positional specifiers.
 */
FeatherResult feather_builtin_scan(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_subst implements the TCL 'subst' command.
 *
 * Usage:
 *   subst ?-nobackslashes? ?-nocommands? ?-novariables? string
 *
 * Performs backslash, command, and variable substitutions on string.
 * Options disable specific substitution types.
 * Handles break (stops substitution), continue (empty substitution),
 * and return (substitutes returned value) exceptions specially.
 */
FeatherResult feather_builtin_subst(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_eval implements the TCL 'eval' command.
 *
 * Usage:
 *   eval arg ?arg ...?
 *
 * Concatenates all arguments (like concat) and evaluates the result as a script.
 * Returns the result of the last command in the script.
 */
FeatherResult feather_builtin_eval(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj cmd, FeatherObj args);

// ============================================================================
// Trace system helpers
// ============================================================================

/**
 * feather_trace_get_dict retrieves the trace dict for a given kind.
 *
 * kind must be "variable", "command", or "execution".
 * Returns the dict stored in ::tcl::trace::{kind}.
 * Returns nil if kind is invalid (caller should validate before calling).
 */
FeatherObj feather_trace_get_dict(const FeatherHostOps *ops, FeatherInterp interp,
                                  const char *kind);

/**
 * feather_trace_set_dict updates the trace dict for a given kind.
 *
 * kind must be "variable", "command", or "execution".
 * Stores dict in ::tcl::trace::{kind}.
 */
void feather_trace_set_dict(const FeatherHostOps *ops, FeatherInterp interp,
                            const char *kind, FeatherObj dict);

/**
 * feather_fire_var_traces fires variable traces for the given operation.
 *
 * varName: the variable name
 * op: "read", "write", or "unset"
 *
 * Traces fire in LIFO order (most recently added first).
 * The trace callback receives: script varName {} op
 *
 * Returns TCL_ERROR if a trace callback returns an error (for read/write).
 * For unset traces, errors are ignored and TCL_OK is always returned.
 * The error message is wrapped as "can't set/read \"varname\": <error>".
 */
FeatherResult feather_fire_var_traces(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj varName, const char *op);

/**
 * feather_fire_cmd_traces fires command traces for the given operation.
 *
 * oldName: the original command name (fully qualified)
 * newName: the new command name (empty for delete)
 * op: "rename" or "delete"
 *
 * Traces fire in FIFO order (first added, first fired).
 */
void feather_fire_cmd_traces(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj oldName, FeatherObj newName, const char *op);

/**
 * feather_fire_exec_traces fires execution traces for enter or leave.
 *
 * cmdName: the command name (fully qualified, for lookup)
 * cmdList: the full command as a list [cmdname, arg1, arg2, ...]
 * op: "enter" or "leave"
 * code: return code (only for leave, 0 for enter)
 * result: command result (only for leave, 0 for enter)
 *
 * Traces fire in LIFO order (last added, first fired).
 *
 * Returns TCL_ERROR if a trace callback returns an error.
 * The error propagates directly without wrapping.
 */
FeatherResult feather_fire_exec_traces(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj cmdName, FeatherObj cmdList,
                                       const char *op, int code, FeatherObj result);

/**
 * feather_has_step_traces checks if a command has enterstep or leavestep traces.
 *
 * cmdName: the command name (fully qualified)
 *
 * Returns 1 if the command has any step traces, 0 otherwise.
 */
int feather_has_step_traces(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmdName);

/**
 * feather_script_eval_obj_stepped evaluates a script with step tracing.
 *
 * Like feather_script_eval_obj, but fires enterstep/leavestep traces
 * for each command executed, looking up traces on stepTarget.
 *
 * stepTarget: the command name to look up step traces on (fully qualified)
 */
FeatherResult feather_script_eval_obj_stepped(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj script, FeatherObj stepTarget,
                                              FeatherEvalFlags flags);

/**
 * feather_command_exec_stepped executes a command with step tracing.
 *
 * Like feather_command_exec, but fires enterstep before and leavestep after
 * execution, looking up traces on stepTarget.
 *
 * stepTarget: the command name to look up step traces on (fully qualified)
 */
FeatherResult feather_command_exec_stepped(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj command, FeatherObj stepTarget,
                                           FeatherEvalFlags flags);

/**
 * feather_get_step_target returns the current step trace target.
 *
 * Returns 0 if no step tracing is active.
 * This is used to propagate step traces through nested procedure calls.
 */
FeatherObj feather_get_step_target(void);

/**
 * feather_set_step_target sets the current step trace target.
 *
 * Pass 0 to disable step tracing.
 */
void feather_set_step_target(FeatherObj target);

// ============================================================================
// Variable access wrappers (with trace support)
// ============================================================================

/**
 * feather_get_var retrieves a variable and fires read traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * All builtins should use this instead of ops->var.get() directly.
 *
 * On success, returns TCL_OK and stores the value in *out.
 * On read trace error, returns TCL_ERROR with wrapped message.
 */
FeatherResult feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj name, FeatherObj *out);

/**
 * feather_set_var sets a variable and fires write traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * All builtins should use this instead of ops->var.set() directly.
 *
 * On write trace error, returns TCL_ERROR with wrapped message.
 * The variable IS set before the trace fires.
 */
FeatherResult feather_set_var(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj name, FeatherObj value);

/**
 * feather_unset_var unsets a variable and fires unset traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * All builtins should use this instead of ops->var.unset() directly.
 * Note: unset traces fire BEFORE the variable is actually unset.
 */
void feather_unset_var(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name);

/**
 * feather_var_exists checks if a variable exists.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * Returns 1 if the variable exists, 0 otherwise.
 */
int feather_var_exists(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name);

// ============================================================================
// Size modifiers for format and scan
// ============================================================================

// Size modifier types for integer truncation (shared by format and scan)
typedef enum {
  SIZE_NONE = 0,    // No modifier - 32-bit truncation for format, 32-bit for scan
  SIZE_H = 1,       // h - 16-bit for format, 32-bit for scan
  SIZE_L = 2,       // l - 64-bit truncation
  SIZE_LL = 3,      // ll - no truncation
  SIZE_BIG_L = 4,   // L - no truncation for format, 64-bit for scan
  SIZE_J = 5,       // j - 64-bit truncation
  SIZE_Z = 6,       // z - pointer size (typically 64-bit)
  SIZE_T = 7,       // t - pointer size (typically 64-bit)
  SIZE_Q = 8        // q - 64-bit truncation
} SizeModifier;

// Apply truncation for format (slightly different from scan for %h)
static inline int64_t feather_apply_format_truncation(int64_t val, SizeModifier size_mod) {
  switch (size_mod) {
    case SIZE_H:
      // 16-bit truncation: mask to 16 bits and sign-extend
      val = (int64_t)(int16_t)(val & 0xFFFF);
      break;
    case SIZE_NONE:
      // 32-bit truncation: mask to 32 bits and sign-extend
      val = (int64_t)(int32_t)(val & 0xFFFFFFFF);
      break;
    case SIZE_L:
    case SIZE_J:
    case SIZE_Q:
    case SIZE_Z:
    case SIZE_T:
      // 64-bit truncation: already int64_t, no truncation needed
      break;
    case SIZE_LL:
    case SIZE_BIG_L:
      // No truncation
      break;
  }
  return val;
}

// Apply truncation for scan (h behaves like no modifier - 32-bit)
static inline int64_t feather_apply_scan_truncation(int64_t val, SizeModifier size_mod) {
  switch (size_mod) {
    case SIZE_H:
    case SIZE_NONE:
      // 32-bit truncation: mask to 32 bits and sign-extend
      val = (int64_t)(int32_t)(val & 0xFFFFFFFF);
      break;
    case SIZE_L:
    case SIZE_BIG_L:
    case SIZE_J:
    case SIZE_Q:
    case SIZE_Z:
    case SIZE_T:
      // 64-bit truncation: already int64_t, no truncation needed
      break;
    case SIZE_LL:
      // No truncation
      break;
  }
  return val;
}

// Apply unsigned conversion: truncate then reinterpret as unsigned
static inline int64_t feather_apply_unsigned_conversion(int64_t val, SizeModifier size_mod) {
  switch (size_mod) {
    case SIZE_H:
    case SIZE_NONE:
      // 32-bit unsigned: mask to 32 bits and interpret as unsigned
      val = (int64_t)(uint32_t)(val & 0xFFFFFFFF);
      break;
    case SIZE_L:
    case SIZE_BIG_L:
    case SIZE_J:
    case SIZE_Q:
    case SIZE_Z:
    case SIZE_T:
      // 64-bit unsigned: already uint64_t when cast, no change
      // (value is already in int64_t, just reinterpret)
      val = (int64_t)(uint64_t)val;
      break;
    case SIZE_LL:
      // %llu not allowed - should have been caught in parser
      break;
  }
  return val;
}

#endif
