# `global` Builtin Command

Comparison of our implementation with official TCL behavior.

## Summary of Our Implementation

Our `global` command is implemented in `src/builtin_global.c`. It creates links from local variables in a procedure to variables in a namespace (typically the global namespace).

Key behaviors:
- No-op when called with no arguments
- No-op at level 0 (global scope, outside any procedure)
- Rejects array element syntax (e.g., `a(b)`)
- Supports namespace-qualified variable names (e.g., `::foo` or `ns::var`)
- For qualified names, the local variable name is the tail (unqualified part)
- Uses `ops->var.link_ns()` to create the variable link

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Multiple varnames in single call | Supported | Loops through all arguments |
| No-op outside proc context | Supported | Checks `frame.level(interp)` |
| Namespace-qualified names | Supported | Uses `feather_obj_resolve_variable()` |
| Local name is tail of qualified name | Supported | Per `namespace tail` semantics |
| Error on array element syntax | Supported | Checks for `(` character |
| Empty argument list | Supported | Returns empty string, no error |

## TCL Features We Do NOT Support

Based on the TCL manual, our implementation appears to be **fully compliant** with the documented behavior. There are no missing features.

The TCL manual documents these behaviors, all of which we implement:

1. Command has no effect unless executed in proc body context - **Implemented**
2. Creates local variables linked to global variables - **Implemented**
3. Namespace qualifiers use unqualified name as local name - **Implemented**
4. Varname is treated as variable name, not array element - **Implemented**
5. Error on array element syntax like `a(b)` - **Implemented**

## Notes on Implementation Differences

### `info locals` Behavior

The TCL manual states that variables created by `global` (like those created by `upvar`) are "not included in the list returned by `info locals`".

Our implementation creates the link via `ops->var.link_ns()`, but whether linked variables are excluded from `info locals` depends on the `info locals` implementation, not the `global` command itself. This is now documented in the usage help for `global`.

### Error Message Format

Our error message for array element syntax is:
```
can't use "varname" as variable name: must be a scalar variable
```

This matches the standard TCL error message format.

### Namespace Resolution

Our implementation uses `feather_obj_resolve_variable()` to handle namespace-qualified names. This should correctly handle:
- Absolute names (e.g., `::foo::bar`)
- Relative names with qualifiers (e.g., `foo::bar`)
- Simple unqualified names (e.g., `bar`)

The behavior should match TCL's `namespace tail` semantics for determining the local variable name.
