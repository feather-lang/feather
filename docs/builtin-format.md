# Feather `format` Command Implementation

This document compares Feather's implementation of the `format` command with the TCL specification.

## Summary of Our Implementation

Our implementation is in `src/builtin_format.c` and provides a subset of TCL's `format` command functionality. The implementation:

- Parses format specifiers following the `%[position$][flags][width][.precision][size]specifier` pattern
- Supports positional arguments using the `%n$` XPG3 syntax
- Handles width and precision from arguments using `*`
- Implements integer, string, character, floating-point, and pointer formatting

## TCL Features We Support

### Conversion Specifiers

| Specifier | Status | Description |
|-----------|--------|-------------|
| `%d` | Supported | Signed decimal integer |
| `%i` | Supported | Signed decimal integer (equivalent to `%d`) |
| `%u` | Supported | Unsigned decimal integer |
| `%o` | Supported | Unsigned octal integer |
| `%x` | Supported | Unsigned hexadecimal (lowercase) |
| `%X` | Supported | Unsigned hexadecimal (uppercase) |
| `%b` | Supported | Unsigned binary integer |
| `%c` | Supported | Unicode character from integer code point |
| `%s` | Supported | String |
| `%f` | Supported | Floating-point decimal notation |
| `%e` | Supported | Scientific notation (lowercase) |
| `%E` | Supported | Scientific notation (uppercase) |
| `%g` | Supported | Shorter of `%e` or `%f` |
| `%G` | Supported | Shorter of `%E` or `%f` |
| `%a` | Supported | Hexadecimal floating-point (lowercase) |
| `%A` | Supported | Hexadecimal floating-point (uppercase) |
| `%p` | Supported | Pointer (hex with `0x` prefix) |
| `%%` | Supported | Literal percent sign |

### Flags

| Flag | Status | Description |
|------|--------|-------------|
| `-` | Supported | Left-justify in field |
| `+` | Supported | Always show sign for numbers |
| ` ` (space) | Supported | Space before positive numbers |
| `0` | Supported | Zero-pad numbers |
| `#` | Supported | Alternate form (prefixes for hex/octal/binary) |

### Other Features

- **Positional arguments (`%n$`)**: Supported - allows reordering arguments
- **Field width**: Supported - both literal and `*` from argument
- **Precision**: Supported - both literal and `*` from argument
- **Size modifiers**: **Fully Supported** - `h` (16-bit), `l`/`j`/`q` (64-bit), `ll`/`L` (no truncation), `z`/`t` (pointer size), and no modifier (32-bit)
- **Error on mixing positional and sequential**: Supported

## TCL Features We Do NOT Support

### Alternate Form (`#`) Differences (All Now Supported!)

TCL's `#` flag has specific behavior:

| Conversion | TCL Behavior | Our Behavior |
|------------|--------------|--------------|
| `%#o` | Adds `0o` prefix (unless zero) | Adds `0o` prefix (unless zero) - Matches |
| `%#x`/`%#X` | Adds `0x` prefix (unless zero) | Adds `0x` prefix (unless zero) - Matches |
| `%#b` | Adds `0b` prefix (unless zero) | Adds `0b` prefix (unless zero) - Matches |
| `%#d` | Adds `0d` prefix (unless zero) | **Implemented** - Matches |
| `%#0Nd` | Adds `0d` with zero padding | **Implemented** - Matches |
| `%#f`, `%#e`, etc. | Guarantees decimal point | **Implemented** - Matches |
| `%#g`, `%#G` | Keeps trailing zeroes | **Implemented** - Matches |

### `%n` Specifier

TCL explicitly states that `%n` (which writes the number of characters written so far to a variable in C's sprintf) is **not supported**. Our implementation also does not support it, which matches TCL behavior.

## Notes on Implementation Differences

### Zero Padding with Signs

Our implementation correctly handles zero-padding with signed numbers: when using `%0Nd` with a negative number, the zeros appear after the sign (e.g., `%-0005` instead of `0000-5`).

### Floating-Point Formatting

Floating-point formatting (`%f`, `%e`, `%g`, `%a`, etc.) is delegated to the host's `ops->dbl.format()` function. The exact behavior depends on the host implementation.

### NaN Handling

Our implementation explicitly errors on NaN values for floating-point conversions, which matches TCL's behavior.

### Unicode Character Conversion (`%c`)

Our implementation properly handles Unicode code points and encodes them as UTF-8. The `%c` specifier requires an integer argument, which matches TCL's requirement.

### Error Messages

Our error messages generally match TCL's style but may have slightly different wording:

- "wrong # args: should be..." for argument count errors
- "expected integer but got..." for type conversion errors
- "cannot mix \"%\" and \"%n$\" conversion specifiers" for positional/sequential mixing
- "format string ended in middle of field specifier" for incomplete specifiers

### Precision for Integers

For integer conversions, precision specifies the minimum number of digits. If the number has fewer digits, leading zeros are added. This matches TCL behavior.

### Precision for Strings

For `%s` conversions, precision specifies the maximum number of characters to print. Longer strings are truncated. This matches TCL behavior.

### Width from Arguments

When width is specified as `*`, a negative width value causes left-justification (equivalent to `-` flag). This matches TCL behavior.
