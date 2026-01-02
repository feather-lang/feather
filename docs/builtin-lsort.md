# lsort Builtin Implementation Comparison

This document compares our feather implementation of `lsort` against the official TCL 8.5+ specification.

## Summary of Our Implementation

Our `lsort` implementation in `src/builtin_lsort.c` provides list sorting functionality with the following characteristics:

- Uses the host's O(n log n) sort algorithm via `ops->list.sort()`
- Supports four sort modes: ASCII (default), dictionary, integer, and real
- Implements case-insensitive comparison via `-nocase`
- Supports ascending/descending order
- Implements duplicate removal via `-unique`
- Supports sorting nested lists by a specific element via `-index`
- Can return indices instead of values via `-indices`
- Supports custom comparison commands via `-command`

## TCL Features We Support

| Option | Description | Status |
|--------|-------------|--------|
| `-ascii` | String comparison using code-point collation order | Supported |
| `-dictionary` | Dictionary-style comparison: case ignored except as tie-breaker, embedded numbers compared as integers | **Implemented** |
| `-integer` | Convert elements to integers and compare numerically | Supported |
| `-real` | Convert elements to floating-point and compare numerically | Supported |
| `-increasing` | Sort in ascending order (default) | Supported |
| `-decreasing` | Sort in descending order | Supported |
| `-nocase` | Case-insensitive string comparison | Supported |
| `-unique` | Remove duplicate elements after sorting | Supported |
| `-index index` | Sort sublists by element at the given index | Supported |
| `-indices` | Return list of indices in sorted order instead of values | Supported |
| `-command cmd` | Use a custom TCL command as the comparison function | **Implemented** |

## TCL Features We Do NOT Support

| Option | Description | TCL Behavior |
|--------|-------------|--------------|
| `-stride <strideLength>` | Treat the list as groups of N elements and sort the groups | Not implemented |

### Feature Details

#### -stride
Groups elements for sorting:
```tcl
lsort -stride 2 {carrot 10 apple 50 banana 25}
# Returns: apple 50 banana 25 carrot 10
```

## Notes on Implementation Differences

1. **Option abbreviations**: TCL accepts unique abbreviations of option names (e.g., `-dec` for `-decreasing`). Our implementation requires full option names.

2. **Error messages**: Our error message for unknown options lists all supported options.

3. **-nocase interaction**: In TCL, `-nocase` has no effect when combined with `-dictionary`, `-integer`, or `-real`. Our `-dictionary` mode already handles case-insensitivity internally.

4. **-unique behavior**: TCL specifies that only the "last set of duplicate elements" is retained. Our implementation correctly keeps the last duplicate by comparing each element with the next one.

5. **Stability**: TCL explicitly uses merge-sort for stable sorting. Our implementation delegates to the host's sort function, so stability depends on the host implementation.

6. **Simple index format**: Our `-index` option only supports simple integer indices, not TCL's `end-N` syntax or nested index lists.

7. **-command behavior**: The comparison command must return an integer. Negative values mean the first argument is less than the second, positive means greater, zero means equal. Non-integer return values cause an error.
