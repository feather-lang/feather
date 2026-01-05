# scan

Parse a string according to a format specification.

## Syntax

```tcl
scan string format ?varName ...?
```

## Parameters

- **string**: The string to parse
- **format**: A format string with conversion specifiers (like C scanf)
- **varName**: Variable names to store the parsed values

## Description

Parses the input string according to the format specification and stores the results in the specified variables. Returns the number of conversions successfully performed. If no variable names are provided, returns a list of the parsed values. Returns -1 when the end of the input string is reached before any conversions have been performed.

## Format Specifiers

| Specifier | Description |
|-----------|-------------|
| `%d` | Decimal integer |
| `%u` | Unsigned decimal integer |
| `%o` | Octal integer |
| `%x`, `%X` | Hexadecimal integer |
| `%b` | Binary integer |
| `%i` | Auto-detect base integer (0x for hex, 0 for octal, otherwise decimal) |
| `%f`, `%e`, `%E`, `%g`, `%G` | Floating-point number |
| `%s` | Non-whitespace string |
| `%c` | Single character (returns Unicode codepoint value) |
| `%[chars]` | Character set matching |
| `%[^chars]` | Negated character set |
| `%n` | Count of characters scanned so far |
| `%%` | Literal percent sign |

## Format Features

| Feature | Example | Description |
|---------|---------|-------------|
| Field width | `%10s` | Limits to 10 characters |
| Suppression | `%*d` | Discards the value (not stored) |
| Positional specifiers | `%2$d` | Assigns to 2nd variable |
| Size modifiers | `%ld`, `%lld` | Parsed for compatibility |
| Character ranges | `%[a-z]` | Matches lowercase letters |
| Bracket in charset | `%[]abc]` | `]` as first character matches literally |

## Return Value Modes

| Mode | Description |
|------|-------------|
| Variable mode | With varNames, returns count of successful conversions |
| Inline mode | Without varNames, returns list of values |
| EOF detection | Returns -1 when input exhausted before conversion |

## Unicode Character Handling

The `%c` specifier reads a single Unicode character and returns its codepoint value. This correctly handles multi-byte UTF-8 sequences:

- ASCII characters: Returns values 0-127
- 2-byte UTF-8: Returns codepoints U+0080 to U+07FF
- 3-byte UTF-8: Returns codepoints U+0800 to U+FFFF
- 4-byte UTF-8: Returns codepoints U+10000 to U+10FFFF (including emoji)

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicParsing = `scan "42 hello" "%d %s" num word
puts "Number: $num"
puts "Word: $word"`

const parsingWithoutVariables = `puts [scan "123 456" "%d %d"]`

const parsingFloatingPoint = `scan "3.14 2.71" "%f %f" pi e
puts "Pi: $pi, e: $e"`

const hexadecimalParsing = `scan "ff 10" "%x %x" a b
puts "a=$a b=$b"`

const parsingFixedWidthFields = `scan "John  25NYC" "%5s%3d%s" name age city
puts "Name: $name, Age: $age, City: $city"`

const countSuccessfulConversions = `set count [scan "hello 42" "%s %d" word num]
puts "Converted $count fields"
puts "word=$word num=$num"`

const binaryParsing = `scan "1010 1111" "%b %b" a b
puts "a=$a b=$b"`

const autoDetectBase = `# %i auto-detects base from prefix
scan "0xff 0777 42" "%i %i %i" hex oct dec
puts "hex=$hex oct=$oct dec=$dec"`

const unicodeCharacter = `# %c returns Unicode codepoint value
scan "A" "%c" codepoint
puts "A = $codepoint"`

const suppressValue = `# %*d skips a value without storing
scan "skip:42 keep:99" "skip:%*d keep:%d" val
puts "Kept value: $val"`
</script>

### Basic parsing

<WasmPlayground :tcl="basicParsing" />

### Parsing without variables

<WasmPlayground :tcl="parsingWithoutVariables" />

### Parsing floating-point

<WasmPlayground :tcl="parsingFloatingPoint" />

### Hexadecimal parsing

<WasmPlayground :tcl="hexadecimalParsing" />

### Parsing fixed-width fields

<WasmPlayground :tcl="parsingFixedWidthFields" />

### Count successful conversions

<WasmPlayground :tcl="countSuccessfulConversions" />

### Binary parsing

<WasmPlayground :tcl="binaryParsing" />

### Auto-detect base with %i

<WasmPlayground :tcl="autoDetectBase" />

### Unicode character codepoint

<WasmPlayground :tcl="unicodeCharacter" />

### Suppression with %*

<WasmPlayground :tcl="suppressValue" />

## See Also

- [format](./format) - Printf-style formatting
- [string](./string) - String operations

