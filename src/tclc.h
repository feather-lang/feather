#ifndef INCLUDE_TCLC
#define INCLUDE_TCLC

#include <stddef.h>
#include <stdint.h>

/**
 * tclc is an embeddable implementation of the core TCL language.
 *
 * TCL was conceived at a time when most networked software was written
 * in C at the core, the internet was young, user expectations were looser.
 *
 * It is a tiny language full of great ideas, but features that were useful
 * 20 years ago are a hindrance today:
 *
 * - I/O in the language is an obstacle, as the host is more than likely
 *   to already have taken a stance on how it wants to handle I/O,
 * - a built-in event loop for multiplexing I/O and orchestrating timers
 *   was useful when no host could easily provide this, but event loops
 *   are widespread and having to integrate multiple event loops in one
 * application is error-prone.
 * - reference counting with lots of calls to malloc and free works great for
 *   standalone TCL, but the emergence of zig and wasm incentivizes being in
 *   control of allocations.
 *
 * So what ideas are worth preserving?
 *
 * A pure form of metaprogramming, syntax moldable like clay, with meaning
 * to be added at a time and in a form that is convenient for that particular
 * use case.
 *
 * A transparent execution environment: every aspect of a running TCL program
 * can be inspected from within that program, and often even modified.
 *
 * A focus on expressing computation in the form of instructions to carry out.
 *
 * The latter point is key: agentic coding benefits from an inspectable and
 * moldable environment.  Having the agent talk to your running program gives it
 * highly valuable feedback for a small amount of tokens.
 *
 * The browser is one example of this model being successful, but what about all
 * the other applications? Your job runner, web server, database, your desktop
 * or mobile app.
 *
 * tclc wants to be the thin glue layer that's easy to embed into your programs,
 * so that you can talk to them while they are running.
 *
 * Another way to look at TCL is this: it is a Lisp-2 with fexprs that extend
 * to the lexical syntax level.  Maybe that is more exciting.
 *
 * Here you will find a faithful implementation of:
 *
 * - control flow and execution primitives: proc, foreach, for, while, if,
 * return, break, continue, error, tailcall, try, throw, catch, switch
 * - introspection capabilities: info, errorCode, errorInfo, trace
 * - values and expressions: expr, incr, set, unset, global, variable
 * - metaprogramming: upvar, uplevel, rename, unknown, namespace
 * - data structures: list, dict, string, apply
 * - string manipulation: split, subst, concat, append, regexp, regsub, join
 *
 * Notable omissions (all to be covered by the host):
 *
 * - I/O: chan, puts, gets, refchan, transchan, after, vwait, update
 *   These are better provided by the host in the form of exposed commands.
 *
 * - OO: tclc intended use case is short, interactive programs
 * similar to bash. Programming in the large is explicitly not supported.
 *
 * - Coroutines: tclc interpreter objects are small and lightweight so you can
 * have of them if you need something like coroutines.
 *
 * Notables qualities of the implementation:
 *
 * This implementation is pure: it does not directly perform I/O or allocation
 * or interact with the kernel at all. It only provides TCL parsing and
 * semantics.
 *
 * All memory is allocated, accessed, and released by the embedding host.
 * The embedding host is highly likely to already have all the building blocks
 * we care about in the implementation and there is little value in building
 * our own version of regular expressions, lists, dictionaries, etc.
 *
 *
 * While this requires the host to implement a large number of functions, the
 * implementation is largely mechanical, which makes it a prime candidate
 * for delegating to agentic coding tools.
 */

/** An opaque handle type, used by the host to identify objects */
typedef uintptr_t TclHandle;

/** A handle to an interpreter instance */
typedef TclHandle TclInterp;

/** A handle to an object */
typedef TclHandle TclObj;

/**
 * TclHostOps contains all operations that the host needs to support for
 * this interpreter to work.
 */
typedef struct TclHostOps TclHostOps;

/**
 * The return code of a function informs the TCL interpreter about how to
 * procede with execution.
 *
 * See `man n return` for the full semantics.
 */
typedef enum {
  /**
   * Proceed as usual to the next instruction.
   */
  TCL_OK = 0,

  /**
   * An error occurred during execution and should be communicated to the user.
   */
  TCL_ERROR = 1,

  /**
   * Return from the caller (used by custom return-like functions).
   */
  TCL_RETURN = 2,

  /**
   * Break in the caller's frame (used by custom break-like functions).
   */
  TCL_BREAK = 3,

  /**
   * Continue in the caller's frame (used by custom continue-like functions).
   */
  TCL_CONTINUE = 4,
} TclResult;

/**
 * TclBuiltinCmd is the signature for builtin command implementations.
 *
 * Builtin commands receive the host operations, interpreter, command name,
 * and argument list. They return a result code and set the interpreter's
 * result via ops->interp.set_result.
 */
typedef TclResult (*TclBuiltinCmd)(const TclHostOps *ops, TclInterp interp,
                                   TclObj cmd, TclObj args);

/**
 * TclTokenType encodes the types of tokens returned by the parser.
 *
 * During parsing, the parser creates tagged spans of text and stores
 * them in a TCL list.
 *
 * Since the host owns all the memory, the parser only needs to communicate
 * positions back.
 */
typedef enum {
  TCL_TOKEN_LITERAL = 0,          // expr
  TCL_TOKEN_VAR = 1,              // $errorInfo
  TCL_TOKEN_VAR_BRACED = 2,       // ${errorInfo}
  TCL_TOKEN_COMMAND = 3,          // [lindex $words 1]
  TCL_TOKEN_QUOTED = 4,           // "hello world"
  TCL_TOKEN_BRACED = 5,           // {hello world}
  TCL_TOKEN_COMMAND_SEPARATOR = 6 // newline, end of input
} TclTokenType;

/**
 * TclParseStatus informs the caller about whether and how the parser
 * can be invoked again on the same input.
 */
typedef enum {
  // parsing finished successfully, result contains parsed command
  TCL_PARSE_OK = 0,
  // the parser needs more input
  TCL_PARSE_INCOMPLETE = 1,
  // the parser could not process the input successfully
  TCL_PARSE_ERROR = 2,
  // no more commands in the script
  TCL_PARSE_DONE = 3
} TclParseStatus;

/**
 * TclParseContext holds the state for iterating over commands in a script.
 */
typedef struct {
  const char *script;  // Original script
  size_t len;          // Total length
  size_t pos;          // Current position
} TclParseContext;

/**
 * tcl_parse_init initializes a parse context for iterating over commands.
 */
void tcl_parse_init(TclParseContext *ctx, const char *script, size_t len);

/**
 * tcl_parse_command parses the next command from the script.
 *
 * Returns TCL_PARSE_OK when a command was parsed successfully.
 * The parsed command (list of words) is in the interpreter's result slot.
 *
 * Returns TCL_PARSE_DONE when the script is exhausted.
 *
 * Returns TCL_PARSE_INCOMPLETE or TCL_PARSE_ERROR on failure,
 * with error information in the interpreter's result slot.
 */
TclParseStatus tcl_parse_command(const TclHostOps *ops, TclInterp interp,
                                  TclParseContext *ctx);

typedef enum {
  // Evaluate in the current scope of the interpreter
  TCL_EVAL_LOCAL = 0,
  // Evaluate in the interpreter's global scope
  TCL_EVAL_GLOBAL = 1,
} TclEvalFlags;

/*
 * EVALUATION API
 *
 * TCL has two distinct representations that can be "evaluated":
 *
 * Script  - Source code as a string. May contain multiple commands
 *           separated by newlines or semicolons.
 *           Analogous to: Lisp source text before READ
 *
 * Command - A parsed command: a list [name, arg1, arg2, ...] where
 *           each element is a word (string/object). Arguments are
 *           NOT recursively parsed - they're strings that may contain
 *           source code for later evaluation.
 *           Analogous to: A single Lisp form, but with string leaves
 *
 * The key insight: TCL command arguments are strings, not nested ASTs.
 * When you write:
 *
 *   if {$x > 0} {puts yes}
 *
 * The `if` command receives two STRING arguments: "$x > 0" and "puts yes".
 * It decides when/whether to parse and evaluate them. This is like Lisp
 * fexprs, not regular functions.
 *
 * The braces { } produce string literals - they are NOT parsed until
 * a command explicitly evaluates them. This enables:
 *   - if/while to avoid evaluating unused branches
 *   - proc to store the body for later execution
 *   - catch to trap errors from the body
 */

/**
 * tcl_command_exec executes a single parsed command.
 *
 * The command must be a list [name, arg1, arg2, ...].
 * Looks up 'name' and invokes it with the argument list.
 * Arguments are NOT evaluated - the command receives them as-is.
 *
 * Lisp equivalent: (APPLY fn args), but args are not evaluated.
 * More precisely: like calling a fexpr/macro.
 *
 * The result of execution is in the interpreter's result slot.
 */
TclResult tcl_command_exec(const TclHostOps *ops, TclInterp interp,
                           TclObj command, TclEvalFlags flags);

/**
 * tcl_script_eval evaluates a script string.
 *
 * Parses each command and executes it. Stops on error or when
 * a command returns a non-OK code (break/continue/return).
 *
 * Lisp equivalent: (PROGN (EVAL (READ s)) ...) for each command in s,
 * but commands are executed as they're parsed, not batched.
 *
 * The result of the last command is in the interpreter's result slot.
 */
TclResult tcl_script_eval(const TclHostOps *ops, TclInterp interp,
                          const char *source, size_t len, TclEvalFlags flags);

/**
 * tcl_script_eval_obj evaluates a script object.
 *
 * Gets the string representation of the object and evaluates it
 * as a script. This is what control structures (if, while, catch, proc)
 * use to evaluate their body arguments.
 *
 * Lisp equivalent: (EVAL obj) where obj is expected to contain source code.
 *
 * The result is in the interpreter's result slot.
 */
TclResult tcl_script_eval_obj(const TclHostOps *ops, TclInterp interp,
                              TclObj script, TclEvalFlags flags);

/**
 * Flags for tcl_subst controlling which substitutions to perform.
 */
typedef enum {
  TCL_SUBST_BACKSLASHES = 1,
  TCL_SUBST_VARIABLES = 2,
  TCL_SUBST_COMMANDS = 4,
  TCL_SUBST_ALL = 7
} TclSubstFlags;

/**
 * tcl_subst performs substitutions on a string.
 *
 * Performs backslash, variable, and/or command substitution on the input
 * string according to the flags parameter. The result is placed in the
 * interpreter's result slot.
 *
 * This is the core substitution engine used by quoted strings in both
 * the main parser and expression evaluator, and implements the `subst`
 * command.
 *
 * Returns TCL_OK on success, TCL_ERROR on failure.
 */
TclResult tcl_subst(const TclHostOps *ops, TclInterp interp,
                    const char *str, size_t len, int flags);

/**
 * The heart of the implementation.  An embedder needs to provide all of the
 * following operations.
 *
 * The rest of the interpreter is implemented in terms of these.
 */

/**
 * TclFrameOps describe the operations on execution frames.
 *
 * Frames contain:
 * - the variable environment in which expressions are evaluated,
 * - the command currently being evaluated,
 * - the return code of that command,
 * - the result object for holding the result of the evaluation,
 * - the error object in case of an error,
 * - their index on the call stack.
 */
typedef struct TclFrameOps {
  /**
   * push adds a new call frame to the stack for the evaluation of cmd and args.
   */
  TclResult (*push)(TclInterp interp, TclObj cmd, TclObj args);

  /**
   * pop removes the topmost frame from the callstack.
   */
  TclResult (*pop)(TclInterp interp);

  /**
   * level returns the current level of the call stack.
   */
  size_t (*level)(TclInterp interp);

  /**
   * set_active makes the provided frame the active frame on the call stack.
   */
  TclResult (*set_active)(TclInterp interp, size_t level);

  /**
   * size returns the size of the call stack.
   *
   * This is important because the level reported by level can
   * be less than the size because of a prior call to set_active.
   */
  size_t (*size)(TclInterp interp);

  /**
   * info returns information about a frame at the given level.
   *
   * Sets *cmd and *args to the command and arguments at that level.
   * Sets *ns to the namespace the frame is executing in.
   * Returns TCL_ERROR if the level is out of bounds.
   */
  TclResult (*info)(TclInterp interp, size_t level, TclObj *cmd, TclObj *args,
                    TclObj *ns);

  /**
   * set_namespace changes the namespace of the current frame.
   *
   * Used by 'namespace eval' to temporarily change context.
   * The namespace is created if it doesn't exist.
   */
  TclResult (*set_namespace)(TclInterp interp, TclObj ns);

  /**
   * get_namespace returns the namespace of the current frame.
   */
  TclObj (*get_namespace)(TclInterp interp);
} TclFrameOps;

/**
 * TclStringOps describes the string operations the host needs to support.
 *
 * Strings are sequences of Unicode codepoints (runes).
 *
 * tclc is encoding neutral, as strings are managed by the host and all
 * characters with special meaning to the parser are part of ASCII.
 */
typedef struct TclStringOps {
  /**
   * intern returns a cached value for the given string s,
   * caching it if not present yet.
   */
  TclObj (*intern)(TclInterp interp, const char *s, size_t len);

  /**
   * get returns the string representation of an object.
   */
  const char *(*get)(TclInterp interp, TclObj obj, size_t *len);

  /**
   * concat returns a new object whose string value is
   * the concatenation of two objects.
   */
  TclObj (*concat)(TclInterp interp, TclObj a, TclObj b);

  /**
   * compare compares two strings using Unicode ordering.
   * Returns <0 if a < b, 0 if a == b, >0 if a > b.
   */
  int (*compare)(TclInterp interp, TclObj a, TclObj b);

  /**
   * regex_match tests if a string matches a regular expression pattern.
   *
   * Returns TCL_OK and sets *result to 1 if the string matches the pattern,
   * or 0 if it doesn't match. Returns TCL_ERROR if the pattern is invalid,
   * with an error message in the interpreter's result.
   */
  TclResult (*regex_match)(TclInterp interp, TclObj pattern, TclObj string,
                           int *result);
} TclStringOps;

/**
 * TclIntOps gives access to integers from the host.
 */
typedef struct TclIntOps {
  /**
   * create requests a possibly new integer from the host.
   */
  TclObj (*create)(TclInterp interp, int64_t val);

  /**
   * get extracts the integer value from an object.
   *
   * This can cause a conversion of the object's internal representation to an
   * integer.
   */
  TclResult (*get)(TclInterp interp, TclObj obj, int64_t *out);
} TclIntOps;

/**
 * TclDoubleOps gives access to floating-point numbers from the host.
 */
typedef struct TclDoubleOps {
  /**
   * create requests a possibly new double from the host.
   */
  TclObj (*create)(TclInterp interp, double val);

  /**
   * get extracts the double value from an object.
   *
   * This can cause a conversion of the object's internal representation to a
   * double.
   */
  TclResult (*get)(TclInterp interp, TclObj obj, double *out);
} TclDoubleOps;

/**
 * TclInterpOps holds the operations on the state of the
 * interpreter instance.
 *
 * @see https://www.tcl-lang.org/man/tcl9.0/TclLib/AddErrInfo.html
 * @see https://www.tcl-lang.org/man/tcl9.0/TclLib/SetResult.html
 */
typedef struct TclInterpOps {
  /**
   * set_result sets the interpreter's result object.
   */
  TclResult (*set_result)(TclInterp interp, TclObj result);

  /**
   * get_result returns the interpreter's result object.
   */
  TclObj (*get_result)(TclInterp interp);

  /**
   * reset_result clears the interpreters evaluation state
   * like the current evaluation result and error information.
   */
  TclResult (*reset_result)(TclInterp interp, TclObj result);

  /**
   * set_return_options corresponds to the options passed to return.
   *
   * Sets the return options of interp to be options.
   * If options contains any invalid value for any key, TCL_ERROR will be
   * returned, and the interp result will be set to an appropriate error
   * message. Otherwise, a completion code in agreement with the -code and
   * -level keys in options will be returned.
   */
  TclResult (*set_return_options)(TclInterp interp, TclObj options);

  /**
   * get_return_options returns the options passed to the return command.
   *
   * Retrieves the dictionary of return options from an interpreter following a
   * script evaluation.
   *
   * Routines such as tcl_eval are called to evaluate a
   * script in an interpreter.
   *
   * These routines return an integer completion code.
   *
   * These routines also leave in the interpreter both a result and a dictionary
   * of return options generated by script evaluation.
   */
  TclObj (*get_return_options)(TclInterp interp, TclResult code);

  /**
   * get_script returns the path of the currently executing script file.
   *
   * Returns empty string if no script file is being evaluated
   * (e.g., interactive input or eval'd string).
   */
  TclObj (*get_script)(TclInterp interp);

  /**
   * set_script sets the current script path.
   *
   * Called by the host when sourcing a file.
   * Pass nil or empty string to clear.
   */
  void (*set_script)(TclInterp interp, TclObj path);
} TclInterpOps;

/**
 * TclVarOps provide access to the interpreter's symbol table.
 *
 * Note that the results depend on the currently active evaluation frame.
 */
typedef struct TclVarOps {
  /**
   * get returns the value of the variable identified by name
   * in the current evaluation frame.
   */
  TclObj (*get)(TclInterp interp, TclObj name);

  /**
   * set sets the value of the variable name to value in
   * the current evaluation frame.
   */
  void (*set)(TclInterp interp, TclObj name, TclObj value);

  /**
   * unset removes the given variable from the current evaluation frame's
   * environment.
   *
   *
   */
  void (*unset)(TclInterp interp, TclObj name);

  /**
   * exists returns TCL_OK when the given variable exists
   * in the current evaluation frame.
   */
  TclResult (*exists)(TclInterp interp, TclObj name);

  /**
   * link creates a connection between the variable local
   * in the current evaluation frame and the variable target
   * in the frame indicated by target_level.
   *
   * The link affects get, set, unset, and exists.
   */
  void (*link)(TclInterp interp, TclObj local, size_t target_level,
               TclObj target);

  /**
   * link_ns creates a link from a local variable to a namespace variable.
   *
   * Used by 'variable' command. After linking, operations on the
   * local variable name affect the namespace variable:
   *   - get(local) returns ns.get_var(ns, name)
   *   - set(local, val) calls ns.set_var(ns, name, val)
   *   - exists(local) checks ns.var_exists(ns, name)
   *   - unset(local) calls ns.unset_var(ns, name)
   *
   * The 'local' parameter is the local name in the current frame.
   * The 'ns' parameter is the absolute namespace path.
   * The 'name' parameter is the variable name in the namespace.
   */
  void (*link_ns)(TclInterp interp, TclObj local, TclObj ns, TclObj name);

  /**
   * names returns a list of variable names.
   *
   * If ns is nil (0), returns variables in the current frame (locals).
   * If ns is "::", returns global namespace variables.
   * If ns is a namespace path, returns that namespace's variables.
   *
   * For frame-local queries (ns=nil), includes:
   * - Variables defined in the current frame
   * - Variables linked via upvar (shows local alias name)
   * - Variables linked via 'variable' (shows local alias name)
   *
   * Does NOT include variables from enclosing scopes that weren't
   * explicitly linked.
   */
  TclObj (*names)(TclInterp interp, TclObj ns);
} TclVarOps;

/**
 * TclCommandType indicates the type of a command in the unified command table.
 */
typedef enum {
  TCL_CMD_NONE = 0,    // command doesn't exist
  TCL_CMD_BUILTIN = 1, // it's a builtin command
  TCL_CMD_PROC = 2,    // it's a user-defined procedure
} TclCommandType;

/**
 * TclProcOps define operations on the interpreter's symbol table.
 *
 * Variables and procs exist in separate namespaces, so having a variable xyz
 * and a proc named xyz at the same time is perfectly valid.
 *
 * Procs are collected in namespaces, of which there is presently only one,
 * the global namespace called "::".
 *
 * The global namespace need not be explicitly stated when referring to entries
 * in the symbol table through this API.
 */
typedef struct TclProcOps {
  /**
   * define overwrites the symbol table entry with the given procedure.
   *
   * Nested namespaces are created automatically.
   */
  void (*define)(TclInterp interp, TclObj name, TclObj params, TclObj body);

  /**
   * exists reports whether a procedure entry with the given name exists.
   */
  int (*exists)(TclInterp interp, TclObj name);

  /**
   * params returns the parameter list associated with a procedure.
   *
   * Trying to retrieve the parameter list of a non-existing procedure
   * puts the interpreter into an error state as indicated by the result.
   */
  TclResult (*params)(TclInterp interp, TclObj name, TclObj *result);

  /**
   * body returns the body list associated with a procedure.
   *
   * Trying to retrieve the body list of a non-existing procedure
   * puts the interpreter into an error state as indicated by the result.
   */
  TclResult (*body)(TclInterp interp, TclObj name, TclObj *result);

  /**
   * names returns a list of all command names visible in the given namespace.
   *
   * If namespace is nil (0), uses the global namespace.
   * This includes builtins, user-defined procs, and host-registered commands.
   */
  TclObj (*names)(TclInterp interp, TclObj namespace);

  /**
   * resolve_namespace resolves a namespace path and returns the namespace object.
   *
   * If path is nil or empty, returns the global namespace object.
   * Returns TCL_ERROR if the namespace does not exist.
   */
  TclResult (*resolve_namespace)(TclInterp interp, TclObj path, TclObj *result);

  /**
   * register_builtin records a builtin command with its implementation.
   *
   * Used by tcl_interp_init to register builtin commands with the host.
   * The host stores the function pointer for later dispatch.
   */
  void (*register_builtin)(TclInterp interp, TclObj name, TclBuiltinCmd fn);

  /**
   * lookup checks if a command exists and returns its type.
   *
   * For builtins, sets *fn to the function pointer.
   * For procs, *fn is set to NULL.
   * For non-existent commands, returns TCL_CMD_NONE and *fn is NULL.
   */
  TclCommandType (*lookup)(TclInterp interp, TclObj name, TclBuiltinCmd *fn);

  /**
   * rename changes a command's name in the unified command table.
   *
   * If newName is empty (zero-length string), the command is deleted.
   * Returns TCL_ERROR if oldName doesn't exist or newName already exists.
   */
  TclResult (*rename)(TclInterp interp, TclObj oldName, TclObj newName);
} TclProcOps;

/**
 * TclListOps define the operations necessary for the interpreter to work with
 * lists.
 *
 * It is up to the host to decide whether lists are implemented as linked lists
 * or contiguous, dynamically growing arrays.  The internal lists the
 * interpreter uses are small.
 */
typedef struct TclListOps {
  /**
   * is_nil return true if the given object is the special nil object.
   */
  int (*is_nil)(TclInterp interp, TclObj obj);

  /**
   * create returns an empty, mutable list.
   */
  TclObj (*create)(TclInterp interp);

  /**
   * from returns a new list initialized from the given object.
   */
  TclObj (*from)(TclInterp interp, TclObj obj);

  /**
   * push appends an item to the end of list and returns the new list head.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   */
  TclObj (*push)(TclInterp interp, TclObj list, TclObj item);

  /**
   * pop removes the rightmost item from the list and returns it.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   *
   * popping the nil object returns nil.
   */
  TclObj (*pop)(TclInterp interp, TclObj list);

  /**
   * unshift prepends an item to the beginning of list and returns the new list
   * head.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   */
  TclObj (*unshift)(TclInterp interp, TclObj list, TclObj item);

  /**
   * shift removes the leftmost item from the list and returns it.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   *
   * shifting the nil object returns nil.
   */
  TclObj (*shift)(TclInterp interp, TclObj list);

  /**
   * length returns the length of the list.
   *
   * The length of the nil object is 0.
   */
  size_t (*length)(TclInterp interp, TclObj list);

  /**
   * at returns the element of list at the given index.
   *
   * If the index is out of bounds, the nil object is returned.
   */
  TclObj (*at)(TclInterp interp, TclObj list, size_t index);
} TclListOps;

/**
 * TclNamespaceOps provides operations on the namespace hierarchy.
 *
 * Namespaces are containers for commands and persistent variables.
 * The global namespace "::" always exists and is the root.
 * Namespace paths use "::" as separator (e.g., "::foo::bar").
 */
typedef struct TclNamespaceOps {
  /**
   * create ensures a namespace exists, creating it and parents as needed.
   *
   * Returns TCL_OK on success.
   * Creating "::" is a no-op (always exists).
   */
  TclResult (*create)(TclInterp interp, TclObj path);

  /**
   * delete removes a namespace and all its children.
   *
   * Variables and commands in the namespace are destroyed.
   * Returns TCL_ERROR if path is "::" (cannot delete global).
   * Returns TCL_ERROR if namespace doesn't exist.
   */
  TclResult (*delete)(TclInterp interp, TclObj path);

  /**
   * exists checks if a namespace exists.
   *
   * Returns 1 if it exists, 0 if not.
   */
  int (*exists)(TclInterp interp, TclObj path);

  /**
   * current returns the namespace path of the current call frame.
   *
   * Returns a string like "::" or "::foo::bar".
   */
  TclObj (*current)(TclInterp interp);

  /**
   * parent returns the parent namespace path.
   *
   * For "::", returns empty string.
   * For "::foo::bar", returns "::foo".
   * Returns TCL_ERROR if namespace doesn't exist.
   */
  TclResult (*parent)(TclInterp interp, TclObj ns, TclObj *result);

  /**
   * children returns a list of child namespace paths.
   *
   * Returns full paths (e.g., "::foo::bar" for child "bar" of "::foo").
   * Returns empty list if no children.
   */
  TclObj (*children)(TclInterp interp, TclObj ns);

  /**
   * get_var retrieves a variable from namespace storage.
   *
   * Returns nil if variable doesn't exist.
   * The 'name' parameter must be unqualified (just "x", not "::foo::x").
   * The namespace path must be absolute.
   */
  TclObj (*get_var)(TclInterp interp, TclObj ns, TclObj name);

  /**
   * set_var sets a variable in namespace storage.
   *
   * Creates the variable if it doesn't exist.
   * Creates the namespace if it doesn't exist.
   * The 'name' parameter must be unqualified.
   */
  void (*set_var)(TclInterp interp, TclObj ns, TclObj name, TclObj value);

  /**
   * var_exists checks if a variable exists in namespace storage.
   *
   * Returns 1 if exists, 0 if not.
   */
  int (*var_exists)(TclInterp interp, TclObj ns, TclObj name);

  /**
   * unset_var removes a variable from namespace storage.
   *
   * No-op if variable doesn't exist.
   */
  void (*unset_var)(TclInterp interp, TclObj ns, TclObj name);

  /**
   * get_command retrieves a command from a namespace.
   *
   * The 'name' parameter must be unqualified (just "set", not "::set").
   * The 'ns' parameter must be an absolute namespace path.
   *
   * For builtins, sets *fn to the function pointer and returns TCL_CMD_BUILTIN.
   * For procs, sets *fn to NULL and returns TCL_CMD_PROC.
   * Returns TCL_CMD_NONE if the command doesn't exist in this namespace.
   */
  TclCommandType (*get_command)(TclInterp interp, TclObj ns, TclObj name,
                                TclBuiltinCmd *fn);

  /**
   * set_command stores a command in a namespace.
   *
   * The 'name' parameter must be unqualified.
   * The 'ns' parameter must be an absolute namespace path.
   * Creates the namespace if it doesn't exist.
   *
   * For builtins: kind=TCL_CMD_BUILTIN, fn=function pointer, params=0, body=0
   * For procs: kind=TCL_CMD_PROC, fn=NULL, params=param list, body=body obj
   */
  void (*set_command)(TclInterp interp, TclObj ns, TclObj name,
                      TclCommandType kind, TclBuiltinCmd fn,
                      TclObj params, TclObj body);

  /**
   * delete_command removes a command from a namespace.
   *
   * Returns TCL_ERROR if the command doesn't exist.
   */
  TclResult (*delete_command)(TclInterp interp, TclObj ns, TclObj name);

  /**
   * list_commands returns a list of command names in a namespace.
   *
   * Returns simple names (not qualified).
   * Does NOT include commands from parent or child namespaces.
   */
  TclObj (*list_commands)(TclInterp interp, TclObj ns);

  /**
   * get_exports returns the list of export patterns for a namespace.
   *
   * Returns a list of pattern strings (e.g., {"get*", "set*"}).
   * Returns empty list if no exports defined.
   */
  TclObj (*get_exports)(TclInterp interp, TclObj ns);

  /**
   * set_exports sets the export patterns for a namespace.
   *
   * 'patterns' is a list of pattern strings.
   * If 'clear' is non-zero, existing patterns are replaced.
   * If 'clear' is zero, patterns are appended to existing list.
   */
  void (*set_exports)(TclInterp interp, TclObj ns, TclObj patterns, int clear);

  /**
   * is_exported checks if a command name matches any export pattern.
   *
   * 'name' is the simple (unqualified) command name.
   * Returns 1 if the command matches an export pattern, 0 otherwise.
   */
  int (*is_exported)(TclInterp interp, TclObj ns, TclObj name);

  /**
   * copy_command copies a command from one namespace to another.
   *
   * Used by namespace import. Copies the command entry from srcNs to dstNs.
   * 'srcName' and 'dstName' are simple (unqualified) names.
   * Returns TCL_ERROR if source command doesn't exist.
   */
  TclResult (*copy_command)(TclInterp interp, TclObj srcNs, TclObj srcName,
                            TclObj dstNs, TclObj dstName);
} TclNamespaceOps;

/**
 * TclTraceOps provides unified trace management for variables and commands.
 *
 * Traces are callbacks that fire when certain events occur on variables
 * or commands. The host is responsible for:
 * - Storing trace registrations
 * - Triggering traces at appropriate points (in var.get/set/unset, command dispatch)
 * - Executing trace scripts by calling back into the interpreter
 *
 * Trace operations (ops parameter) are strings:
 * - Variable: "read", "write", "unset", or combinations like "read write"
 * - Command: "rename", "delete"
 */
typedef struct TclTraceOps {
  /**
   * add registers a trace callback.
   *
   * kind: "variable" or "command"
   * name: the variable or command name to trace
   * ops: space-separated list of operations ("read write", "rename delete")
   * script: the command prefix to invoke when trace fires
   *
   * For variable traces, the script is invoked as: script name1 name2 op
   *   - name1: variable name
   *   - name2: empty (array element index, not supported)
   *   - op: "read", "write", or "unset"
   *
   * For command traces, the script is invoked as: script oldName newName op
   *   - oldName: original command name
   *   - newName: new name (empty for delete)
   *   - op: "rename" or "delete"
   */
  TclResult (*add)(TclInterp interp, TclObj kind, TclObj name,
                   TclObj ops, TclObj script);

  /**
   * remove unregisters a trace callback.
   *
   * All parameters must match a previously registered trace.
   * Returns TCL_ERROR if no matching trace found.
   */
  TclResult (*remove)(TclInterp interp, TclObj kind, TclObj name,
                      TclObj ops, TclObj script);

  /**
   * info returns a list of traces on a variable or command.
   *
   * Returns a list of {ops script} pairs for each registered trace.
   * Returns empty list if no traces.
   */
  TclObj (*info)(TclInterp interp, TclObj kind, TclObj name);
} TclTraceOps;

/**
 * TclBindOps defines the operations for host <> interpreter interop.
 */
typedef struct TclBindOpts {
  /**
   * unknown is invoked when the interpreter tries to invoke an undefined
   * procedure and gives the host a chance to react to this.
   *
   * If the host returns TCL_ERROR, the interpreter considers the procedure
   * lookup to have failed for good.
   */
  TclResult (*unknown)(TclInterp interp, TclObj cmd, TclObj args,
                       TclObj *value);
} TclBindOpts;

/**
 * TclHostOps contains the aggregation of all operations necessary
 * for interpreter to work.
 */
typedef struct TclHostOps {
  TclFrameOps frame;
  TclVarOps var;
  TclProcOps proc;
  TclNamespaceOps ns;
  TclStringOps string;
  TclListOps list;
  TclIntOps integer;
  TclDoubleOps dbl;
  TclInterpOps interp;
  TclBindOpts bind;
  TclTraceOps trace;
} TclHostOps;

/**
 * tcl_interp_init registers all builtin commands with the interpreter.
 *
 * This should be called once after creating the interpreter and before
 * evaluating any scripts.
 */
void tcl_interp_init(const TclHostOps *ops, TclInterp interp);

/**
 * tcl_builtin_set implements the TCL 'set' command.
 *
 * Usage:
 *   set varName ?value?
 *
 * With two arguments, sets varName to value and returns value.
 * With one argument, returns the current value of varName.
 * Errors if varName does not exist (one-argument form).
 */
TclResult tcl_builtin_set(const TclHostOps *ops, TclInterp interp,
                          TclObj cmd, TclObj args);

/**
 * tcl_builtin_expr implements the TCL 'expr' command.
 *
 * Usage:
 *   expr arg ?arg ...?
 *
 * Concatenates arguments with spaces and evaluates as an expression.
 * Supports comparison (<, <=, >, >=, ==, !=), bitwise (&, |),
 * and logical (&&, ||) operators on integers.
 */
TclResult tcl_builtin_expr(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args);

/**
 * tcl_strlen counts bytes in a null-terminated C string, excluding the null.
 *
 * This is equivalent to strlen but avoids stdlib dependency.
 */
size_t tcl_strlen(const char *s);

/**
 * tcl_glob_match performs glob pattern matching.
 *
 * Supports:
 *   * - matches any sequence of characters (including empty)
 *   ? - matches any single character
 *   [...] - matches any character in the set
 *   [^...] or [!...] - matches any character NOT in the set
 *   \x - matches x literally (escape)
 *
 * Returns 1 if string matches pattern, 0 otherwise.
 */
int tcl_glob_match(const char *pattern, size_t pattern_len,
                   const char *string, size_t string_len);

/**
 * tcl_is_qualified returns 1 if name contains "::", 0 otherwise.
 */
int tcl_is_qualified(const char *name, size_t len);

/**
 * tcl_resolve_variable resolves a variable name to namespace + local parts.
 *
 * Three cases:
 *   1. Unqualified ("x") - no "::" in name
 *      -> ns_out = nil, local_out = "x"
 *      -> Caller uses var.get for frame-local lookup
 *
 *   2. Absolute ("::foo::x") - starts with "::"
 *      -> ns_out = "::foo", local_out = "x"
 *
 *   3. Relative ("foo::x") - contains "::" but doesn't start with it
 *      -> Prepends current namespace from ops->ns.current()
 *      -> If current is "::bar", resolves to ns="::bar::foo", local="x"
 *
 * Returns TCL_OK on success.
 */
TclResult tcl_resolve_variable(const TclHostOps *ops, TclInterp interp,
                               const char *name, size_t len,
                               TclObj *ns_out, TclObj *local_out);

#endif
