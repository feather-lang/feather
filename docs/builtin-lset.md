# lset Builtin Comparison

This document compares our implementation of the `lset` command with the official TCL 8.4+ specification.

## Summary of Our Implementation

Our `lset` implementation (`src/builtin_lset.c`) provides basic list element modification with a single index. It:

1. Requires at least 3 arguments: `lset varName index value`
2. Reads the variable value and converts it to a list
3. Parses the index using `feather_parse_index()` which supports:
   - Simple integers (e.g., `0`, `1`, `2`)
   - The `end` keyword
   - Index arithmetic with `+` and `-` (e.g., `end-1`, `0+1`)
4. Performs bounds checking (index must be >= 0 and < list length)
5. Replaces the element at the specified index using `ops->list.set_at()`
6. Stores the modified list back in the variable
7. Returns the modified list as the result

## TCL Features We Support

- **Basic element replacement**: `lset varName index newValue`
- **Index keywords**: `end` for the last element
- **Index arithmetic**: `end-1`, `end+0`, `0+1`, etc.
- **Variable modification**: Modifies the variable in place and returns the new list
- **Error handling**:
  - Error if variable does not exist
  - Error if index is out of range (negative or >= list length)
  - Error if index format is invalid

## TCL Features We Do NOT Support

### 1. Zero-Index Form (Replace Entire List)

TCL supports replacing the entire variable value when no index is provided:

```tcl
lset varName newValue      ;# Replaces entire list
lset varName {} newValue   ;# Same as above (empty index list)
```

Our implementation requires at least one index and does not support this form.

### 2. Multiple/Nested Indices

TCL supports modifying elements in nested sublists using multiple indices:

```tcl
lset x 1 2 newValue        ;# Modify element 2 of sublist 1
lset x {1 2} newValue      ;# Same as above (indices as list)
lset x 1 1 0 newValue      ;# Modify deeply nested element
lset x {1 1 0} newValue    ;# Same as above
```

Our implementation only supports a single index and cannot modify nested list elements.

### 3. Appending via Index

TCL allows appending to a list by using an index equal to the list length:

```tcl
set x {a b c}
lset x 3 d                 ;# x becomes {a b c d}
lset x {2 3} j             ;# Append j to sublist at index 2
```

Our implementation treats `index == listLen` as an out-of-bounds error.

### 4. Index as List Argument

TCL allows indices to be passed as a single list argument:

```tcl
lset x {1 2} newValue      ;# Indices 1 and 2 as a list
```

Our implementation does not parse the index argument as a list of indices.

## Notes on Implementation Differences

### Error Messages

- Our error message for invalid index format is: `bad index "...": must be integer?[+-]integer? or end?[+-]integer?`
- TCL's error handling follows the `string index` command conventions

### Bounds Checking

- Our implementation is stricter: we reject `index == listLen` which TCL uses for appending
- Both reject negative indices and indices greater than list length

### Index Parsing

Our index parsing (`src/index_parse.c`) correctly implements:
- Integer parsing with optional sign
- The `end` keyword
- Arithmetic operations (`+` and `-`) for offset calculations

This matches the TCL specification for simple index expressions as described in the `string index` command.

### Performance

Our implementation uses `ops->list.set_at()` for O(1) in-place modification when possible, which is an optimization over reconstructing the entire list.
