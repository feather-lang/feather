# lset

Sets an element in a list variable at a specified index, with support for nested lists.

## Syntax

```tcl
lset varName newValue
lset varName index newValue
lset varName index ?index ...? newValue
```

## Parameters

- **varName**: Name of the list variable (not the value)
- **index**: Index of element to set. Supports:
  - Simple integers (e.g., `0`, `1`, `2`)
  - The `end` keyword for the last element
  - Index arithmetic with `+` and `-` (e.g., `end-1`, `end+0`, `0+1`)
  - Multiple indices for nested lists (e.g., `1 2` to access element 2 of the sublist at index 1)
  - Indices as a list (e.g., `{1 2}` is equivalent to `1 2`)
  - Empty index list `{}` to replace the entire variable
- **newValue**: New value for the element

Returns the modified list. The variable is modified in place.

## Special Behaviors

- When called with just `varName newValue` (no index), replaces the entire variable value
- When called with an empty index list `{}`, also replaces the entire variable value
- When the index equals the list length, the element is appended to the list
- When accessing a nested index on a scalar, the scalar is treated as a single-element list

## Error Conditions

- Error if the variable does not exist
- Error if the index is out of range (negative or greater than list length)
- Error if the index format is invalid

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

const nestedList = `set matrix {{a b c} {d e f} {g h i}}
lset matrix 1 2 X
puts $matrix`

const appendElement = `set list {a b c}
lset list 3 d
puts $list`

const replaceEntireVar = `set x {old list}
lset x {new list}
puts $x`

const indexArithmetic = `set items {a b c d e}
lset items 0+2 X
puts $items`
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

### Modify nested list

<WasmPlayground :tcl="nestedList" />

### Append element

When the index equals the list length, the element is appended:

<WasmPlayground :tcl="appendElement" />

### Replace entire variable

Using no index or an empty index list replaces the whole value:

<WasmPlayground :tcl="replaceEntireVar" />

### Index arithmetic

Indices support `+` and `-` arithmetic:

<WasmPlayground :tcl="indexArithmetic" />

## See Also

- [lindex](./lindex) - Get element by index
- [lreplace](./lreplace) - Replace range of elements
- [set](./set) - Set variable

