# continue Builtin Documentation

## Summary of Our Implementation

Our implementation of `continue` is located in `src/builtin_continue.c`. It:

1. Validates that no arguments are passed (argc == 0)
2. Returns an error with message "wrong # args: should be \"continue\"" if arguments are provided
3. Sets the result to an empty string
4. Returns `TCL_CONTINUE` (return code 4) to signal a continue exception

The implementation is minimal and straightforward at 16 lines of C code.

## TCL Features We Support

- **Basic continue behavior**: Returns TCL_CONTINUE (code 4) which signals a continue exception
- **No-argument syntax**: `continue` takes no arguments, matching TCL specification
- **Argument validation**: Correctly rejects any arguments with appropriate error message
- **Empty result**: Sets result to empty string on success

## TCL Features We Do NOT Support

Our implementation appears to be **complete** with respect to the TCL specification.

The TCL manual describes `continue` as a simple command that:
- Takes no arguments
- Returns a TCL_CONTINUE (4) result code
- Causes the current script to abort to the innermost containing loop

All of these features are implemented in our version.

## Notes on Implementation Differences

1. **Exception handling context**: The TCL manual mentions that continue exceptions are handled in "a few other situations, such as the catch command and the outermost scripts of procedure bodies." Whether our interpreter correctly handles continue exceptions in all these contexts depends on the implementation of those other commands (for, foreach, while, catch, proc), not on the `continue` builtin itself.

2. **Result value**: Our implementation explicitly sets an empty string as the result before returning TCL_CONTINUE. This is consistent with standard TCL behavior where the result is typically ignored when a continue exception is raised.

3. **Error message format**: Our error message "wrong # args: should be \"continue\"" follows the standard TCL error message convention for arity errors.

The `continue` command is intentionally simple - it only signals the exception. The actual behavior of skipping to the next loop iteration is implemented by the loop commands (for, foreach, while) that catch and handle the TCL_CONTINUE return code.
