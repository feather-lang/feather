# list

Creates a list from the given arguments.

## Syntax

```tcl
list ?arg arg ...?
```

## Parameters

- **arg**: Optional values to include in the list

## Description

Returns a list comprised of all the arguments. When called with no arguments, returns an empty list.

The `list` command works directly from original arguments, preserving their structure. This is different from `concat`, which removes one level of grouping from its arguments. Braces and backslashes are added as necessary so that `lindex` can re-extract the original arguments unchanged.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const creatingASimpleList = `set colors [list red green blue]
puts $colors`

const emptyList = `set empty [list]
puts "Length: [llength $empty]"`

const listWithSpacesInElements = `set items [list "hello world" "foo bar"]
puts [lindex $items 0]`

const preservingStructure = `# list preserves argument structure
set nested [list a b "c d e" {f g}]
puts $nested
puts "Element 2: [lindex $nested 2]"
puts "Element 3: [lindex $nested 3]"`

const listVsConcat = `# Difference between list and concat
set a {x y}
set b {z}
puts "list result: [list $a $b]"
puts "concat result: [concat $a $b]"`
</script>

### Creating a simple list

<WasmPlayground :tcl="creatingASimpleList" />

### Empty list

<WasmPlayground :tcl="emptyList" />

### List with spaces in elements

<WasmPlayground :tcl="listWithSpacesInElements" />

### Preserving argument structure

<WasmPlayground :tcl="preservingStructure" />

### Difference from concat

<WasmPlayground :tcl="listVsConcat" />

## See Also

- [concat](./concat) - Concatenate arguments (removes one level of grouping)
- [lappend](./lappend) - Append to list variable
- [llength](./llength) - Get list length
- [lindex](./lindex) - Get element by index

