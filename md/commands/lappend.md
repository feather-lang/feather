# lappend

Appends values to a list variable.

## Syntax

```tcl
lappend varName ?value value value ...?
```

## Parameters

- **varName**: Name of the variable containing the list
- **value**: Optional values to append to the list

## Description

The `lappend` command appends values to a list stored in a variable. Each value is appended as a separate list element (not raw text, unlike `append`).

Key behaviors:

- If the variable does not exist, it is created as a new list containing the provided values
- Multiple values can be appended in a single call
- Qualified variable names are supported
- Variable traces (read and write) are fired appropriately
- The resulting list is returned as the command result

**Note**: Array element default values are not supported. If the variable indicates an element that does not exist of an array with a default value, the behavior may differ from standard Tcl.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const appendingToExistingList = `set fruits [list apple banana]
lappend fruits orange grape
puts $fruits`

const creatingNewList = `lappend newlist first second third
puts $newlist`

const buildingListInLoop = `set squares {}
foreach n {1 2 3 4 5} {
    lappend squares [expr {$n * $n}]
}
puts $squares`
</script>

### Appending to an existing list

<WasmPlayground :tcl="appendingToExistingList" />

### Creating a new list

<WasmPlayground :tcl="creatingNewList" />

### Building a list in a loop

<WasmPlayground :tcl="buildingListInLoop" />

## See Also

- [list](./list) - Create a list
- [linsert](./linsert) - Insert elements into list
- [llength](./llength) - Get list length

