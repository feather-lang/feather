# lindex

Returns a list element by index.

## Syntax

```tcl
lindex list index
```

## Parameters

- **list**: The list to index into
- **index**: The index of the element to return (0-based, or negative from end)

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicIndexing = `set colors {red green blue yellow}
puts [lindex $colors 0]
puts [lindex $colors 2]`

const negativeIndices = `set numbers {1 2 3 4 5}
puts "Last: [lindex $numbers -1]"
puts "Second to last: [lindex $numbers -2]"`

const outOfBounds = `set items {a b c}
set result [lindex $items 10]
puts "Result: $result"`
</script>

### Basic indexing

<WasmPlayground :tcl="basicIndexing" />

### Negative indices

<WasmPlayground :tcl="negativeIndices" />

### Out of bounds

<WasmPlayground :tcl="outOfBounds" />

## See Also

- [list](./list) - Create a list
- [llength](./llength) - Get list length
- [lassign](./lassign) - Assign elements to variables

