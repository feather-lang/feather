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

The implementation searches through list elements linearly and returns either the index of the first match (or -1 if not found), or when combined with `-all` and/or `-inline`, returns lists of indices or matching values.

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
| `-stride N` | Treat list as groups of N elements, search by first element of each group | **Implemented** |

## TCL Features We Do NOT Support

### Matching Style Options

| Option | Description |
|--------|-------------|
| `-sorted` | Binary search for sorted lists (more efficient algorithm) |

### Content Description Options

These options control how list elements are compared (only meaningful with `-exact` and `-sorted`):

| Option | Description |
|--------|-------------|
| `-ascii` | Compare as Unicode strings (default in TCL) - **Accepted** (no effect on linear search) |
| `-dictionary` | Dictionary-style comparison - **Accepted** (no effect on linear search) |
| `-integer` | Compare as integers - **Accepted** (no effect on linear search) |
| `-real` | Compare as floating-point values - **Accepted** (no effect on linear search) |

Note: These options are accepted for compatibility but have no effect without `-sorted` mode.

### Sorted List Options

These options control the sort order when using `-sorted`:

| Option | Description |
|--------|-------------|
| `-increasing` | List is sorted in increasing order (default) - **Accepted** (no effect without -sorted) |
| `-decreasing` | List is sorted in decreasing order - **Accepted** (no effect without -sorted) |
| `-bisect` | Inexact search returning last index <= pattern (increasing) or >= pattern (decreasing) - Not implemented |

### Nested List Options

| Option | Description |
|--------|-------------|
| `-subindices` | Return full path indices for nested matches - Not implemented |

## Notes on Implementation Differences

1. **Default behavior matches TCL**: Our default matching mode is `-glob`, which is correct per TCL specification.

2. **No `-nocase` support for `-regexp`**: In TCL, `-nocase` affects regexp matching. Our implementation passes regexp matching to the host's `regex_match` function, but it's unclear if nocase is honored for regexp mode.

3. **Simple index format**: Our `-start` and `-index` options only support simple integer indices, not TCL's `end-N` syntax or nested index lists.

4. **Content comparison options accepted**: The `-ascii`, `-dictionary`, `-integer`, `-real`, `-increasing`, and `-decreasing` options are accepted for compatibility, but they only affect binary search mode which is not implemented. Linear search always uses string comparison for matching.

5. **No binary search optimization**: TCL's `-sorted` option enables O(log n) binary search. Our implementation always uses O(n) linear search.

6. **Error messages**: Our error message for unknown options includes the bad option name, which matches TCL behavior.

7. **-stride behavior**: With `-stride N`, the list is treated as groups of N elements. By default, searches match against the first element of each group. With `-index M`, searches match against the Mth element of each group (0-indexed). With `-inline`, returns all elements in the matching group.
