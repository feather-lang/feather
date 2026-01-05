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
- **-dictionary**: Natural sort that handles embedded numbers (e.g., "a2" before "a10")
- **-integer**: Compare as integers
- **-real**: Compare as floating-point numbers

### Sort order
- **-increasing**: Ascending order (default)
- **-decreasing**: Descending order

### Result control
- **-indices**: Return the sorted indices instead of the sorted values
- **-unique**: Remove duplicate values from the result

### Sub-list sorting
- **-index indexSpec**: Sort by a specific element within each sub-list. The indexSpec can be a number (0-based), `end`, `end-N`, or a list of indices for nested access
- **-stride length**: Treat the list as groups of `length` elements, sorting by the first element of each group. Length must be at least 2

### Custom comparison
- **-command cmdName**: Use a custom comparison procedure. The procedure must take two arguments and return a negative integer, zero, or positive integer

### Modifiers
- **-nocase**: Case-insensitive comparison (works with -ascii and -dictionary)

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

const dictionarySort = `# Dictionary sort handles embedded numbers naturally
set files {file1.txt file10.txt file2.txt file20.txt}
puts [lsort -dictionary $files]`

const sortByIndex = `# Sort list of pairs by second element
set pairs {{a 3} {b 1} {c 2}}
puts [lsort -index 1 $pairs]`

const sortByIndexInteger = `# Sort by second element as integers
set data {{a 10} {b 2} {c 20}}
puts [lsort -integer -index 1 $data]`

const getIndices = `# Get sorted indices instead of values
set items {c a b}
puts [lsort -indices $items]`

const strideSort = `# Sort key-value pairs by key (stride of 2)
set pairs {carrot 10 apple 50 banana 25}
puts [lsort -stride 2 $pairs]`

const strideSortByValue = `# Sort key-value pairs by value
set pairs {carrot 10 apple 50 banana 25}
puts [lsort -stride 2 -index 1 -integer $pairs]`

const customCommand = `# Custom comparison by string length
proc lencmp {a b} {
    expr {[string length $a] - [string length $b]}
}
puts [lsort -command lencmp {cat elephant dog mouse}]`
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

### Dictionary sort with embedded numbers

<WasmPlayground :tcl="dictionarySort" />

### Sort by sub-list element

<WasmPlayground :tcl="sortByIndex" />

### Sort by sub-list element as integers

<WasmPlayground :tcl="sortByIndexInteger" />

### Get sorted indices

<WasmPlayground :tcl="getIndices" />

### Sort with stride (key-value pairs)

<WasmPlayground :tcl="strideSort" />

### Sort stride pairs by value

<WasmPlayground :tcl="strideSortByValue" />

### Custom comparison command

<WasmPlayground :tcl="customCommand" />

## See Also

- [lsearch](./lsearch) - Search list
- [lreverse](./lreverse) - Reverse list
- [list](./list) - Create a list

