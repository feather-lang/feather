# lrange Builtin Command

## Summary of Our Implementation

Our `lrange` implementation is located in `/Users/dhamidi/projects/feather/src/builtin_lrange.c`. It extracts a range of elements from a list, returning a new list containing elements from index `first` through `last` (inclusive).

The implementation:
1. Validates that exactly 3 arguments are provided (list, first, last)
2. Converts the first argument to a list
3. Parses both index arguments using `feather_parse_index`
4. Clamps indices to valid bounds (first to 0 if negative, last to end if beyond list length)
5. Returns an empty string if the range is invalid (first > last) or the list is empty
6. Uses `ops->list.slice` for efficient O(n) extraction where n is the slice size

## TCL Features We Support

### Basic Syntax
- `lrange list first last` - Returns elements from first through last (inclusive)

### Index Formats
Our index parser (`src/index_parse.c`) supports:
- **Integer indices**: `0`, `1`, `2`, etc.
- **Negative integers**: `-1`, `-2`, etc.
- **The `end` keyword**: References the last element
- **Simple index arithmetic**: `end-1`, `end-2`, `0+1`, `5-2`, etc.
- **Signed numbers in arithmetic**: `end+-1`, `end--2`

### Index Clamping Behavior
- If `first` is less than zero, it is treated as zero
- If `last` is greater than or equal to the list length, it is treated as `end`
- If `first` is greater than `last`, an empty string is returned

### Examples That Work
```tcl
lrange {a b c d e} 0 1      ;# Returns: a b
lrange {a b c d e} end-2 end ;# Returns: c d e
lrange {a b c d e} 1 end-1   ;# Returns: b c d
lrange {a b c d e} 1 1       ;# Returns: {b} (as a list)
```

## TCL Features We Do NOT Support

Based on the TCL manual, our implementation appears to be **fully compatible** with the documented behavior. All features mentioned in the TCL 7.4+ manual are supported:

1. Basic list range extraction
2. The `end` keyword for indices
3. Index arithmetic with `+` and `-` operators
4. Proper clamping of out-of-bounds indices
5. Empty result for invalid ranges (first > last)

There are no documented TCL features for `lrange` that we do not support.

## Notes on Implementation Differences

### String vs List Result for Empty Range
Our implementation returns an empty string (`""`) for empty/invalid ranges, which is consistent with TCL behavior (an empty list has the string representation of an empty string).

### Index Parsing Error Messages
Our error message format for bad indices is:
```
bad index "X": must be integer?[+-]integer? or end?[+-]integer?
```
This may differ slightly from TCL's exact wording but conveys the same information.

### Brace Preservation
The TCL manual notes that `lrange list first first` produces different results than `lindex list first` because `lrange` returns a proper list. For example:
```tcl
set var {some {elements to} select}
lindex $var 1     ;# Returns: elements to
lrange $var 1 1   ;# Returns: {elements to}
```
Our implementation should preserve this behavior through the list slicing mechanism, returning elements in their proper list representation.

### Index Arithmetic Limitations
While we support basic index arithmetic (`end-1`, `5+3`), we do not support:
- Whitespace in index expressions (e.g., `end - 1`)
- Multiplication or division in index expressions

However, the TCL manual only documents simple `[+-]integer` arithmetic for indices, which we fully support.
