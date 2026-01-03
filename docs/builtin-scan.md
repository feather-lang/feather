# Feather `scan` Builtin - Comparison with TCL

This document compares feather's implementation of the `scan` command with the official TCL 8.4+ specification.

## Summary of Our Implementation

The feather `scan` command is implemented in `src/builtin_scan.c`. It provides a C-based implementation that parses strings using format specifiers in the style of `sscanf`, supporting both variable-assignment mode and inline mode (returning results as a list).

Key implementation characteristics:
- Supports positional specifiers (`%n$`)
- Supports field width limits
- Supports suppression with `*`
- Recognizes size modifiers (`h`, `l`, `ll`, `z`, `t`, `L`, `j`, `q`) but ignores them for actual truncation
- Maximum of 64 conversion results
- 4096-byte buffer limit for string/charset conversions

## TCL Features We Support

### Conversion Specifiers

| Specifier | Description | Status |
|-----------|-------------|--------|
| `%d` | Decimal integer | Supported |
| `%u` | Unsigned decimal integer | Supported (treated same as `%d`) |
| `%o` | Octal integer | Supported |
| `%x`, `%X` | Hexadecimal integer | Supported |
| `%b` | Binary integer | Supported |
| `%i` | Auto-detect base integer (C convention) | Supported |
| `%c` | Single character (as Unicode codepoint value) | Supported |
| `%s` | Non-whitespace string | Supported |
| `%f`, `%e`, `%E`, `%g`, `%G` | Floating-point number | Supported |
| `%[chars]` | Character set matching | Supported |
| `%[^chars]` | Negated character set | Supported |
| `%n` | Characters scanned so far | Supported |
| `%%` | Literal percent | Supported |

### Format Features

| Feature | Description | Status |
|---------|-------------|--------|
| Field width | `%10s` limits to 10 characters | Supported |
| Suppression | `%*d` discards the value | Supported |
| Positional specifiers | `%2$d` assigns to 2nd variable | Supported |
| Size modifiers | `%ld`, `%lld`, etc. | Parsed but ignored |
| Whitespace matching | Whitespace in format matches any whitespace | Supported |
| Literal matching | Non-% characters must match exactly | Supported |
| Character ranges in charset | `%[a-z]` | Supported |
| `]` as first character in charset | `%[]abc]` | Supported |

### Return Value Modes

| Mode | Description | Status |
|------|-------------|--------|
| Variable mode | With varNames, returns count of conversions | Supported |
| Inline mode | Without varNames, returns list of values | Supported |
| EOF detection | Returns -1 when input exhausted before conversion | Supported |

## TCL Features We Do NOT Support

### Integer Truncation Based on Size Modifiers

TCL performs actual integer truncation based on size modifiers:
- `%hd` or `%d` (no modifier): Truncates to 32-bit range (max 2147483647)
- `%ld`, `%qd`, `%jd`: Truncates to 64-bit "wide" range
- `%lld`, `%Ld`: Unlimited integer range
- `%zd`, `%td`: Platform-dependent (32 or 64 bit based on pointer size)

**Our implementation**: Size modifiers are parsed and accepted syntactically, but **no truncation is performed**. All integers are stored as 64-bit values without range limiting.

### Unsigned Integer Conversion (`%u`)

TCL's `%u` specifier:
> "The integer value is truncated as required by the size modifier value, and the corresponding unsigned value for that truncated range is computed and stored in the variable as a decimal string."

**Our implementation**: `%u` is treated identically to `%d`. No unsigned reinterpretation or truncation is performed.

### Positional Specifier Validation

TCL requires:
> "Every varName on the argument list must correspond to exactly one conversion specifier or an error is generated"

**Our implementation**: We do not validate that every variable has a corresponding positional specifier. Extra variables are silently ignored in positional mode.

### Empty String Return in Inline Mode

TCL specifies:
> "In the inline case, an empty string is returned when the end of the input string is reached before any conversions have been performed."

**Our implementation**: We return an empty list `{}` rather than an empty string `""`. In TCL, these are equivalent due to shimmering, so this is functionally correct.

## Notes on Implementation Differences

### Whitespace Handling

Our implementation matches TCL's whitespace handling:
- Whitespace in format matches zero or more whitespace characters in input
- `%c` and `%[...]` do not skip leading whitespace
- All other specifiers skip leading whitespace before conversion

### Unicode Character Handling for `%c`

TCL states that `%c` reads "a single character" and stores "its Unicode value".

**Our implementation**: We decode UTF-8 sequences at the current byte position to read a single Unicode codepoint. This correctly handles:
- ASCII characters (1 byte): Returns values 0-127
- 2-byte UTF-8 sequences: Returns codepoints U+0080 to U+07FF
- 3-byte UTF-8 sequences: Returns codepoints U+0800 to U+FFFF
- 4-byte UTF-8 sequences: Returns codepoints U+10000 to U+10FFFF (including emoji)

Examples:
- `scan "A" "%c"` returns 65 (U+0041)
- `scan "é" "%c"` returns 233 (U+00E9)
- `scan "中" "%c"` returns 20013 (U+4E2D)
- `scan "👋" "%c"` returns 128075 (U+1F44B)

The input position advances by the number of UTF-8 bytes consumed (1-4 bytes per character).

### Hexadecimal Prefix Handling

For `%x`/`%X`, we correctly handle optional `0x`/`0X` prefix, consuming it if present.

For `%i` (auto-detect), we correctly detect:
- `0x...` as hexadecimal
- `0...` as octal
- Otherwise decimal

### Buffer Limitations

Our implementation has fixed limits:
- Maximum 64 conversion results
- Maximum 4096 bytes for string/charset conversions

TCL does not have these arbitrary limits.

### Error Messages

Our error messages are similar but may not be identical to TCL's. For example:
- Wrong number of arguments: `wrong # args: should be "scan string format ?varName ...?"`
- Mixed positional/sequential: `cannot mix "%" and "%n$" conversion specifiers`

### Floating-Point Precision

Our floating-point parsing implementation in `scan_float_obj` uses iterative multiplication/division for exponents, which may have minor precision differences compared to TCL's implementation that likely uses the C library's `strtod` or equivalent.

### Not Implemented: `%p` Pointer Specifier

TCL explicitly documents that `%p` (pointer) is not supported. We also do not support it, which is correct behavior.
