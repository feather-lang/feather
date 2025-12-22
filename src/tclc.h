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

/**
 * tcl_eval_string evaluates a string in the context of the given interpreter.
 *
 * The result of the evaluation is in the interpreter's result slot.
 */
TclResult tcl_eval_string(const TclHostOps *ops, TclInterp interp,
                          const char *script, size_t len, TclEvalFlags flags);
/**
 * tcl_eval_obj evaluates the script contained in the object
 * in the context of the given interpreter.
 *
 * The result of the evaluation is in the interpreter's result slot.
 */
TclResult tcl_eval_obj(const TclHostOps *ops, TclInterp interp, TclObj script,
                       TclEvalFlags flags);

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

} TclVarOps;

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
  TclStringOps string;
  TclListOps list;
  TclIntOps integer;
  TclDoubleOps dbl;
  TclInterpOps interp;
  TclBindOpts bind;
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

#endif
