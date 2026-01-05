# linsert

Inserts elements into a list at a specified position.

## Syntax

```tcl
linsert list index ?value ...?
```

## Parameters

- **list**: The original list
- **index**: Position to insert at. Supports:
  - Simple integer indices (0-based)
  - `end` keyword to insert at the end
  - `end-N` syntax for end-relative positioning
  - Negative indices are clamped to 0 (insert at beginning)
  - Indices greater than list length are treated as `end`
- **value**: Values to insert (zero or more)

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

const endRelative = `set items {a b c d}
set result [linsert $items end-1 X]
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

### End-relative indexing

<WasmPlayground :tcl="endRelative" />

### Original list unchanged

<WasmPlayground :tcl="originalListUnchanged" />

## See Also

- [list](./list) - Create a list
- [lappend](./lappend) - Append to list variable
- [lindex](./lindex) - Get element by index

