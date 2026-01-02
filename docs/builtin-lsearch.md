# lsearch Builtin Implementation

## Summary of Our Implementation

Our `lsearch` implementation in `src/builtin_lsearch.c` provides list searching functionality with the following features:

- Three matching modes: exact, glob (default), and regexp
- Case-insensitive matching via `-nocase`
- Return all matches via `-all`
- Return values instead of indices via `-inline`
- Negated matching via `-not`
- Start searching from a specific index via `-start`
- Search within nested list elements via `-index`
- Search through groups of elements via `-stride`
- Binary search for sorted lists via `-sorted` with multiple comparison modes
- Insertion point search via `-bisect`

The implementation supports both linear search (O(n)) for unsorted lists and binary search (O(log n)) for sorted lists via the `-sorted` option.

## TCL Features We Support

| Option | Description |
|--------|-------------|
| `-exact` | Literal string comparison |
| `-glob` | Glob-style pattern matching (default) |
| `-regexp` | Regular expression matching |
| `-nocase` | Case-insensitive comparison |
| `-all` | Return all matching indices/values |
| `-inline` | Return matching values instead of indices |
| `-not` | Negate the match condition |
| `-start index` | Begin searching at specified index |
| `-index index` | Search within nested list elements at the specified index |
| `-stride N` | Treat list as groups of N elements, search by first element of each group |
| `-sorted` | Binary search for sorted lists (O(log n) algorithm) |
| `-bisect` | Inexact search returning last index <= pattern (increasing) or >= pattern (decreasing) |
| `-ascii` | Compare as Unicode strings (default for sorted search) |
| `-dictionary` | Dictionary-style comparison (case-insensitive, embedded numbers compared numerically) |
| `-integer` | Compare as integers |
| `-real` | Compare as floating-point values |
| `-increasing` | List is sorted in increasing order (default) |
| `-decreasing` | List is sorted in decreasing order |
| `-subindices` | Return full path indices {listindex subindex} for nested matches |

## TCL Features We Do NOT Support

All major lsearch features are now implemented.

## Notes on Implementation Differences

1. **Default behavior matches TCL**: Our default matching mode is `-glob`, which is correct per TCL specification.

2. **No `-nocase` support for `-regexp`**: In TCL, `-nocase` affects regexp matching. Our implementation passes regexp matching to the host's `regex_match` function, but it's unclear if nocase is honored for regexp mode.

3. **Simple index format**: Our `-start` and `-index` options only support simple integer indices, not TCL's `end-N` syntax or nested index lists.

4. **Binary search with -sorted**: The `-sorted` option enables O(log n) binary search. The comparison mode options (`-ascii`, `-dictionary`, `-integer`, `-real`) control how elements are compared during binary search.

5. **-bisect behavior**: With `-bisect`, returns the index of the last element <= pattern (for increasing order) or >= pattern (for decreasing order). Returns -1 if pattern is smaller than all elements.

6. **-sorted with -not**: When `-sorted` and `-not` are combined, the implementation falls back to linear search since binary search can't efficiently find non-matches.

7. **Error messages**: Our error message for unknown options includes the bad option name, which matches TCL behavior.

8. **-stride behavior**: With `-stride N`, the list is treated as groups of N elements. By default, searches match against the first element of each group. With `-index M`, searches match against the Mth element of each group (0-indexed). With `-inline`, returns all elements in the matching group.

9. **-subindices behavior**: Requires `-index`. Returns `{listindex subindex}` instead of just the list index. With `-inline`, returns the matched element value (the element at the specified subindex). Our implementation only supports simple integer indices, not TCL's nested index lists.
