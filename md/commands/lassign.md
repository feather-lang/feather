# lassign

Assigns list elements to variables.

## Syntax

```tcl
lassign list varName ?varName ...?
```

## Parameters

- **list**: The list to assign from
- **varName**: Names of variables to receive list elements

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicAssignment = `set coords {10 20 30}
lassign $coords x y z
puts "x=$x y=$y z=$z"`

const fewerVariables = `set data {a b c d e}
set rest [lassign $data first second]
puts "first=$first second=$second"
puts "rest=$rest"`

const moreVariables = `set pair {hello world}
lassign $pair a b c
puts "a=$a b=$b c=$c"`
</script>

### Basic assignment

<FeatherPlayground :code="basicAssignment" />

### Fewer variables than elements

<FeatherPlayground :code="fewerVariables" />

### More variables than elements

<FeatherPlayground :code="moreVariables" />

## See Also

- [list](./list) - Create a list
- [lindex](./lindex) - Get element by index
- [foreach](./foreach) - Iterate over list
