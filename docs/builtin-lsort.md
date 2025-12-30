# lsort Builtin Implementation Comparison

This document compares our feather implementation of `lsort` against the official TCL 8.5+ specification.

## Summary of Our Implementation

Our `lsort` implementation in `src/builtin_lsort.c` provides basic list sorting functionality with the following characteristics:

- Uses the host's O(n log n) sort algorithm via `ops->list.sort()`
- Supports three sort modes: ASCII (default), integer, and real
- Implements case-insensitive comparison via `-nocase`
- Supports ascending/descending order
- Implements duplicate removal via `-unique`

## TCL Features We Support

| Option | Description | Status |
|--------|-------------|--------|
| `-ascii` | String comparison using code-point collation order | Supported |
| `-integer` | Convert elements to integers and compare numerically | Supported |
| `-real` | Convert elements to floating-point and compare numerically | Supported |
| `-increasing` | Sort in ascending order (default) | Supported |
| `-decreasing` | Sort in descending order | Supported |
| `-nocase` | Case-insensitive string comparison | Supported |
| `-unique` | Remove duplicate elements after sorting | Supported |

## TCL Features We Do NOT Support

| Option | Description | TCL Behavior |
|--------|-------------|--------------|
| `-dictionary` | Dictionary-style comparison: case ignored except as tie-breaker, embedded numbers compared as integers (e.g., `x10y` sorts between `x9y` and `x11y`) | Not implemented |
| `-command <command>` | Use a custom TCL script as the comparison function | Not implemented |
| `-index <indexList>` | Sort sublists by extracting and comparing the element at the given index path | Not implemented |
| `-indices` | Return a list of indices in sorted order instead of the values themselves | Not implemented |
| `-stride <strideLength>` | Treat the list as groups of N elements and sort the groups | Not implemented |

### Feature Details

#### -dictionary
Dictionary sorting provides "natural" ordering where:
- Case is ignored except as a tie-breaker (`bigBoy` sorts between `bigbang` and `bigboy`)
- Embedded numbers are compared numerically (`a10` comes after `a2`, not before)

#### -command
Allows arbitrary TCL scripts to define the comparison:
```tcl
lsort -command {myCompare} $list
```
The command receives two elements and must return -1, 0, or 1.

#### -index
Enables sorting nested lists by a specific element:
```tcl
lsort -integer -index 1 {{First 24} {Second 18} {Third 30}}
# Returns: {Second 18} {First 24} {Third 30}
```
Supports nested indexing with a list of indices.

#### -indices
Returns positions rather than values:
```tcl
lsort -indices {c a b}
# Returns: 1 2 0 (indices of sorted elements in original list)
```

#### -stride
Groups elements for sorting:
```tcl
lsort -stride 2 {carrot 10 apple 50 banana 25}
# Returns: apple 50 banana 25 carrot 10
```

## Notes on Implementation Differences

1. **Option abbreviations**: TCL accepts unique abbreviations of option names (e.g., `-dec` for `-decreasing`). Our implementation requires full option names.

2. **Error messages**: Our error message for unknown options lists only the options we support:
   ```
   bad option "...": must be -ascii, -decreasing, -increasing, -integer, -nocase, -real, or -unique
   ```

3. **-nocase interaction**: In TCL, `-nocase` has no effect when combined with `-dictionary`, `-integer`, or `-real`. Our implementation does not document or test this interaction since we don't support `-dictionary`.

4. **-unique behavior**: TCL specifies that only the "last set of duplicate elements" is retained. Our implementation removes consecutive duplicates after sorting, which achieves the same result for most cases but may differ in edge cases involving the `-command` option (which we don't support).

5. **Stability**: TCL explicitly uses merge-sort for stable sorting. Our implementation delegates to the host's sort function, so stability depends on the host implementation.
