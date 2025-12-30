# Feather `split` Builtin - Comparison with TCL

## Summary of Our Implementation

The `split` command in Feather is implemented in `src/builtin_split.c`. It splits a string into a list based on delimiter characters.

**Syntax:** `split string ?splitChars?`

Our implementation supports:

- Splitting a string by specified delimiter characters
- Default splitting on whitespace (space, tab, newline, carriage return)
- Splitting into individual characters when an empty delimiter is provided
- Generating empty list elements for adjacent delimiters
- Proper handling of empty input strings

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic syntax `split string ?splitChars?` | Supported | Matches TCL signature |
| Default whitespace splitting | Supported | Uses space, tab, newline, carriage return |
| Custom delimiter characters | Supported | Each character in splitChars is a delimiter |
| Empty delimiter (split into chars) | Supported | `split "abc" {}` returns `{a b c}` |
| Empty list elements for adjacent delimiters | Supported | Correctly generates empty elements |
| Empty string input | Supported | Returns empty list |

## TCL Features We Do NOT Support

Based on comparing our implementation with the TCL 9 manual, our implementation appears to be feature-complete for the `split` command. There are no missing features.

## Notes on Implementation Differences

### 1. Default Whitespace Characters

Our implementation defines default whitespace as:
- Space (` `)
- Tab (`\t`)
- Newline (`\n`)
- Carriage return (`\r`)

This matches standard TCL behavior for whitespace-based splitting.

### 2. Byte-Level vs Character-Level Splitting

Our implementation operates at the byte level (`ops->string.byte_at`, `ops->string.byte_length`). This means:

- ASCII strings work correctly
- Multi-byte UTF-8 characters may not be handled correctly when:
  - Used as delimiter characters
  - Split into individual characters with empty delimiter

TCL's `split` command operates on Unicode characters. For full TCL compatibility with international text, character-level operations would be needed instead of byte-level operations.

### 3. Performance Characteristics

Our implementation uses a simple O(n*m) algorithm where:
- n = length of input string
- m = length of splitChars

For each character in the input, we scan through all delimiter characters. TCL implementations may use more optimized lookup structures (like a character set bitmap) for better performance with large delimiter sets.

### 4. Memory Management

Our implementation builds the result list incrementally using `ops->list.push`, which is appropriate for the host-managed allocation model used in Feather.
