# lrepeat

Creates a list by repeating a sequence of values a specified number of times.

## Syntax

```tcl
lrepeat count ?value ...?
```

## Parameters

- **count**: Number of times to repeat the values (must be non-negative)
- **value**: Zero or more values to repeat

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const repeatSingleValue = `puts [lrepeat 5 x]`

const repeatMultipleValues = `puts [lrepeat 3 a b]`

const createInitializedList = `set zeros [lrepeat 4 0]
puts $zeros`

const buildPatternSequence = `set pattern [lrepeat 2 red green blue]
puts $pattern`

const zeroRepetitions = `puts [lrepeat 0 anything]`
</script>

### Repeat single value

<WasmPlayground :tcl="repeatSingleValue" />

### Repeat multiple values

<WasmPlayground :tcl="repeatMultipleValues" />

### Create initialized list

<WasmPlayground :tcl="createInitializedList" />

### Build pattern sequence

<WasmPlayground :tcl="buildPatternSequence" />

### Zero repetitions

<WasmPlayground :tcl="zeroRepetitions" />

## See Also

- [list](./list) - Create a list
- [lappend](./lappend) - Append to list
- [concat](./concat) - Concatenate lists

