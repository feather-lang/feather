# concat Builtin Comparison

Comparison of feather's `concat` implementation against the official TCL manual.

## Summary of Our Implementation

Our implementation in `src/builtin_concat.c` provides the `concat` command which:

1. Accepts any number of arguments (zero or more)
2. Trims leading and trailing whitespace from each argument
3. Joins non-empty arguments with single spaces
4. Returns an empty string when called with no arguments
5. Skips arguments that become empty after trimming

The implementation uses a simple byte-by-byte approach to trim whitespace, recognizing space, tab, newline, and carriage return as whitespace characters.

## TCL Features We Support

- **Variable arguments**: Accepts any number of arguments (`concat ?arg arg ...?`)
- **Empty result for no args**: Returns empty string when no arguments provided
- **Whitespace trimming**: Trims leading and trailing whitespace from each argument
- **Space-separated joining**: Joins trimmed arguments with single spaces
- **Empty argument skipping**: Arguments that are empty after trimming are ignored entirely
- **Preserving internal spaces**: Spaces within arguments (not at edges) are preserved

## TCL Features We Do NOT Support

Our implementation appears to be **feature-complete** with respect to the TCL `concat` command. The TCL manual describes `concat` as:

> This command joins each of its arguments together with spaces after trimming leading and trailing white-space from each of them. Arguments that are empty (after trimming) are ignored entirely.

Our implementation matches this behavior exactly.

## Notes on Implementation Differences

1. **Whitespace characters**: Our implementation recognizes space (` `), tab (`\t`), newline (`\n`), and carriage return (`\r`) as whitespace. The TCL manual does not explicitly define what constitutes "white-space" for trimming, but standard TCL whitespace typically includes these characters. We may be missing some edge cases like form feed (`\f`) or vertical tab (`\v`), though these are rarely encountered in practice.

2. **List semantics**: The TCL manual notes that "if all of the arguments are lists, this has the same effect as concatenating them into a single list." Our implementation treats all arguments as strings and performs string concatenation. This produces equivalent results for the `concat` command itself, but the distinction matters for the internal representation of TCL objects (shimmering). In real TCL, the result may retain list structure; in our implementation, the result is always a string.

3. **Performance**: Our implementation performs byte-by-byte iteration for whitespace trimming, then uses string concatenation operations. This is straightforward but may be less efficient than a single-pass approach for very large inputs.

4. **Unicode handling**: Our implementation uses `byte_at` for whitespace detection, which works correctly for ASCII whitespace. The behavior with multi-byte UTF-8 whitespace characters (e.g., non-breaking space U+00A0) is not explicitly handled but would not cause incorrect trimming of ASCII whitespace.
