# join

Join list elements into a string.

## Syntax

```tcl
join list ?joinString?
```

## Parameters

- **list**: A list of elements to join
- **joinString**: The separator to place between elements (default: space)

## Description

Concatenates the elements of a list into a single string, placing the join string between each element. If no join string is specified, a single space is used.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const defaultSeparator = `puts [join {a b c d}]`

const customSeparator = `puts [join {a b c} ", "]
puts [join {1 2 3 4} "-"]`

const noSeparator = `puts [join {H e l l o} ""]`

const newlineSeparator = `puts [join {line1 line2 line3} "\n"]`

const buildingPaths = `puts [join {usr local bin} "/"]`
</script>

### Default separator (space)

<WasmPlayground :tcl="defaultSeparator" />

### Custom separator

<WasmPlayground :tcl="customSeparator" />

### No separator

<WasmPlayground :tcl="noSeparator" />

### Newline separator

<WasmPlayground :tcl="newlineSeparator" />

### Building paths

<WasmPlayground :tcl="buildingPaths" />

## See Also

- [split](./split) - Split string into list
- [concat](./concat) - Concatenate with spaces
- [list](./list) - Create a list

