# llength

Returns the number of elements in a list.

## Syntax

```tcl
llength list
```

## Parameters

- **list**: The list to measure

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicUsage = `set items {a b c d e}
puts [llength $items]`

const emptyList = `set empty {}
puts [llength $empty]`

const listWithCompoundElements = `set data [list "hello world" {nested list} single]
puts [llength $data]`
</script>

### Basic usage

<WasmPlayground :tcl="basicUsage" />

### Empty list

<WasmPlayground :tcl="emptyList" />

### List with compound elements

<WasmPlayground :tcl="listWithCompoundElements" />

## See Also

- [list](./list) - Create a list
- [lindex](./lindex) - Get element by index
- [lappend](./lappend) - Append to list variable

