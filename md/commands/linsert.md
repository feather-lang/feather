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
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="insertAtBeginning" />

### Insert in middle

<FeatherPlayground :code="insertInMiddle" />

### Insert at end

<FeatherPlayground :code="insertAtEnd" />

### Original list unchanged

<FeatherPlayground :code="originalListUnchanged" />

## See Also

- [list](./list) - Create a list
- [lappend](./lappend) - Append to list variable
- [lindex](./lindex) - Get element by index
