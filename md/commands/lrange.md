# lrange

Extracts a sublist from a list, returning elements from the first index to the last index (inclusive).

## Syntax

```tcl
lrange list first last
```

## Parameters

- **list**: The list to extract from
- **first**: Starting index (0-based, or `end`, `end-N`)
- **last**: Ending index (inclusive, or `end`, `end-N`)

Indices are clamped to the valid range of the list. If first > last, an empty list is returned.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicSublistExtraction = `set colors {red orange yellow green blue}
puts [lrange $colors 1 3]`

const usingEndRelativeIndices = `set nums {1 2 3 4 5 6 7 8 9 10}
puts [lrange $nums end-2 end]`

const getAllButFirstAndLast = `set items {a b c d e}
puts [lrange $items 1 end-1]`

const indicesClampedToValidRange = `set short {x y z}
puts [lrange $short 0 100]`
</script>

### Basic sublist extraction

<WasmPlayground :tcl="basicSublistExtraction" />

### Using end-relative indices

<WasmPlayground :tcl="usingEndRelativeIndices" />

### Get all but first and last

<WasmPlayground :tcl="getAllButFirstAndLast" />

### Indices clamped to valid range

<WasmPlayground :tcl="indicesClampedToValidRange" />

## See Also

- [lindex](./lindex) - Get single element
- [lreplace](./lreplace) - Replace elements in range
- [list](./list) - Create a list

