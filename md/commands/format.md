# format

Printf-style string formatting.

## Syntax

```tcl
format formatString ?arg ...?
```

## Parameters

- **formatString**: A format string containing literal text and conversion specifiers
- **arg**: Values to substitute into the format string

## Format Specifiers

| Specifier | Description |
|-----------|-------------|
| `%d`, `%i` | Signed decimal integer |
| `%u` | Unsigned decimal integer |
| `%o` | Octal integer |
| `%x`, `%X` | Hexadecimal integer (lowercase/uppercase) |
| `%b` | Binary integer |
| `%c` | Character (from integer code) |
| `%s` | String |
| `%f` | Floating-point (fixed notation) |
| `%e`, `%E` | Floating-point (exponential notation) |
| `%g`, `%G` | Floating-point (shorter of %f or %e) |
| `%a`, `%A` | Floating-point (hexadecimal) |
| `%p` | Pointer (platform-specific) |
| `%%` | Literal percent sign |

## Flags

| Flag | Description |
|------|-------------|
| `-` | Left-justify within field width |
| `0` | Pad with zeros instead of spaces |
| `+` | Always show sign for numeric values |
| `#` | Alternate form (prefix for octal/hex) |
| ` ` | Space before positive numbers |

## Width and Precision

- **width**: Minimum field width (e.g., `%10s`)
- **.precision**: For floats, decimal places; for strings, max length (e.g., `%.2f`)
- **positional**: Use `%n$` to reference argument by position (e.g., `%2$s`)

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicFormatting = `puts [format "Hello, %s!" "World"]
puts [format "Count: %d" 42]`

const numberFormatting = `puts [format "Decimal: %d" 255]
puts [format "Hex: %x" 255]
puts [format "Hex (upper): %X" 255]
puts [format "Octal: %o" 255]
puts [format "Binary: %b" 255]`

const floatingPointFormatting = `set pi 3.14159265
puts [format "Default: %f" $pi]
puts [format "2 decimals: %.2f" $pi]
puts [format "Scientific: %e" $pi]`

const widthAndAlignment = `puts [format "|%10s|" "hello"]
puts [format "|%-10s|" "hello"]
puts [format "|%05d|" 42]`

const positionalArguments = `puts [format "%2\$s %1\$s" "World" "Hello"]`

const characterConversion = `puts [format "%c%c%c" 65 66 67]`
</script>

### Basic formatting

<FeatherPlayground :code="basicFormatting" />

### Number formatting

<FeatherPlayground :code="numberFormatting" />

### Floating-point formatting

<FeatherPlayground :code="floatingPointFormatting" />

### Width and alignment

<FeatherPlayground :code="widthAndAlignment" />

### Positional arguments

<FeatherPlayground :code="positionalArguments" />

### Character conversion

<FeatherPlayground :code="characterConversion" />

## See Also

- [scan](./scan) - Parse strings with format
- [string](./string) - String operations
- [subst](./subst) - Variable and command substitution
