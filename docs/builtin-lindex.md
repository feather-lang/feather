# lindex Builtin Implementation Comparison

This document compares feather's implementation of `lindex` with TCL's official specification.

## Summary of Our Implementation

Our implementation in `src/builtin_lindex.c` provides list indexing functionality:

- Accepts exactly 2 arguments: a list and an index expression
- Returns the element at the specified position (0-indexed)
- Returns an empty string for out-of-bounds indices (negative or >= list length)
- Supports `end`, `end-N`, and arithmetic expressions (`M+N`, `M-N`)

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic indexing with integer | Supported | `lindex {a b c} 1` returns `b` |
| Zero-based indexing | Supported | First element is at index 0 |
| Negative index returns empty | Supported | `lindex {a b c} -1` returns `""` |
| Out-of-bounds returns empty | Supported | `lindex {a b c} 5` returns `""` |
| `end` index | Supported | `lindex {a b c} end` returns `c` |
| `end-N` syntax | Supported | `lindex {a b c} end-1` returns `b` |
| Arithmetic expressions | Supported | `lindex {a b c} 0+1` returns `b` |

## TCL Features We Do NOT Support

### 1. Multiple Indices for Nested Lists

TCL allows multiple indices to access nested lists:

```tcl
lindex {{a b c} {d e f} {g h i}} 2 1           ;# returns "h"
lindex {{a b c} {d e f} {g h i}} {2 1}         ;# same result
lindex {{{a b} {c d}} {{e f} {g h}}} 1 1 0     ;# returns "g"
```

Our implementation requires exactly 2 arguments and does not support nested indexing.

### 2. Zero Arguments (Identity Behavior)

TCL allows calling `lindex` with just the list (or with an empty index list):

```tcl
lindex {a b c}      ;# returns "a b c"
lindex {a b c} {}   ;# returns "a b c"
```

Our implementation requires exactly 2 arguments and will error in these cases.

### 3. Index List as Single Argument

TCL allows grouping multiple indices into a list:

```tcl
lindex $nested {1 2 3}    ;# equivalent to: lindex $nested 1 2 3
```

Our implementation does not support this syntax.

## Notes on Implementation

### Index Parsing

Our implementation uses `feather_parse_index()` which properly handles:
- Simple integers: `0`, `1`, `2`, `-1`
- `end` keyword: resolves to `length - 1`
- `end-N` syntax: `end-1`, `end-2`, etc.
- Arithmetic: `M+N`, `M-N`, e.g., `0+1`, `5-2`
- Combined forms: `end+1`, `end-5+2`

### Argument Count Strictness

Our implementation is stricter than TCL:

- **Feather**: Requires exactly 2 arguments
- **TCL**: Accepts 1 argument (returns list as-is) or 2+ arguments (for nested access)
