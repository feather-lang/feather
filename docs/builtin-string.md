# Feather `string` Builtin Comparison with TCL

This document compares the feather implementation of the `string` command with the official TCL 9 specification.

## Summary of Our Implementation

Feather implements the following `string` subcommands in `src/builtin_string.c`:

- `string length` - Returns character count of a string
- `string index` - Returns character at specified index
- `string range` - Returns substring between two indices
- `string match` - Glob-style pattern matching with optional `-nocase`
- `string toupper` - Converts string to uppercase
- `string tolower` - Converts string to lowercase
- `string trim` - Removes leading/trailing characters
- `string trimleft` - Removes leading characters
- `string trimright` - Removes trailing characters
- `string map` - Substring replacement with optional `-nocase`

## TCL Features We Support

| Subcommand | Status | Notes |
|------------|--------|-------|
| `string length string` | Supported | Returns character count (Unicode-aware via `ops->rune.length`) |
| `string index string charIndex` | Supported | Supports TCL index expressions including `end`, `end-N`, arithmetic |
| `string range string first last` | Supported | Full index expression support |
| `string match ?-nocase? pattern string` | Supported | Glob patterns with `*`, `?`, `[chars]`, `\x` escapes |
| `string toupper string` | Partial | Converts entire string only; `?first? ?last?` arguments are parsed but ignored |
| `string tolower string` | Partial | Converts entire string only; `?first? ?last?` arguments are parsed but ignored |
| `string trim string ?chars?` | Supported | Default whitespace or custom character set |
| `string trimleft string ?chars?` | Supported | Default whitespace or custom character set |
| `string trimright string ?chars?` | Supported | Default whitespace or custom character set |
| `string map ?-nocase? mapping string` | Supported | Key-value pair substitution |

## TCL Features We Do NOT Support

### Missing Subcommands

| Subcommand | Description |
|------------|-------------|
| `string cat ?string1? ?string2...?` | Concatenates strings (alternative to juxtaposition) |
| `string compare ?-nocase? ?-length len? string1 string2` | Lexicographic comparison returning -1, 0, or 1 |
| `string equal ?-nocase? ?-length len? string1 string2` | Equality test returning 0 or 1 |
| `string first needleString haystackString ?startIndex?` | Find first occurrence of substring |
| `string last needleString haystackString ?lastIndex?` | Find last occurrence of substring |
| `string insert string index insertString` | Insert substring at index |
| `string is class ?-strict? ?-failindex varname? string` | Character class testing (alnum, alpha, ascii, boolean, control, dict, digit, double, false, graph, integer, list, lower, print, punct, space, true, upper, wideinteger, wordchar, xdigit) |
| `string repeat string count` | Repeat string N times |
| `string replace string first last ?newstring?` | Replace range with optional new string |
| `string reverse string` | Reverse character order |
| `string totitle string ?first? ?last?` | Title case conversion |
| `string wordend string charIndex` | Find end of word (obsolete) |
| `string wordstart string charIndex` | Find start of word (obsolete) |

### Partial Implementations

| Subcommand | Missing Feature |
|------------|-----------------|
| `string toupper string ?first? ?last?` | Range-limited conversion (first/last arguments ignored) |
| `string tolower string ?first? ?last?` | Range-limited conversion (first/last arguments ignored) |

## Notes on Implementation Differences

### Index Expressions
Our implementation uses `feather_parse_index()` which appears to support TCL-style index expressions including `end`, `end-N`, and arithmetic forms like `M+N` and `M-N`.

### Unicode Handling
- `string length` uses `ops->rune.length` for Unicode character count
- `string index` and `string range` use rune-based operations
- `string toupper`/`tolower` use `ops->rune.to_upper`/`to_lower`
- `string match` with `-nocase` uses `ops->rune.fold` for case folding
- `string trim`/`trimleft`/`trimright` operate on bytes, not runes (may cause issues with multi-byte Unicode characters in the trim character set)
- `string map` has a TODO noting it uses byte-level comparison with ASCII-only case folding for `-nocase`

### Whitespace Definition
Our implementation defines whitespace as: space, tab, newline, carriage return, vertical tab, form feed. TCL defines whitespace as any character for which `string is space` returns 1, plus NUL (`\0`). This includes additional Unicode whitespace characters like mongolian vowel separator (U+180e), zero width space (U+200b), word joiner (U+2060), and zero width no-break space (U+feff).

### Error Messages
Error messages match TCL format, e.g., `wrong # args: should be "string length string"`.

### Case Folding in `string map`
The implementation note indicates that `-nocase` uses case folding which may change string length for certain Unicode characters. The code handles this by matching against the folded string but outputs from the folded string for non-matched characters, which may differ from TCL behavior.
