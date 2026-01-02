# lindex Builtin Implementation

This document describes feather's implementation of `lindex`.

## Summary

Our `lindex` implementation in `src/builtin_lindex.c` provides full TCL-compatible list indexing:

- Accepts 1 or more arguments
- Returns the element at the specified position(s) (0-indexed)
- Returns an empty string for out-of-bounds indices
- Supports `end`, `end-N`, and arithmetic expressions (`M+N`, `M-N`)
- Supports nested indexing with multiple indices
- Supports index lists as a single argument

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
| Identity (no index) | Supported | `lindex {a b c}` returns `a b c` |
| Empty index list | Supported | `lindex {a b c} {}` returns `a b c` |
| Multiple indices | Supported | `lindex {{a b} {c d}} 1 0` returns `c` |
| Index list | Supported | `lindex {{a b} {c d}} {1 0}` returns `c` |
| Deep nesting | Supported | Up to 64 levels of nesting |

## Usage Examples

```tcl
# Basic indexing
lindex {a b c} 1           ;# returns "b"

# Identity (no index)
lindex {a b c}             ;# returns "a b c"

# End-relative indexing
lindex {a b c d e} end     ;# returns "e"
lindex {a b c d e} end-2   ;# returns "c"

# Nested list indexing
lindex {{a b} {c d} {e f}} 1 0     ;# returns "c"
lindex {{a b} {c d} {e f}} {1 0}   ;# same result

# Deep nesting
lindex {{{a b} {c d}} {{e f} {g h}}} 1 1 0   ;# returns "g"

# End in nested indexing
lindex {{a b c} {d e f}} end end   ;# returns "f"
```

## Implementation Notes

1. **Index Resolution**: Each index in the sequence is resolved using `feather_parse_index()`, which handles `end`, `end-N`, and arithmetic at each nesting level.

2. **Empty Results**: If any index is out of bounds at any level, an empty string is returned immediately.

3. **Maximum Depth**: The implementation supports up to 64 levels of nesting.

4. **Single vs List Index**: When given exactly 2 arguments, the second argument is checked:
   - If it parses as a list with multiple elements, each element is treated as an index
   - If it's empty, the list is returned as-is
   - Otherwise, it's treated as a single index

## All Major Features Implemented

This implementation is now feature-complete for the common use cases of `lindex`.
