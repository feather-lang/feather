# list

Creates a list from the given arguments.

## Syntax

```tcl
list ?element ...?
```

## Parameters

- **element**: Optional values to include in the list

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const creatingASimpleList = `set colors [list red green blue]
puts $colors`

const emptyList = `set empty [list]
puts "Length: [llength $empty]"`

const listWithSpacesInElements = `set items [list "hello world" "foo bar"]
puts [lindex $items 0]`
</script>

### Creating a simple list

<FeatherPlayground :code="creatingASimpleList" />

### Empty list

<FeatherPlayground :code="emptyList" />

### List with spaces in elements

<FeatherPlayground :code="listWithSpacesInElements" />

## See Also

- [lappend](./lappend) - Append to list variable
- [llength](./llength) - Get list length
- [lindex](./lindex) - Get element by index
