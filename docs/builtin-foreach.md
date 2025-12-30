# foreach Builtin Comparison

This document compares Feather's `foreach` implementation with the official TCL manual.

## Summary of Our Implementation

Feather's `foreach` implementation is located in `src/builtin_foreach.c`. It provides a loop construct that iterates over elements of one or more lists, assigning values to loop variables for each iteration.

**Signature:**
```tcl
foreach varList list ?varList list ...? command
```

**Core behavior:**
- Supports single variable iteration: `foreach x {a b c} { ... }`
- Supports multiple variables per list: `foreach {i j} {a b c d} { ... }`
- Supports parallel iteration over multiple lists: `foreach i {a b c} j {d e f} { ... }`
- Supports combined form: `foreach i {a b c} {j k} {d e f g} { ... }`
- Validates that varlists are non-empty
- Uses empty string for missing elements when list is exhausted
- Returns empty string on normal completion
- Supports `break` and `continue` statements

## TCL Features We Support

All core `foreach` functionality is implemented:

| Feature | Status | Notes |
|---------|--------|-------|
| Single variable iteration | Supported | `foreach x $list { ... }` |
| Multiple variables per list | Supported | `foreach {i j} $list { ... }` |
| Parallel list iteration | Supported | `foreach i $list1 j $list2 { ... }` |
| Combined form | Supported | `foreach i $list1 {j k} $list2 { ... }` |
| Empty values for missing elements | Supported | When list has fewer elements than variables |
| `break` statement | Supported | Exits loop normally |
| `continue` statement | Supported | Skips to next iteration |
| Error propagation | Supported | Errors in body are propagated |
| Empty string return | Supported | Normal completion returns `""` |
| Variable list validation | Supported | Empty varlist is rejected with error |

## TCL Features We Do NOT Support

Based on comparison with the TCL manual, **all documented features are implemented**. There are no missing features from the TCL specification.

## Notes on Implementation Differences

### Argument Validation

Our implementation validates:
1. Minimum of 3 arguments required
2. Argument count must be odd (pairs of varlist/list + body)
3. Each varlist must be non-empty

The error message format matches TCL convention:
```
wrong # args: should be "foreach varList list ?varList list ...? command"
```

### Element Extraction

The TCL manual states that elements are assigned "as if the lindex command had been used to extract the element." Our implementation uses `ops->list.at()` which provides equivalent behavior.

### Iteration Count Calculation

Our implementation correctly calculates the maximum number of iterations as:
```c
size_t iters = (listLen + numVars - 1) / numVars;  // ceiling division
```

This ensures all values from all lists are used exactly once, matching TCL semantics.

### Return Codes

Our implementation handles return codes consistently with TCL:
- `TCL_OK` - Continue iteration
- `TCL_BREAK` - Exit loop, return normally with empty string
- `TCL_CONTINUE` - Skip to next iteration
- `TCL_ERROR` (and other errors) - Propagate immediately

### Scope

The body is evaluated with `TCL_EVAL_LOCAL` flag, meaning variable assignments in the body affect the local scope, which matches TCL's behavior.
