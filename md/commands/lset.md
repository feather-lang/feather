# lset

Sets an element in a list variable at a specified index.

## Syntax

```tcl
lset varName index value
```

## Parameters

- **varName**: Name of the list variable (not the value)
- **index**: Index of element to set (0-based, or `end`, `end-N`)
- **value**: New value for the element

Returns the new list value. Errors if the index is out of bounds.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const setElementByIndex = `set colors {red green blue}
lset colors 1 yellow
puts $colors`

const setLastElement = `set nums {1 2 3 4 5}
lset nums end 99
puts $nums`

const setUsingEndRelativeIndex = `set items {a b c d e}
lset items end-1 X
puts $items`

const modifyInLoop = `set squares {0 0 0 0 0}
for {set i 0} {$i < 5} {incr i} {
    lset squares $i [expr {$i * $i}]
}
puts $squares`

const returnsNewValue = `set list {a b c}
puts [lset list 0 X]`
</script>

### Set element by index

<WasmPlayground :tcl="setElementByIndex" />

### Set last element

<WasmPlayground :tcl="setLastElement" />

### Set using end-relative index

<WasmPlayground :tcl="setUsingEndRelativeIndex" />

### Modify in loop

<WasmPlayground :tcl="modifyInLoop" />

### Returns new value

<WasmPlayground :tcl="returnsNewValue" />

## See Also

- [lindex](./lindex) - Get element by index
- [lreplace](./lreplace) - Replace range of elements
- [set](./set) - Set variable

