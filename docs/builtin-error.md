# The `error` Builtin Command

## Summary of Our Implementation

Our implementation of the `error` command is located in `src/builtin_error.c`. It follows the standard TCL signature:

```
error message ?info? ?code?
```

The implementation:
1. Validates argument count (1-3 arguments)
2. Extracts the error message from the first argument
3. Builds a return options dictionary containing `-code 1` (TCL_ERROR)
4. Optionally adds `-errorinfo` if the second argument is provided
5. Optionally adds `-errorcode` if the third argument is provided
6. Stores return options via `ops->interp.set_return_options()`
7. Sets the error message as the interpreter result
8. Initializes error trace state if no explicit `-errorinfo` was provided and no error trace is already active
9. Returns `TCL_ERROR`

## TCL Features We Support

- **Basic error generation**: Returns `TCL_ERROR` with a message
- **The `message` argument**: Sets the error message as the interpreter result
- **The optional `info` argument**: When provided, sets `-errorinfo` in return options, which initializes the stack trace
- **The optional `code` argument**: When provided, sets `-errorcode` in return options for machine-readable error codes
- **Stack trace initialization**: When no explicit `info` is provided, our implementation initializes automatic error tracing via `feather_error_init()`

## TCL Features We Do NOT Support

Based on the TCL manual, our implementation appears to be **feature-complete** for the `error` command itself. All three arguments (`message`, `info`, `code`) are supported.

However, the following related functionality may have limitations:

1. **Stack trace accumulation during unwinding**: The TCL manual describes how the interpreter automatically adds information to `-errorinfo` as nested commands unwind. Our implementation initializes error tracing via `feather_error_init()`, but the completeness of the stack trace accumulation during command unwinding depends on how well this is integrated throughout the interpreter.

2. **Integration with `catch`**: The manual shows a pattern where caught errors can be re-raised with preserved stack traces. This depends on whether our `catch` implementation properly preserves and exposes the `$::errorInfo` variable and return options.

3. **Integration with `return -options`**: The modern TCL pattern `return -options $options $errMsg` requires full support in the `return` command.

## Notes on Implementation Differences

1. **Return options format**: Our implementation builds return options as a list of key-value pairs (e.g., `{-code 1 -errorinfo ... -errorcode ...}`). This is the expected dictionary-like format.

2. **Error code value**: We use integer `1` for the `-code` value, which corresponds to `TCL_ERROR`. TCL also accepts symbolic values, but our implementation uses the numeric form.

3. **Automatic trace initialization**: Our implementation has special handling where it only initializes the error trace if:
   - No explicit `info` argument was provided (argc < 2)
   - No error trace is already active (`!feather_error_is_active()`)

   This matches TCL behavior where providing an explicit `info` argument overrides automatic stack trace generation.

4. **String interning**: We use `ops->string.intern()` for the option keys (`-code`, `-errorinfo`, `-errorcode`) which may provide memory efficiency for frequently-used strings.
