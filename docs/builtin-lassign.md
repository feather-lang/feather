# lassign Builtin Comparison

This document compares the feather implementation of `lassign` with the official TCL 8.5+ specification.

## Summary of Our Implementation

The feather `lassign` command is implemented in `src/builtin_lassign.c`. It provides the core functionality of assigning list elements to variables:

1. Parses the first argument as a list
2. Assigns successive list elements to the provided variable names
3. Variables without corresponding list elements are set to empty string
4. Returns a list of unassigned elements (or empty string if none remain)

## TCL Features We Support

Our implementation supports **all** features documented in the TCL manual:

| Feature | Supported | Notes |
|---------|-----------|-------|
| Basic list element assignment | Yes | Elements assigned to variables in order |
| Empty string for excess variables | Yes | When more variables than list elements |
| Return unassigned elements | Yes | Returns list of remaining elements |
| Zero variable names | Yes | `lassign list` with no varNames is allowed |
| Error on missing list argument | Yes | Reports "wrong # args" error |

### Behavior Examples (All Supported)

```tcl
lassign {a b c} x y z       ;# Returns "", sets x=a, y=b, z=c
lassign {d e} x y z         ;# Returns "", sets x=d, y=e, z=""
lassign {f g h i} x y       ;# Returns "h i", sets x=f, y=g
```

## TCL Features We Do NOT Support

**None identified.** Our implementation appears to be feature-complete with respect to the TCL 8.5 manual specification for `lassign`.

## Notes on Implementation Differences

### Error Message Format

Our error message matches TCL convention:
```
wrong # args: should be "lassign list ?varName ...?"
```

### List Parsing

The implementation uses `ops->list.from()` to parse the first argument as a list. If parsing fails, the command returns `TCL_ERROR`. This matches TCL behavior where malformed lists cause errors.

### Empty Variable Name Handling

The implementation does not explicitly validate variable names. It passes them directly to `ops->var.set()`. The behavior for empty or invalid variable names depends on the host implementation.

### Memory Management

The implementation creates a new list for remaining elements using `ops->list.create()` and `ops->list.push()`. This is an internal detail that matches the expected TCL semantics.

## Reference

- **TCL Manual Version:** 8.5
- **Our Implementation:** `src/builtin_lassign.c`
- **Synopsis:** `lassign list ?varName ...?`
