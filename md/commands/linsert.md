# linsert

Inserts elements into a list at a specified position.

## Syntax

```tcl
linsert list index ?value ...?
```

## Parameters

- **list**: The original list
- **index**: Position to insert at (0-based)
- **value**: Values to insert

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const insertAtBeginning = `set nums {2 3 4}
set result [linsert $nums 0 1]
puts $result`

const insertInMiddle = `set letters {a b e f}
set result [linsert $letters 2 c d]
puts $result`

const insertAtEnd = `set items {x y}
set result [linsert $items end z]
puts $result`

const originalListUnchanged = `set original {1 2 3}
set modified [linsert $original 1 new]
puts "Original: $original"
puts "Modified: $modified"`
</script>

### Insert at beginning

<WasmPlayground :tcl="insertAtBeginning" />

### Insert in middle

<WasmPlayground :tcl="insertInMiddle" />

### Insert at end

<WasmPlayground :tcl="insertAtEnd" />

### Original list unchanged

<WasmPlayground :tcl="originalListUnchanged" />

## See Also

- [list](./list) - Create a list
- [lappend](./lappend) - Append to list variable
- [lindex](./lindex) - Get element by index

