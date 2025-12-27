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
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicUsage = `set items {a b c d e}
puts [llength $items]`

const emptyList = `set empty {}
puts [llength $empty]`

const listWithCompoundElements = `set data [list "hello world" {nested list} single]
puts [llength $data]`
</script>

### Basic usage

<FeatherPlayground :code="basicUsage" />

### Empty list

<FeatherPlayground :code="emptyList" />

### List with compound elements

<FeatherPlayground :code="listWithCompoundElements" />

## See Also

- [list](./list) - Create a list
- [lindex](./lindex) - Get element by index
- [lappend](./lappend) - Append to list variable
