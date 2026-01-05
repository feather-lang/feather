# lsearch

Searches a list for elements matching a pattern and returns their indices or values.

## Syntax

```tcl
lsearch ?options? list pattern
```

## Parameters

- **list**: The list to search
- **pattern**: The pattern to match against

## Options

### Match type
- **-exact**: Exact string match (default)
- **-glob**: Glob-style pattern matching
- **-regexp**: Regular expression matching

### Data type
- **-ascii**: Compare as strings (default)
- **-dictionary**: Dictionary-style comparison (handles embedded numbers naturally, e.g., "a2" < "a10")
- **-integer**: Compare as integers
- **-real**: Compare as floating-point numbers

### Sort order (for -sorted)
- **-sorted**: List is sorted, use binary search
- **-increasing**: Ascending order (default with -sorted)
- **-decreasing**: Descending order
- **-bisect**: Return index of last element less than or equal to pattern (for sorted lists)

### Result control
- **-all**: Return all matching indices
- **-inline**: Return matching values instead of indices
- **-not**: Return elements that do NOT match the pattern
- **-start index**: Start searching at index (supports `end`, `end-N`, and arithmetic expressions like `0+1`)

### Element selection
- **-index indexList**: Compare against a specific element within each list item. Can be a single index or a nested index list (e.g., `-index {0 1}` for deep access)
- **-subindices**: Return full index path to matched element (requires `-index`)
- **-stride count**: Treat list as groups of count elements, searching only the first element of each group

### Modifiers
- **-nocase**: Case-insensitive matching

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const findFirstOccurrence = `set colors {red green blue green yellow}
puts [lsearch $colors green]`

const findAllOccurrences = `set nums {1 2 3 2 4 2 5}
puts [lsearch -all $nums 2]`

const getMatchingValuesInline = `set items {apple banana apricot cherry}
puts [lsearch -all -inline -glob $items a*]`

const caseInsensitiveSearch = `set words {Hello WORLD foo Bar}
puts [lsearch -nocase $words hello]`

const searchSortedList = `set sorted {10 20 30 40 50}
puts [lsearch -integer -sorted $sorted 30]`

const startFromIndex = `set list {a b c a b c}
puts [lsearch -start 2 $list a]`

const notFoundReturnsNegativeOne = `set items {x y z}
puts [lsearch $items w]`

const findNonMatching = `set items {a b c d e}
puts [lsearch -not $items b]
puts [lsearch -not -all $items b]`

const searchByIndex = `set records {{alice 30} {bob 25} {carol 35}}
puts [lsearch -index 0 $records bob]
puts [lsearch -index 1 -inline $records 25]`

const strideSearch = `set pairs {name alice age 30 city boston}
puts [lsearch -stride 2 $pairs age]
puts [lsearch -stride 2 -inline $pairs age]`

const bisectSearch = `set sorted {a b d e}
puts [lsearch -sorted -bisect $sorted c]`

const dictionarySearch = `set versions {a1 a2 a10 a20}
puts [lsearch -exact -dictionary $versions a10]`

const endIndexExpr = `set list {a b c d e}
puts [lsearch -start end-2 $list d]`
</script>

### Find first occurrence

<WasmPlayground :tcl="findFirstOccurrence" />

### Find all occurrences

<WasmPlayground :tcl="findAllOccurrences" />

### Get matching values with -inline

<WasmPlayground :tcl="getMatchingValuesInline" />

### Case-insensitive search

<WasmPlayground :tcl="caseInsensitiveSearch" />

### Search sorted list

<WasmPlayground :tcl="searchSortedList" />

### Start from index

<WasmPlayground :tcl="startFromIndex" />

### Not found returns -1

<WasmPlayground :tcl="notFoundReturnsNegativeOne" />

### Find non-matching elements with -not

<WasmPlayground :tcl="findNonMatching" />

### Search by element index with -index

<WasmPlayground :tcl="searchByIndex" />

### Search strided lists with -stride

<WasmPlayground :tcl="strideSearch" />

### Find insertion point with -bisect

<WasmPlayground :tcl="bisectSearch" />

### Dictionary-style comparison

<WasmPlayground :tcl="dictionarySearch" />

### Using end-N index expressions

<WasmPlayground :tcl="endIndexExpr" />

## See Also

- [lindex](./lindex) - Get element by index
- [lsort](./lsort) - Sort list
- [string match](./string) - String pattern matching

