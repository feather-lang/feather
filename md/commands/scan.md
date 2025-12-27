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

Parses the input string according to the format specification and stores the results in the specified variables. Returns the number of conversions successfully performed. If no variable names are provided, returns a list of the parsed values.

## Format Specifiers

| Specifier | Description |
|-----------|-------------|
| `%d`, `%i` | Decimal integer |
| `%u` | Unsigned integer |
| `%o` | Octal integer |
| `%x`, `%X` | Hexadecimal integer |
| `%f`, `%e`, `%g` | Floating-point number |
| `%s` | String (whitespace-delimited) |
| `%c` | Single character |
| `%[chars]` | Character set |
| `%n` | Count of characters read so far |
| `%%` | Literal percent sign |

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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
</script>

### Basic parsing

<FeatherPlayground :code="basicParsing" />

### Parsing without variables

<FeatherPlayground :code="parsingWithoutVariables" />

### Parsing floating-point

<FeatherPlayground :code="parsingFloatingPoint" />

### Hexadecimal parsing

<FeatherPlayground :code="hexadecimalParsing" />

### Parsing fixed-width fields

<FeatherPlayground :code="parsingFixedWidthFields" />

### Count successful conversions

<FeatherPlayground :code="countSuccessfulConversions" />

## See Also

- [format](./format) - Printf-style formatting
- [string](./string) - String operations
