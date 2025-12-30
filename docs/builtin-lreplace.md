# lreplace Builtin Implementation

This document compares feather's `lreplace` implementation with the official TCL `lreplace` command.

## Summary of Our Implementation

The feather `lreplace` implementation (`src/builtin_lreplace.c`) provides the core functionality of replacing elements in a list with zero or more new elements.

Key implementation details:
- Requires at least 3 arguments: `lreplace list first last ?element ...?`
- Uses `feather_parse_index` for index parsing
- Uses `ops->list.splice` for efficient O(n) replacement
- Supports both deletion (no replacement elements) and replacement with any number of elements

## TCL Features We Support

### Basic Replacement
- Replace one or more elements with zero or more new elements
- Example: `lreplace {a b c d e} 1 1 foo` returns `a foo c d e`

### Element Deletion
- When no replacement elements are provided, elements are deleted
- Example: `lreplace {a b c d e} 1 2` returns `a d e`

### Index Expressions
Our `feather_parse_index` function supports:
- **Integer indices**: `0`, `1`, `2`, etc.
- **Negative indices**: `-1`, `-2`, etc.
- **The `end` keyword**: Refers to the last element of the list
- **Index arithmetic**: `end-1`, `end+2`, `5+3`, `10-2`, etc.

### Prepending Elements
- When `first` is less than zero, it allows prepending elements
- Our implementation clamps `first < 0` to `0`

### Appending Elements
- When `first` or `last` is greater than the list length, elements can be appended
- Our implementation clamps indices appropriately

### Insertion (No Deletion)
- When `last < first`, elements are inserted without deletion
- Our implementation handles `last < first - 1` by setting `last = first - 1`, resulting in `deleteCount = 0`

## TCL Features We Do NOT Support

After careful comparison with the TCL 9 manual, our implementation appears to support all documented features of `lreplace`. The TCL manual describes:

1. Index values interpreted like `string index` - **Supported** via `feather_parse_index`
2. Indices less than zero refer to before the first element - **Supported** via clamping
3. Indices greater than list length treated as one past the end - **Supported** via clamping
4. `last < first` means insertion with no deletion - **Supported**
5. Zero or more replacement elements - **Supported**

**No unsupported features identified.**

## Notes on Implementation Differences

### Index Clamping Behavior
Our implementation explicitly clamps indices:
- `first < 0` becomes `0`
- `first > listLen` becomes `listLen`
- `last < first - 1` becomes `first - 1`
- `last >= listLen` becomes `listLen - 1`

This matches TCL's documented behavior but is done explicitly in our C code rather than relying on underlying list operations.

### Error Message Format
Our error message for wrong argument count:
```
wrong # args: should be "lreplace list first last ?element ...?"
```
This matches the standard TCL error message format.

### Index Parsing Error Message
Our index parsing error message:
```
bad index "...": must be integer?[+-]integer? or end?[+-]integer?
```
This follows TCL's error message style for invalid indices.

### Internal Implementation
Our implementation uses a single `splice` operation for efficiency, which performs the replacement in O(n) time where n is the list length. This is an implementation detail that does not affect the external behavior.
