# lsort

Sorts a list and returns the sorted result.

## Syntax

```tcl
lsort ?options? list
```

## Parameters

- **list**: The list to sort

## Options

### Sort type
- **-ascii**: Dictionary-style string comparison (default)
- **-integer**: Compare as integers
- **-real**: Compare as floating-point numbers

### Sort order
- **-increasing**: Ascending order (default)
- **-decreasing**: Descending order

### Modifiers
- **-nocase**: Case-insensitive comparison
- **-unique**: Remove duplicate values

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const sortStringsAlphabetically = `set words {banana apple cherry date}
puts [lsort $words]`

const sortNumbersAsIntegers = `set nums {10 2 30 4 25}
puts [lsort -integer $nums]`

const sortInDescendingOrder = `set values {5 2 8 1 9}
puts [lsort -integer -decreasing $values]`

const caseInsensitiveSort = `set mixed {Apple banana CHERRY date}
puts [lsort -nocase $mixed]`

const removeDuplicates = `set dups {a b a c b c a}
puts [lsort -unique $dups]`

const sortFloatingPointNumbers = `set floats {3.14 1.5 2.7 0.5}
puts [lsort -real $floats]`

const combineOptions = `set items {Foo bar BAZ foo BAR baz}
puts [lsort -nocase -unique $items]`
</script>

### Sort strings alphabetically

<WasmPlayground :tcl="sortStringsAlphabetically" />

### Sort numbers as integers

<WasmPlayground :tcl="sortNumbersAsIntegers" />

### Sort in descending order

<WasmPlayground :tcl="sortInDescendingOrder" />

### Case-insensitive sort

<WasmPlayground :tcl="caseInsensitiveSort" />

### Remove duplicates

<WasmPlayground :tcl="removeDuplicates" />

### Sort floating-point numbers

<WasmPlayground :tcl="sortFloatingPointNumbers" />

### Combine options

<WasmPlayground :tcl="combineOptions" />

## See Also

- [lsearch](./lsearch) - Search list
- [lreverse](./lreverse) - Reverse list
- [list](./list) - Create a list

