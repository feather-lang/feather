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
- **-integer**: Compare as integers

### Sort order (for -sorted)
- **-sorted**: List is sorted, use binary search
- **-increasing**: Ascending order (default with -sorted)
- **-decreasing**: Descending order

### Result control
- **-all**: Return all matching indices
- **-inline**: Return matching values instead of indices
- **-start index**: Start searching at index

### Modifiers
- **-nocase**: Case-insensitive matching

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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
</script>

### Find first occurrence

<FeatherPlayground :code="findFirstOccurrence" />

### Find all occurrences

<FeatherPlayground :code="findAllOccurrences" />

### Get matching values with -inline

<FeatherPlayground :code="getMatchingValuesInline" />

### Case-insensitive search

<FeatherPlayground :code="caseInsensitiveSearch" />

### Search sorted list

<FeatherPlayground :code="searchSortedList" />

### Start from index

<FeatherPlayground :code="startFromIndex" />

### Not found returns -1

<FeatherPlayground :code="notFoundReturnsNegativeOne" />

## See Also

- [lindex](./lindex) - Get element by index
- [lsort](./lsort) - Sort list
- [string match](./string) - String pattern matching
