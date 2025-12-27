# split

Split a string into a list.

## Syntax

```tcl
split string ?splitChars?
```

## Parameters

- **string**: The string to split
- **splitChars**: Characters to split on (default: whitespace)

## Description

Splits a string into a list of elements. Each character in `splitChars` is treated as a delimiter. If `splitChars` is an empty string, the string is split into individual characters.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const defaultSplit = `puts [split "hello world foo bar"]`

const splitOnSpecificChar = `puts [split "a,b,c,d" ","]
puts [split "usr/local/bin" "/"]`

const splitIntoChars = `puts [split "hello" ""]`

const multipleDelimiters = `puts [split "a:b;c:d" ":;"]`

const parsingCsvData = `set line "name,age,city"
foreach field [split $line ","] {
    puts "Field: $field"
}`
</script>

### Default split (whitespace)

<FeatherPlayground :code="defaultSplit" />

### Split on specific character

<FeatherPlayground :code="splitOnSpecificChar" />

### Split into characters

<FeatherPlayground :code="splitIntoChars" />

### Multiple delimiter characters

<FeatherPlayground :code="multipleDelimiters" />

### Parsing CSV-like data

<FeatherPlayground :code="parsingCsvData" />

## See Also

- [join](./join) - Join list elements into string
- [string](./string) - String operations
- [list](./list) - Create a list
