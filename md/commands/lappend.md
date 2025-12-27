# lappend

Appends values to a list variable.

## Syntax

```tcl
lappend varName ?value ...?
```

## Parameters

- **varName**: Name of the variable containing the list
- **value**: Optional values to append to the list

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="appendingToExistingList" />

### Creating a new list

<FeatherPlayground :code="creatingNewList" />

### Building a list in a loop

<FeatherPlayground :code="buildingListInLoop" />

## See Also

- [list](./list) - Create a list
- [linsert](./linsert) - Insert elements into list
- [llength](./llength) - Get list length
