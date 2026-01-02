#ifndef INCLUDE_FEATHER
#define INCLUDE_FEATHER

#include <stddef.h>
#include <stdint.h>

/**
 * feather is an embeddable implementation of the core TCL language.
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
 * feather wants to be the thin glue layer that's easy to embed into your programs,
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
 * - OO: feather intended use case is short, interactive programs
 * similar to bash. Programming in the large is explicitly not supported.
 *
 * - Coroutines: feather interpreter objects are small and lightweight so you can
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
typedef uintptr_t FeatherHandle;

/** A handle to an interpreter instance */
typedef FeatherHandle FeatherInterp;

/** A handle to an object */
typedef FeatherHandle FeatherObj;

/**
 * FeatherHostOps contains all operations that the host needs to support for
 * this interpreter to work.
 */
typedef struct FeatherHostOps FeatherHostOps;

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
} FeatherResult;

/**
 * FeatherBuiltinCmd is the signature for builtin command implementations.
 *
 * Builtin commands receive the host operations, interpreter, command name,
 * and argument list. They return a result code and set the interpreter's
 * result via ops->interp.set_result.
 */
typedef FeatherResult (*FeatherBuiltinCmd)(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj cmd, FeatherObj args);

/**
 * FeatherTokenType encodes the types of tokens returned by the parser.
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
} FeatherTokenType;

/**
 * FeatherParseStatus informs the caller about whether and how the parser
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
} FeatherParseStatus;

/**
 * FeatherParseContext holds the state for iterating over commands in a script.
 * (char* based - for backward compatibility)
 */
typedef struct {
  const char *script;  // Original script
  size_t len;          // Total length
  size_t pos;          // Current position
  size_t line;         // Current line number (1-based)
  size_t cmd_line;     // Line number where current command started
} FeatherParseContext;

/**
 * FeatherParseContextObj holds the state for iterating over commands using
 * object-based byte access. Preferred over FeatherParseContext.
 */
typedef struct {
  FeatherObj script;   // Script as object
  size_t len;          // Total length
  size_t pos;          // Current position
  size_t line;         // Current line number (1-based)
  size_t cmd_line;     // Line number where current command started
} FeatherParseContextObj;

/**
 * feather_parse_init initializes a parse context for iterating over commands.
 * (char* based - for backward compatibility)
 */
void feather_parse_init(FeatherParseContext *ctx, const char *script, size_t len);

/**
 * feather_parse_init_obj initializes an object-based parse context.
 */
void feather_parse_init_obj(FeatherParseContextObj *ctx, FeatherObj script, size_t len);

/**
 * feather_parse_command parses the next command from the script.
 * (char* based - for backward compatibility)
 *
 * Returns TCL_PARSE_OK when a command was parsed successfully.
 * The parsed command (list of words) is in the interpreter's result slot.
 *
 * Returns TCL_PARSE_DONE when the script is exhausted.
 *
 * Returns TCL_PARSE_INCOMPLETE or TCL_PARSE_ERROR on failure,
 * with error information in the interpreter's result slot.
 */
FeatherParseStatus feather_parse_command(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherParseContext *ctx);

/**
 * feather_parse_command_obj parses the next command using object-based access.
 * Preferred over feather_parse_command.
 */
FeatherParseStatus feather_parse_command_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherParseContextObj *ctx);

typedef enum {
  // Evaluate in the current scope of the interpreter
  TCL_EVAL_LOCAL = 0,
  // Evaluate in the interpreter's global scope
  TCL_EVAL_GLOBAL = 1,
} FeatherEvalFlags;

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
 * feather_command_exec executes a single parsed command.
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
FeatherResult feather_command_exec(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj command, FeatherEvalFlags flags);

/**
 * feather_script_eval evaluates a script string.
 *
 * Parses each command and executes it. Stops on error or when
 * a command returns a non-OK code (break/continue/return).
 *
 * Lisp equivalent: (PROGN (EVAL (READ s)) ...) for each command in s,
 * but commands are executed as they're parsed, not batched.
 *
 * The result of the last command is in the interpreter's result slot.
 */
FeatherResult feather_script_eval(const FeatherHostOps *ops, FeatherInterp interp,
                          const char *source, size_t len, FeatherEvalFlags flags);

/**
 * feather_script_eval_obj evaluates a script object.
 *
 * Gets the string representation of the object and evaluates it
 * as a script. This is what control structures (if, while, catch, proc)
 * use to evaluate their body arguments.
 *
 * Lisp equivalent: (EVAL obj) where obj is expected to contain source code.
 *
 * The result is in the interpreter's result slot.
 */
FeatherResult feather_script_eval_obj(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj script, FeatherEvalFlags flags);

/**
 * Flags for feather_subst controlling which substitutions to perform.
 */
typedef enum {
  TCL_SUBST_BACKSLASHES = 1,
  TCL_SUBST_VARIABLES = 2,
  TCL_SUBST_COMMANDS = 4,
  TCL_SUBST_ALL = 7
} FeatherSubstFlags;

/**
 * feather_subst performs substitutions on a string.
 * (char* based - for backward compatibility)
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
FeatherResult feather_subst(const FeatherHostOps *ops, FeatherInterp interp,
                    const char *str, size_t len, int flags);

/**
 * feather_subst_obj performs substitutions using object-based access.
 * Preferred over feather_subst.
 */
FeatherResult feather_subst_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj str, int flags);

/**
 * The heart of the implementation.  An embedder needs to provide all of the
 * following operations.
 *
 * The rest of the interpreter is implemented in terms of these.
 */

/**
 * FeatherFrameOps describe the operations on execution frames.
 *
 * Frames contain:
 * - the variable environment in which expressions are evaluated,
 * - the command currently being evaluated,
 * - the return code of that command,
 * - the result object for holding the result of the evaluation,
 * - the error object in case of an error,
 * - their index on the call stack.
 */
typedef struct FeatherFrameOps {
  /**
   * push adds a new call frame to the stack for the evaluation of cmd and args.
   */
  FeatherResult (*push)(FeatherInterp interp, FeatherObj cmd, FeatherObj args);

  /**
   * pop removes the topmost frame from the callstack.
   */
  FeatherResult (*pop)(FeatherInterp interp);

  /**
   * level returns the current level of the call stack.
   */
  size_t (*level)(FeatherInterp interp);

  /**
   * set_active makes the provided frame the active frame on the call stack.
   */
  FeatherResult (*set_active)(FeatherInterp interp, size_t level);

  /**
   * size returns the size of the call stack.
   *
   * This is important because the level reported by level can
   * be less than the size because of a prior call to set_active.
   */
  size_t (*size)(FeatherInterp interp);

  /**
   * info returns information about a frame at the given level.
   *
   * Sets *cmd and *args to the command and arguments at that level.
   * Sets *ns to the namespace the frame is executing in.
   * Returns TCL_ERROR if the level is out of bounds.
   */
  FeatherResult (*info)(FeatherInterp interp, size_t level, FeatherObj *cmd, FeatherObj *args,
                    FeatherObj *ns);

  /**
   * set_namespace changes the namespace of the current frame.
   *
   * Used by 'namespace eval' to temporarily change context.
   * The namespace is created if it doesn't exist.
   */
  FeatherResult (*set_namespace)(FeatherInterp interp, FeatherObj ns);

  /**
   * get_namespace returns the namespace of the current frame.
   */
  FeatherObj (*get_namespace)(FeatherInterp interp);

  /**
   * set_line sets the line number for the current frame.
   * Used to track source location for debugging and error reporting.
   */
  FeatherResult (*set_line)(FeatherInterp interp, size_t line);

  /**
   * get_line returns the line number for a frame at the given level.
   * Returns 0 if no line info is available.
   */
  size_t (*get_line)(FeatherInterp interp, size_t level);

  /**
   * set_lambda stores the lambda expression for the current frame.
   * Used by apply to record the lambda for info frame.
   */
  FeatherResult (*set_lambda)(FeatherInterp interp, FeatherObj lambda);

  /**
   * get_lambda returns the lambda expression for a frame at the given level.
   * Returns 0 if no lambda info is available (not an apply frame).
   */
  FeatherObj (*get_lambda)(FeatherInterp interp, size_t level);
} FeatherFrameOps;

/**
 * FeatherStringOps describes the string operations the host needs to support.
 *
 * Strings are sequences of bytes. The C code never holds pointers into host
 * memory - all string content stays in the host (Go/JS), managed by the host's
 * garbage collector.
 *
 * feather is encoding neutral, as strings are managed by the host and all
 * characters with special meaning to the parser are part of ASCII.
 */
typedef struct FeatherStringOps {
  /**
   * byte_at returns the byte at the given index, or -1 if out of bounds.
   * Index is 0-based. This is the primary way C accesses string content.
   */
  int (*byte_at)(FeatherInterp interp, FeatherObj str, size_t index);

  /**
   * byte_length returns the length of the string in bytes.
   */
  size_t (*byte_length)(FeatherInterp interp, FeatherObj str);

  /**
   * slice returns a new string object containing bytes [start, end).
   * Returns empty string if start >= end or start >= length.
   */
  FeatherObj (*slice)(FeatherInterp interp, FeatherObj str, size_t start, size_t end);

  /**
   * concat returns a new object whose string value is
   * the concatenation of two objects.
   */
  FeatherObj (*concat)(FeatherInterp interp, FeatherObj a, FeatherObj b);

  /**
   * compare compares two strings using Unicode ordering.
   * Returns <0 if a < b, 0 if a == b, >0 if a > b.
   */
  int (*compare)(FeatherInterp interp, FeatherObj a, FeatherObj b);

  /**
   * equal returns 1 if strings are byte-equal, 0 otherwise.
   * More efficient than compare when only equality is needed.
   */
  int (*equal)(FeatherInterp interp, FeatherObj a, FeatherObj b);

  /**
   * match tests if string matches a glob pattern.
   * Returns 1 if matches, 0 otherwise.
   */
  int (*match)(FeatherInterp interp, FeatherObj pattern, FeatherObj str, int nocase);

  /**
   * regex_match tests if a string matches a regular expression pattern.
   *
   * Returns TCL_OK and sets *result to 1 if the string matches the pattern,
   * or 0 if it doesn't match. Returns TCL_ERROR if the pattern is invalid,
   * with an error message in the interpreter's result.
   *
   * Parameters:
   *   nocase  - If non-zero, perform case-insensitive matching
   *   result  - Set to 1 on match, 0 on no match
   *   matches - If non-NULL, set to list of matched strings [full, group1, ...]
   *   indices - If non-NULL, set to list of {start end} pairs for each group
   *
   * When matches/indices are NULL, only the boolean result is computed.
   * Indices are inclusive character positions (not byte offsets).
   */
  FeatherResult (*regex_match)(FeatherInterp interp, FeatherObj pattern, FeatherObj string,
                               int nocase, int *result,
                               FeatherObj *matches, FeatherObj *indices);

  /**
   * builder_new creates a new string builder with optional initial capacity.
   */
  FeatherObj (*builder_new)(FeatherInterp interp, size_t capacity);

  /**
   * builder_append_byte appends a single byte to the builder.
   */
  void (*builder_append_byte)(FeatherInterp interp, FeatherObj builder, int byte);

  /**
   * builder_append_obj appends another string object's bytes to the builder.
   */
  void (*builder_append_obj)(FeatherInterp interp, FeatherObj builder, FeatherObj str);

  /**
   * builder_finish converts builder to immutable string, returns handle.
   * The builder handle becomes invalid after this call.
   */
  FeatherObj (*builder_finish)(FeatherInterp interp, FeatherObj builder);

  /**
   * intern returns a cached value for the given string s,
   * caching it if not present yet.
   *
   * DEPRECATED: This will be removed. Use builder operations instead.
   */
  FeatherObj (*intern)(FeatherInterp interp, const char *s, size_t len);
} FeatherStringOps;

/**
 * FeatherRuneOps provides Unicode-aware character operations.
 *
 * These operations work with Unicode code points (runes) rather than bytes.
 * The host is responsible for proper UTF-8 handling.
 */
typedef struct FeatherRuneOps {
  /**
   * length returns the number of Unicode code points in a string.
   *
   * For UTF-8 encoded strings, this counts runes, not bytes.
   * Example: "héllo" has 5 runes but 6 bytes.
   */
  size_t (*length)(FeatherInterp interp, FeatherObj str);

  /**
   * at returns the nth Unicode character as a new string object.
   *
   * Index is 0-based. Returns empty string if index is out of bounds.
   * The returned string contains the single character at that position.
   */
  FeatherObj (*at)(FeatherInterp interp, FeatherObj str, size_t index);

  /**
   * range returns substring from first to last (inclusive) by character index.
   *
   * Indices are 0-based. Negative indices are treated as 0.
   * If last >= length, it is clamped to length-1.
   * Returns empty string if first > last or string is empty.
   */
  FeatherObj (*range)(FeatherInterp interp, FeatherObj str, int64_t first, int64_t last);

  /**
   * to_upper returns a new string with Unicode-aware uppercase conversion.
   *
   * Handles non-ASCII characters (é→É, ß→SS, etc.)
   */
  FeatherObj (*to_upper)(FeatherInterp interp, FeatherObj str);

  /**
   * to_lower returns a new string with Unicode-aware lowercase conversion.
   *
   * Handles non-ASCII characters (É→é, etc.)
   */
  FeatherObj (*to_lower)(FeatherInterp interp, FeatherObj str);

  /**
   * fold returns case-folded string for case-insensitive comparison.
   *
   * Case folding is more appropriate than lowercasing for comparison.
   * For example, German ß folds to "ss".
   */
  FeatherObj (*fold)(FeatherInterp interp, FeatherObj str);
} FeatherRuneOps;

/**
 * FeatherIntOps gives access to integers from the host.
 */
typedef struct FeatherIntOps {
  /**
   * create requests a possibly new integer from the host.
   */
  FeatherObj (*create)(FeatherInterp interp, int64_t val);

  /**
   * get extracts the integer value from an object.
   *
   * This can cause a conversion of the object's internal representation to an
   * integer.
   */
  FeatherResult (*get)(FeatherInterp interp, FeatherObj obj, int64_t *out);
} FeatherIntOps;

/**
 * FeatherDoubleClass classifies floating-point values for special value detection.
 */
typedef enum FeatherDoubleClass {
  FEATHER_DBL_NORMAL    = 0, /* Finite, non-zero, normalized */
  FEATHER_DBL_ZERO      = 1, /* Positive or negative zero */
  FEATHER_DBL_INF       = 2, /* Positive infinity */
  FEATHER_DBL_NEG_INF   = 3, /* Negative infinity */
  FEATHER_DBL_NAN       = 4, /* Not a number */
  FEATHER_DBL_SUBNORMAL = 5, /* Subnormal (denormalized) */
} FeatherDoubleClass;

/**
 * FeatherMathOp identifies transcendental and special math operations.
 * Unary operations use parameter 'a' only; binary operations use both 'a' and 'b'.
 */
typedef enum FeatherMathOp {
  /* Unary operations */
  FEATHER_MATH_SQRT,
  FEATHER_MATH_EXP,
  FEATHER_MATH_LOG,
  FEATHER_MATH_LOG10,
  FEATHER_MATH_SIN,
  FEATHER_MATH_COS,
  FEATHER_MATH_TAN,
  FEATHER_MATH_ASIN,
  FEATHER_MATH_ACOS,
  FEATHER_MATH_ATAN,
  FEATHER_MATH_SINH,
  FEATHER_MATH_COSH,
  FEATHER_MATH_TANH,
  FEATHER_MATH_FLOOR,
  FEATHER_MATH_CEIL,
  FEATHER_MATH_ROUND,
  FEATHER_MATH_ABS,

  /* Binary operations */
  FEATHER_MATH_POW,
  FEATHER_MATH_ATAN2,
  FEATHER_MATH_FMOD,
  FEATHER_MATH_HYPOT,
} FeatherMathOp;

/**
 * FeatherDoubleOps gives access to floating-point numbers from the host.
 */
typedef struct FeatherDoubleOps {
  /**
   * create requests a possibly new double from the host.
   */
  FeatherObj (*create)(FeatherInterp interp, double val);

  /**
   * get extracts the double value from an object.
   *
   * This can cause a conversion of the object's internal representation to a
   * double.
   */
  FeatherResult (*get)(FeatherInterp interp, FeatherObj obj, double *out);

  /**
   * classify returns the classification of a double value.
   * Used to detect special values (Inf, -Inf, NaN) without stdlib.
   */
  FeatherDoubleClass (*classify)(double val);

  /**
   * format converts a double to a string object.
   * Handles special values (Inf, -Inf, NaN) and format specifiers.
   * @param specifier One of 'e', 'f', 'g', 'E', 'G'
   * @param precision Number of decimal places (-1 for default)
   */
  FeatherObj (*format)(FeatherInterp interp, double val, char specifier,
                       int precision);

  /**
   * math computes a transcendental or special math operation.
   * @param op The operation to perform
   * @param a First operand (used by all operations)
   * @param b Second operand (used only by binary operations)
   * @param out Result is written here on success
   * @return TCL_OK on success, TCL_ERROR on domain error
   */
  FeatherResult (*math)(FeatherInterp interp, FeatherMathOp op, double a,
                        double b, double *out);
} FeatherDoubleOps;

/**
 * FeatherInterpOps holds the operations on the state of the
 * interpreter instance.
 *
 * @see https://www.tcl-lang.org/man/tcl9.0/FeatherLib/AddErrInfo.html
 * @see https://www.tcl-lang.org/man/tcl9.0/FeatherLib/SetResult.html
 */
typedef struct FeatherInterpOps {
  /**
   * set_result sets the interpreter's result object.
   */
  FeatherResult (*set_result)(FeatherInterp interp, FeatherObj result);

  /**
   * get_result returns the interpreter's result object.
   */
  FeatherObj (*get_result)(FeatherInterp interp);

  /**
   * reset_result clears the interpreters evaluation state
   * like the current evaluation result and error information.
   */
  FeatherResult (*reset_result)(FeatherInterp interp, FeatherObj result);

  /**
   * set_return_options corresponds to the options passed to return.
   *
   * Sets the return options of interp to be options.
   * If options contains any invalid value for any key, TCL_ERROR will be
   * returned, and the interp result will be set to an appropriate error
   * message. Otherwise, a completion code in agreement with the -code and
   * -level keys in options will be returned.
   */
  FeatherResult (*set_return_options)(FeatherInterp interp, FeatherObj options);

  /**
   * get_return_options returns the options passed to the return command.
   *
   * Retrieves the dictionary of return options from an interpreter following a
   * script evaluation.
   *
   * Routines such as feather_eval are called to evaluate a
   * script in an interpreter.
   *
   * These routines return an integer completion code.
   *
   * These routines also leave in the interpreter both a result and a dictionary
   * of return options generated by script evaluation.
   */
  FeatherObj (*get_return_options)(FeatherInterp interp, FeatherResult code);

  /**
   * get_script returns the path of the currently executing script file.
   *
   * Returns empty string if no script file is being evaluated
   * (e.g., interactive input or eval'd string).
   */
  FeatherObj (*get_script)(FeatherInterp interp);

  /**
   * set_script sets the current script path.
   *
   * Called by the host when sourcing a file.
   * Pass nil or empty string to clear.
   */
  void (*set_script)(FeatherInterp interp, FeatherObj path);
} FeatherInterpOps;

/**
 * FeatherVarOps provide access to the interpreter's symbol table.
 *
 * Note that the results depend on the currently active evaluation frame.
 */
typedef struct FeatherVarOps {
  /**
   * get returns the value of the variable identified by name
   * in the current evaluation frame.
   */
  FeatherObj (*get)(FeatherInterp interp, FeatherObj name);

  /**
   * set sets the value of the variable name to value in
   * the current evaluation frame.
   */
  void (*set)(FeatherInterp interp, FeatherObj name, FeatherObj value);

  /**
   * unset removes the given variable from the current evaluation frame's
   * environment.
   *
   *
   */
  void (*unset)(FeatherInterp interp, FeatherObj name);

  /**
   * exists returns TCL_OK when the given variable exists
   * in the current evaluation frame.
   */
  FeatherResult (*exists)(FeatherInterp interp, FeatherObj name);

  /**
   * link creates a connection between the variable local
   * in the current evaluation frame and the variable target
   * in the frame indicated by target_level.
   *
   * The link affects get, set, unset, and exists.
   */
  void (*link)(FeatherInterp interp, FeatherObj local, size_t target_level,
               FeatherObj target);

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
  void (*link_ns)(FeatherInterp interp, FeatherObj local, FeatherObj ns, FeatherObj name);

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
  FeatherObj (*names)(FeatherInterp interp, FeatherObj ns);

  /**
   * is_link checks if a variable in the current frame is a link.
   *
   * Returns 1 if the variable is linked to another location
   * (via upvar, global, or variable commands).
   * Returns 0 if it's a true local variable defined in this frame.
   *
   * Only meaningful for frame-local variables. For namespace variables,
   * always returns 0.
   */
  int (*is_link)(FeatherInterp interp, FeatherObj name);

  /**
   * resolve_link follows variable links to find the target variable name.
   *
   * If the variable is a link (via upvar, global, or variable commands),
   * returns the name of the target variable that traces should be looked up on.
   * If the variable is not a link, returns the original name.
   *
   * This is needed for trace support: traces are registered on the actual
   * variable, but when accessed via a link, the trace should be called with
   * the local (link) name while being looked up by the target name.
   */
  FeatherObj (*resolve_link)(FeatherInterp interp, FeatherObj name);
} FeatherVarOps;

/**
 * FeatherCommandType indicates the type of a command in the unified command table.
 */
typedef enum {
  TCL_CMD_NONE = 0,    // command doesn't exist
  TCL_CMD_BUILTIN = 1, // it's a builtin command
  TCL_CMD_PROC = 2,    // it's a user-defined procedure
} FeatherCommandType;

/**
 * FeatherListOps define the operations necessary for the interpreter to work with
 * lists.
 *
 * It is up to the host to decide whether lists are implemented as linked lists
 * or contiguous, dynamically growing arrays.  The internal lists the
 * interpreter uses are small.
 */
typedef struct FeatherListOps {
  /**
   * is_nil return true if the given object is the special nil object.
   */
  int (*is_nil)(FeatherInterp interp, FeatherObj obj);

  /**
   * create returns an empty, mutable list.
   */
  FeatherObj (*create)(FeatherInterp interp);

  /**
   * from returns a new list initialized from the given object.
   */
  FeatherObj (*from)(FeatherInterp interp, FeatherObj obj);

  /**
   * push appends an item to the end of list and returns the new list head.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   */
  FeatherObj (*push)(FeatherInterp interp, FeatherObj list, FeatherObj item);

  /**
   * pop removes the rightmost item from the list and returns it.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   *
   * popping the nil object returns nil.
   */
  FeatherObj (*pop)(FeatherInterp interp, FeatherObj list);

  /**
   * unshift prepends an item to the beginning of list and returns the new list
   * head.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   */
  FeatherObj (*unshift)(FeatherInterp interp, FeatherObj list, FeatherObj item);

  /**
   * shift removes the leftmost item from the list and returns it.
   *
   * If the host decides to mutate the underlying list, returning the list
   * object is expected.
   *
   * shifting the nil object returns nil.
   */
  FeatherObj (*shift)(FeatherInterp interp, FeatherObj list);

  /**
   * length returns the length of the list.
   *
   * The length of the nil object is 0.
   */
  size_t (*length)(FeatherInterp interp, FeatherObj list);

  /**
   * at returns the element of list at the given index.
   *
   * If the index is out of bounds, the nil object is returned.
   */
  FeatherObj (*at)(FeatherInterp interp, FeatherObj list, size_t index);

  /**
   * slice returns a new list containing elements from first to last (inclusive).
   *
   * Indices are 0-based. Out-of-bounds indices are clamped to valid range.
   * Returns empty list if first > last or list is empty.
   *
   * This enables O(n) lrange where n is slice size, not source list size.
   */
  FeatherObj (*slice)(FeatherInterp interp, FeatherObj list, size_t first, size_t last);

  /**
   * set_at replaces the element at index with value.
   *
   * Returns TCL_ERROR if index is out of bounds.
   * Mutates the list in place if the host implementation supports it.
   *
   * This enables O(1) lset instead of O(n) list rebuild.
   */
  FeatherResult (*set_at)(FeatherInterp interp, FeatherObj list, size_t index, FeatherObj value);

  /**
   * splice removes 'deleteCount' elements starting at 'first',
   * then inserts all elements from 'insertions' at that position.
   * Returns the modified list.
   *
   * For lreplace list first last ?elem ...?:
   *   splice(list, first, last-first+1, insertions)
   *
   * This enables O(n) lreplace instead of O(n²).
   */
  FeatherObj (*splice)(FeatherInterp interp, FeatherObj list, size_t first,
                   size_t deleteCount, FeatherObj insertions);

  /**
   * sort sorts the list in place using the provided comparison function.
   *
   * The comparison function receives two elements and user context.
   * It should return <0 if a<b, 0 if a==b, >0 if a>b.
   *
   * The host can use its native O(n log n) sort algorithm.
   * This removes the 1024 element limit and O(n²) insertion sort.
   */
  FeatherResult (*sort)(FeatherInterp interp, FeatherObj list,
                    int (*cmp)(FeatherInterp interp, FeatherObj a, FeatherObj b, void *ctx),
                    void *ctx);
} FeatherListOps;

/**
 * FeatherDictOps provides operations on dictionaries.
 *
 * Dictionaries are ordered key-value maps. Keys are strings, values are
 * arbitrary TCL objects. Insertion order is preserved for iteration.
 *
 * The host is responsible for providing an efficient implementation.
 * Typical backing: Go map with separate slice for key order,
 * or a linked hash map in other languages.
 */
typedef struct FeatherDictOps {
  /**
   * create returns a new empty dictionary.
   */
  FeatherObj (*create)(FeatherInterp interp);

  /**
   * is_dict checks if an object is natively a dictionary.
   *
   * Returns 1 if the object was created as a dict (via dict create,
   * dict set, etc.) and still has dict representation.
   * Returns 0 for lists, strings, etc. even if they could be
   * converted to a dict.
   */
  int (*is_dict)(FeatherInterp interp, FeatherObj obj);

  /**
   * from converts a list or string to a dictionary.
   *
   * The input must have an even number of elements (key-value pairs).
   * Returns 0 (nil) if the conversion fails (odd number of elements,
   * or input is not a valid list).
   *
   * Duplicate keys: only the last value for each key is retained.
   */
  FeatherObj (*from)(FeatherInterp interp, FeatherObj obj);

  /**
   * get retrieves the value for a key.
   *
   * Returns 0 (nil) if the key does not exist.
   */
  FeatherObj (*get)(FeatherInterp interp, FeatherObj dict, FeatherObj key);

  /**
   * set stores a key-value pair in the dictionary.
   *
   * If the key already exists, its value is updated.
   * If the key is new, it is appended to the key order.
   * Returns the (possibly new) dictionary object.
   */
  FeatherObj (*set)(FeatherInterp interp, FeatherObj dict, FeatherObj key, FeatherObj value);

  /**
   * exists checks if a key is present in the dictionary.
   *
   * Returns 1 if the key exists, 0 otherwise.
   */
  int (*exists)(FeatherInterp interp, FeatherObj dict, FeatherObj key);

  /**
   * remove deletes a key from the dictionary.
   *
   * Returns the (possibly new) dictionary object.
   * Removing a non-existent key is a no-op.
   */
  FeatherObj (*remove)(FeatherInterp interp, FeatherObj dict, FeatherObj key);

  /**
   * size returns the number of key-value pairs.
   */
  size_t (*size)(FeatherInterp interp, FeatherObj dict);

  /**
   * keys returns a list of all keys in insertion order.
   */
  FeatherObj (*keys)(FeatherInterp interp, FeatherObj dict);

  /**
   * values returns a list of all values in key order.
   */
  FeatherObj (*values)(FeatherInterp interp, FeatherObj dict);
} FeatherDictOps;

/**
 * FeatherNamespaceOps provides operations on the namespace hierarchy.
 *
 * Namespaces are containers for commands and persistent variables.
 * The global namespace "::" always exists and is the root.
 * Namespace paths use "::" as separator (e.g., "::foo::bar").
 */
typedef struct FeatherNamespaceOps {
  /**
   * create ensures a namespace exists, creating it and parents as needed.
   *
   * Returns TCL_OK on success.
   * Creating "::" is a no-op (always exists).
   */
  FeatherResult (*create)(FeatherInterp interp, FeatherObj path);

  /**
   * delete removes a namespace and all its children.
   *
   * Variables and commands in the namespace are destroyed.
   * Returns TCL_ERROR if path is "::" (cannot delete global).
   * Returns TCL_ERROR if namespace doesn't exist.
   */
  FeatherResult (*delete)(FeatherInterp interp, FeatherObj path);

  /**
   * exists checks if a namespace exists.
   *
   * Returns 1 if it exists, 0 if not.
   */
  int (*exists)(FeatherInterp interp, FeatherObj path);

  /**
   * current returns the namespace path of the current call frame.
   *
   * Returns a string like "::" or "::foo::bar".
   */
  FeatherObj (*current)(FeatherInterp interp);

  /**
   * parent returns the parent namespace path.
   *
   * For "::", returns empty string.
   * For "::foo::bar", returns "::foo".
   * Returns TCL_ERROR if namespace doesn't exist.
   */
  FeatherResult (*parent)(FeatherInterp interp, FeatherObj ns, FeatherObj *result);

  /**
   * children returns a list of child namespace paths.
   *
   * Returns full paths (e.g., "::foo::bar" for child "bar" of "::foo").
   * Returns empty list if no children.
   */
  FeatherObj (*children)(FeatherInterp interp, FeatherObj ns);

  /**
   * get_var retrieves a variable from namespace storage.
   *
   * Returns nil if variable doesn't exist.
   * The 'name' parameter must be unqualified (just "x", not "::foo::x").
   * The namespace path must be absolute.
   */
  FeatherObj (*get_var)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

  /**
   * set_var sets a variable in namespace storage.
   *
   * Creates the variable if it doesn't exist.
   * Creates the namespace if it doesn't exist.
   * The 'name' parameter must be unqualified.
   */
  void (*set_var)(FeatherInterp interp, FeatherObj ns, FeatherObj name, FeatherObj value);

  /**
   * var_exists checks if a variable exists in namespace storage.
   *
   * Returns 1 if exists, 0 if not.
   */
  int (*var_exists)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

  /**
   * unset_var removes a variable from namespace storage.
   *
   * No-op if variable doesn't exist.
   */
  void (*unset_var)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

  /**
   * get_command retrieves a command from a namespace.
   *
   * The 'name' parameter must be unqualified (just "set", not "::set").
   * The 'ns' parameter must be an absolute namespace path.
   *
   * For builtins: sets *fn to function pointer, *params and *body to 0.
   * For procs: sets *fn to NULL, *params to parameter list, *body to body.
   * Returns TCL_CMD_NONE if the command doesn't exist in this namespace.
   *
   * The params and body pointers may be NULL if not needed.
   */
  FeatherCommandType (*get_command)(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                                FeatherBuiltinCmd *fn, FeatherObj *params, FeatherObj *body);

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
  void (*set_command)(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                      FeatherCommandType kind, FeatherBuiltinCmd fn,
                      FeatherObj params, FeatherObj body);

  /**
   * delete_command removes a command from a namespace.
   *
   * Returns TCL_ERROR if the command doesn't exist.
   */
  FeatherResult (*delete_command)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

  /**
   * list_commands returns a list of command names in a namespace.
   *
   * Returns simple names (not qualified).
   * Does NOT include commands from parent or child namespaces.
   */
  FeatherObj (*list_commands)(FeatherInterp interp, FeatherObj ns);

  /**
   * get_exports returns the list of export patterns for a namespace.
   *
   * Returns a list of pattern strings (e.g., {"get*", "set*"}).
   * Returns empty list if no exports defined.
   */
  FeatherObj (*get_exports)(FeatherInterp interp, FeatherObj ns);

  /**
   * set_exports sets the export patterns for a namespace.
   *
   * 'patterns' is a list of pattern strings.
   * If 'clear' is non-zero, existing patterns are replaced.
   * If 'clear' is zero, patterns are appended to existing list.
   */
  void (*set_exports)(FeatherInterp interp, FeatherObj ns, FeatherObj patterns, int clear);

  /**
   * is_exported checks if a command name matches any export pattern.
   *
   * 'name' is the simple (unqualified) command name.
   * Returns 1 if the command matches an export pattern, 0 otherwise.
   */
  int (*is_exported)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

  /**
   * copy_command copies a command from one namespace to another.
   *
   * Used by namespace import. Copies the command entry from srcNs to dstNs.
   * 'srcName' and 'dstName' are simple (unqualified) names.
   * Returns TCL_ERROR if source command doesn't exist.
   */
  FeatherResult (*copy_command)(FeatherInterp interp, FeatherObj srcNs, FeatherObj srcName,
                            FeatherObj dstNs, FeatherObj dstName);
} FeatherNamespaceOps;

/**
 * FeatherBindOps defines the operations for host <> interpreter interop.
 */
typedef struct FeatherBindOpts {
  /**
   * unknown is invoked when the interpreter tries to invoke an undefined
   * procedure and gives the host a chance to react to this.
   *
   * If the host returns TCL_ERROR, the interpreter considers the procedure
   * lookup to have failed for good.
   */
  FeatherResult (*unknown)(FeatherInterp interp, FeatherObj cmd, FeatherObj args,
                       FeatherObj *value);
} FeatherBindOpts;

/**
 * FeatherForeignOps provides operations for foreign (host-language) objects.
 *
 * Foreign objects are values that wrap host language types (Go structs,
 * JavaScript objects, Java objects, etc.). They participate in TCL's
 * shimmering mechanism and can be introspected.
 *
 * The host is responsible for:
 * - Creating foreign objects with NewForeign or equivalent
 * - Maintaining a type registry mapping type names to implementations
 * - Implementing method dispatch for each foreign type
 *
 * Foreign objects:
 * - Have a type name (e.g., "Mux", "Connection")
 * - Have a string representation for display/shimmering
 * - Can expose methods callable from TCL
 * - Participate in introspection via info type/methods
 */
typedef struct FeatherForeignOps {
  /**
   * is_foreign checks if an object is a foreign object.
   *
   * Returns 1 if obj is a foreign object, 0 otherwise.
   */
  int (*is_foreign)(FeatherInterp interp, FeatherObj obj);

  /**
   * type_name returns the type name of a foreign object.
   *
   * Returns the type name (e.g., "Mux", "Connection") as a FeatherObj.
   * Returns nil (0) if the object is not a foreign object.
   */
  FeatherObj (*type_name)(FeatherInterp interp, FeatherObj obj);

  /**
   * string_rep returns the string representation of a foreign object.
   *
   * This is called during shimmering when a foreign object needs to be
   * converted to a string. The returned string is cached in the object.
   *
   * Typical formats: "<Mux:1>", "mux1", "<Connection:host:3306>"
   * Returns nil (0) if the object is not a foreign object.
   */
  FeatherObj (*string_rep)(FeatherInterp interp, FeatherObj obj);

  /**
   * methods returns a list of method names available on a foreign object.
   *
   * Returns a list of strings (e.g., {"handle" "listen" "close" "destroy"}).
   * Returns empty list if the object is not foreign or has no methods.
   *
   * Used by "info methods $obj" for introspection.
   */
  FeatherObj (*methods)(FeatherInterp interp, FeatherObj obj);

  /**
   * invoke calls a method on a foreign object.
   *
   * obj: the foreign object
   * method: the method name to invoke
   * args: list of arguments to the method
   *
   * Returns TCL_OK on success with result in interpreter's result slot.
   * Returns TCL_ERROR if method doesn't exist or invocation fails.
   */
  FeatherResult (*invoke)(FeatherInterp interp, FeatherObj obj, FeatherObj method, FeatherObj args);

  /**
   * destroy is called when a foreign object is being destroyed.
   *
   * This gives the host an opportunity to clean up resources associated
   * with the foreign object (close connections, release handles, etc.).
   *
   * Called when the object's command is deleted (e.g., via rename to "").
   */
  void (*destroy)(FeatherInterp interp, FeatherObj obj);
} FeatherForeignOps;

/**
 * FeatherHostOps contains the aggregation of all operations necessary
 * for interpreter to work.
 */
typedef struct FeatherHostOps {
  FeatherFrameOps frame;
  FeatherVarOps var;
  FeatherNamespaceOps ns;
  FeatherStringOps string;
  FeatherRuneOps rune;
  FeatherListOps list;
  FeatherDictOps dict;
  FeatherIntOps integer;
  FeatherDoubleOps dbl;
  FeatherInterpOps interp;
  FeatherBindOpts bind;
  FeatherForeignOps foreign;
} FeatherHostOps;

/**
 * feather_interp_init registers all builtin commands with the interpreter.
 *
 * This should be called once after creating the interpreter and before
 * evaluating any scripts.
 */
void feather_interp_init(const FeatherHostOps *ops, FeatherInterp interp);

/**
 * feather_builtin_set implements the TCL 'set' command.
 *
 * Usage:
 *   set varName ?value?
 *
 * With two arguments, sets varName to value and returns value.
 * With one argument, returns the current value of varName.
 * Errors if varName does not exist (one-argument form).
 */
FeatherResult feather_builtin_set(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args);

/**
 * feather_builtin_expr implements the TCL 'expr' command.
 *
 * Usage:
 *   expr arg ?arg ...?
 *
 * Concatenates arguments with spaces and evaluates as an expression.
 * Supports comparison (<, <=, >, >=, ==, !=), bitwise (&, |),
 * and logical (&&, ||) operators on integers.
 */
FeatherResult feather_builtin_expr(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args);

/**
 * feather_strlen counts bytes in a null-terminated C string, excluding the null.
 *
 * This is equivalent to strlen but avoids stdlib dependency.
 */
size_t feather_strlen(const char *s);

/**
 * feather_glob_match performs glob pattern matching.
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
int feather_glob_match(const char *pattern, size_t pattern_len,
                   const char *string, size_t string_len);

/**
 * feather_list_parse_obj parses a string object as a TCL list.
 *
 * TCL list syntax:
 *   - Elements separated by whitespace (space, tab, newline)
 *   - {braced} elements: content is literal, braces nest
 *   - "quoted" elements: backslash escapes are processed
 *   - bare words: terminated by whitespace
 *
 * Returns a list object containing the parsed elements.
 * On parse error (unmatched brace/quote), returns an incomplete list
 * (parsing stops at the error point).
 */
FeatherObj feather_list_parse_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj s);

#endif
