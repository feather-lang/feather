# lindex Builtin Implementation Comparison

This document compares feather's implementation of `lindex` with TCL's official specification.

## Summary of Our Implementation

Our implementation in `src/builtin_lindex.c` provides basic list indexing functionality:

- Accepts exactly 2 arguments: a list and an integer index
- Returns the element at the specified position (0-indexed)
- Returns an empty string for out-of-bounds indices (negative or >= list length)
- Converts the index argument to an integer, failing with an error if conversion fails

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic indexing with integer | Supported | `lindex {a b c} 1` returns `b` |
| Zero-based indexing | Supported | First element is at index 0 |
| Negative index returns empty | Supported | `lindex {a b c} -1` returns `""` |
| Out-of-bounds returns empty | Supported | `lindex {a b c} 5` returns `""` |

## TCL Features We Do NOT Support

### 1. `end` and `end-N` Index Expressions

TCL supports special index keywords:

```tcl
lindex {a b c} end      ;# returns "c"
lindex {a b c} end-1    ;# returns "b"
lindex {a b c} end-2    ;# returns "a"
```

Our implementation only accepts integer indices and will error on these expressions.

### 2. Index Arithmetic

TCL supports index arithmetic with `+` and `-`:

```tcl
set idx 1
lindex {a b c d e f} $idx+2    ;# returns "d"
lindex {a b c d e f} 2+1       ;# returns "d"
```

Our implementation does not parse or evaluate these expressions.

### 3. Multiple Indices for Nested Lists

TCL allows multiple indices to access nested lists:

```tcl
lindex {{a b c} {d e f} {g h i}} 2 1           ;# returns "h"
lindex {{a b c} {d e f} {g h i}} {2 1}         ;# same result
lindex {{{a b} {c d}} {{e f} {g h}}} 1 1 0     ;# returns "g"
```

Our implementation requires exactly 2 arguments and does not support nested indexing.

### 4. Zero Arguments (Identity Behavior)

TCL allows calling `lindex` with just the list (or with an empty index list):

```tcl
lindex {a b c}      ;# returns "a b c"
lindex {a b c} {}   ;# returns "a b c"
```

Our implementation requires exactly 2 arguments and will error in these cases.

### 5. Index List as Single Argument

TCL allows grouping multiple indices into a list:

```tcl
lindex $nested {1 2 3}    ;# equivalent to: lindex $nested 1 2 3
```

Our implementation does not support this syntax.

## Notes on Implementation Differences

### Error Messages

Our error message for invalid index type differs from TCL:

- **Feather**: `expected integer but got "end"`
- **TCL**: Would likely interpret `end` as a valid index expression

### Argument Count Strictness

Our implementation is stricter than TCL:

- **Feather**: Requires exactly 2 arguments
- **TCL**: Accepts 1 argument (returns list as-is) or 2+ arguments (for nested access)

### Index Expression Parsing

TCL's index handling is shared with `string index` and other commands via a common index parsing routine that understands `end`, `end-N`, and arithmetic. Our implementation does simple integer conversion only, which makes it simpler but less feature-complete.

### Performance Consideration

Our simpler implementation may be faster for the common case (integer index into flat list) since it avoids parsing index expressions. However, users must implement workarounds for features like `end` indexing:

```tcl
# Workaround for end indexing in feather
set len [llength $list]
set lastIdx [expr {$len - 1}]
lindex $list $lastIdx
```
