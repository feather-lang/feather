# lset Builtin Comparison

This document compares our implementation of the `lset` command with the official TCL 8.4+ specification.

## Summary of Our Implementation

Our `lset` implementation (`src/builtin_lset.c`) provides full list element modification with nested index support. It:

1. Requires at least 2 arguments: `lset varName newValue` or `lset varName ?index ...? value`
2. Reads the variable value and converts it to a list
3. Parses indices using `feather_parse_index()` which supports:
   - Simple integers (e.g., `0`, `1`, `2`)
   - The `end` keyword
   - Index arithmetic with `+` and `-` (e.g., `end-1`, `0+1`)
4. Performs bounds checking (index must be >= 0 and <= list length)
5. Supports multiple/nested indices for modifying nested sublists
6. Allows appending via index equal to list length
7. Replaces the element at the specified index using `ops->list.set_at()`
8. Stores the modified list back in the variable
9. Returns the modified list as the result

## TCL Features We Support

- **Basic element replacement**: `lset varName index newValue`
- **Zero-index form**: `lset varName newValue` (replaces entire variable)
- **Empty index list**: `lset varName {} newValue` (replaces entire variable)
- **Index keywords**: `end` for the last element
- **Index arithmetic**: `end-1`, `end+0`, `0+1`, etc.
- **Multiple indices**: `lset x 1 2 newValue` (modify nested element)
- **Indices as list**: `lset x {1 2} newValue` (same as multiple indices)
- **Appending**: `lset x 3 d` where index equals list length appends element
- **Nested appending**: Works at any nesting level
- **Variable modification**: Modifies the variable in place and returns the new list
- **Error handling**:
  - Error if variable does not exist
  - Error if index is out of range (negative or > list length)
  - Error if index format is invalid

## TCL Features We Do NOT Support

All major TCL `lset` features are now implemented.

## Notes on Implementation

### Error Messages

- Out of range error message: `index "X" out of range` (matches TCL format)
- Invalid index format: `bad index "...": must be integer?[+-]integer? or end?[+-]integer?`
- Wrong arguments: `wrong # args: should be "lset listVar ?index? ?index ...? value"`

### Nested List Modification

When modifying nested lists, the implementation:
1. Parses all indices (either as separate arguments or as a list)
2. Recursively descends into sublists
3. At each level, converts the element to a list (scalars become single-element lists)
4. Performs the modification at the deepest level
5. Reconstructs parent lists with the modified sublists

### Scalar to List Conversion

When accessing a nested index on a scalar element, TCL treats the scalar as a single-element list:
```tcl
set x {a y z}
lset x 1 0 W   ;# y becomes W (replacing element 0 of single-element list {y})
lset x 1 1 W   ;# y becomes {y W} (appending element 1)
```

### Performance

Our implementation uses `ops->list.set_at()` for O(1) in-place modification when possible, which is an optimization over reconstructing the entire list.
