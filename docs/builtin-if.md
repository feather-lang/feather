# The `if` Builtin Command

This document compares feather's implementation of the `if` command with the official TCL specification.

## Summary of Our Implementation

The feather `if` implementation is located in `src/builtin_if.c`. It provides conditional script execution with the following structure:

```
if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?
```

The implementation:
1. Evaluates conditions using the `expr` builtin
2. Supports boolean values: `true`, `false`, `yes`, `no`, and integer values (0 = false, non-zero = true)
3. Executes the body corresponding to the first true condition
4. Returns an empty string if no condition matches and no `else` clause exists

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic `if expr body` | Supported | Core functionality |
| Optional `then` keyword | Supported | Accepted as noise word between expression and body |
| `elseif` clauses | Supported | Any number of elseif clauses allowed |
| `else` clause | Supported | Final fallback body |
| Boolean literals `true`/`false` | Supported | Case appears to be exact match |
| Boolean literals `yes`/`no` | Supported | Case appears to be exact match |
| Integer boolean evaluation | Supported | 0 = false, non-zero = true |
| Expression evaluation via `expr` | Supported | Conditions are evaluated using the expr builtin |
| Return value from executed body | Supported | Returns result of the executed body script |
| Empty string return when no match | Supported | Returns "" if no condition matches and no else |

## TCL Features We Do NOT Support

Based on comparison with the TCL 9 manual, our implementation appears to be **fully compliant** with the TCL specification. All documented features are implemented:

- Basic conditional execution
- Optional `then` keyword
- Multiple `elseif` clauses
- Optional `else` clause
- Optional `bodyN` (when `else` is omitted)
- Boolean expression evaluation
- Proper return values

**No missing features identified.**

## Notes on Implementation Differences

### Boolean Value Handling

Our implementation checks for boolean literals in a specific order:
1. `true` (literal match)
2. `false` (literal match)
3. `yes` (literal match)
4. `no` (literal match)
5. Integer conversion (0 = false, non-zero = true)

TCL also accepts these boolean values, though standard TCL may be case-insensitive for boolean string matching (accepting `TRUE`, `True`, `YES`, etc.). Our implementation uses exact literal matching via `feather_obj_eq_literal`, which may be case-sensitive.

**Potential difference:** Case sensitivity of boolean string values may differ from standard TCL.

### Expression Evaluation

Our implementation delegates condition evaluation to the `expr` builtin. This is consistent with TCL's documented behavior where expressions are evaluated "in the same way that expr evaluates its argument."

### Error Messages

Our implementation uses the standard TCL error message format:
```
wrong # args: should be "if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?"
```

For invalid boolean values, we report:
```
expected boolean value but got "..."
```

### Script Evaluation

Bodies are evaluated using `feather_script_eval_obj` with `TCL_EVAL_LOCAL` flag, ensuring proper local scope execution.

### Edge Cases

1. **Empty else body**: The TCL manual states "BodyN may also be omitted as long as else is omitted too." Our implementation requires a body after `else`, which matches this specification.

2. **Multi-line expressions**: TCL explicitly mentions support for multi-line expressions. Since we delegate to `expr`, this should work if `expr` supports it.
