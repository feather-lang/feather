# while - Builtin Command Comparison

This document compares our implementation of the `while` command against the official TCL 9 manual.

## Summary of Our Implementation

Our `while` implementation is located in `src/builtin_while.c`. It provides a standard while loop that:

1. Takes exactly two arguments: a test condition and a body script
2. Evaluates the test condition using the `expr` builtin
3. If the condition is true, executes the body script
4. Repeats until the condition becomes false
5. Returns an empty string on normal completion

The condition result is interpreted as:
- Boolean literals (true/false)
- Integer values (0 = false, non-zero = true)

Loop control is handled via:
- `TCL_BREAK` - exits the loop immediately
- `TCL_CONTINUE` - skips to the next iteration
- `TCL_ERROR` - propagates errors up the call stack

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic while loop syntax | Supported | `while test body` |
| Expression evaluation via `expr` | Supported | Condition is evaluated as an expression |
| Boolean literal results | Supported | true/false recognized |
| Integer boolean results | Supported | 0 = false, non-zero = true |
| `break` command | Supported | Exits loop immediately |
| `continue` command | Supported | Skips to next iteration |
| Returns empty string | Supported | On normal completion |
| Error propagation | Supported | Errors in body are propagated |
| Argument count validation | Supported | Exactly 2 arguments required |

## TCL Features We Do NOT Support

Based on comparison with the TCL 9 manual, our implementation appears to be **fully compliant** with the documented behavior. The TCL manual describes:

1. **Syntax**: `while test body` - We support this.
2. **Expression evaluation**: Test is evaluated like `expr` - We use `expr` builtin.
3. **Boolean requirement**: Value must be a proper boolean - We validate this.
4. **Body execution**: Passed to interpreter - We use `feather_script_eval_obj`.
5. **Loop control**: `continue` and `break` - We handle both return codes.
6. **Return value**: Empty string - We return `""`.

No missing features were identified.

## Notes on Implementation Differences

### 1. Error Message Format

Our error message for invalid boolean values:
```
expected boolean value but got "..."
```

This follows standard TCL error messaging conventions.

### 2. Expression Evaluation

We delegate condition evaluation entirely to the `expr` builtin, which ensures consistency with how TCL evaluates expressions. The condition is wrapped in a list and passed to `feather_builtin_expr`.

### 3. Local Evaluation

The body script is evaluated with `TCL_EVAL_LOCAL` flag, which maintains proper scope semantics.

### 4. Boolean Conversion

We use a helper function `feather_obj_to_bool_literal` for boolean literal detection, followed by integer conversion fallback. This matches TCL's boolean evaluation rules where:
- `true`, `yes`, `on`, `1` are true
- `false`, `no`, `off`, `0` are false
- Non-zero integers are true

### 5. Memory Management

Our implementation uses the `FeatherHostOps` interface for all memory operations, ensuring proper integration with the host environment's memory management strategy.

## See Also

Related TCL commands:
- `break` - Exit loop immediately
- `continue` - Skip to next iteration
- `for` - C-style for loop
- `foreach` - Iterate over list elements
