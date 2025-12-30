# throw Builtin Command

## Summary of Our Implementation

Our implementation of `throw` in `src/builtin_throw.c` provides the core functionality of the TCL `throw` command:

- Accepts exactly two arguments: `type` and `message`
- Validates that `type` is a non-empty list
- Sets up return options with `-code 1` and `-errorcode` containing the type
- Sets the error message as the interpreter result
- Initializes error trace state for stack trace support
- Returns `TCL_ERROR` to unwind the stack

## TCL Features We Support

1. **Basic syntax**: `throw type message` - both arguments are required
2. **Type validation**: The `type` argument must be a non-empty list
3. **Error code propagation**: The `type` list is stored in `-errorcode` return option
4. **Error code format**: Stored as `-code 1` (equivalent to `TCL_ERROR`)
5. **Machine-readable error codes**: The type list can contain multiple words describing the error
6. **Human-readable message**: The message is set as the interpreter result

## TCL Features We Do NOT Support

Our implementation appears to be **feature-complete** with respect to the TCL `throw` command specification. The TCL manual describes `throw` as a simple command with exactly two arguments:

- `type` - a list of words for machine-readable error classification
- `message` - human-readable text for display

Both of these are fully implemented. The behavior of stack unwinding, error trapping by `catch`/`try`, and `bgerror` handling are features of the interpreter's error handling system rather than the `throw` command itself.

## Notes on Implementation Differences

1. **Error message wording**: Our "wrong # args" error says `"throw type message"` which matches the TCL convention for argument errors.

2. **Type validation error**: When the type list is empty, we return `"type must be non-empty"`. TCL's exact error message may differ slightly, but the behavior (rejecting empty type lists) is correct per the specification.

3. **Return options format**: We use a list-based dictionary representation for return options (`-code 1 -errorcode {type list}`). This matches TCL's return options dictionary structure.

4. **Error trace integration**: Our implementation integrates with `error_trace.h` for stack trace support, which is a feather-specific enhancement that aligns with TCL 8.6+'s `-errorinfo` functionality.

5. **Convention adherence**: The TCL manual notes that "by convention, the words in the type argument should go from most general to most specific" (e.g., `{ARITH DIVZERO {divide by zero}}`). Our implementation enforces that the type is non-empty but does not validate the ordering convention - this is consistent with TCL itself, which treats this as a convention rather than a requirement.
