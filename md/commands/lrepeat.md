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
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const repeatSingleValue = `puts [lrepeat 5 x]`

const repeatMultipleValues = `puts [lrepeat 3 a b]`

const createInitializedList = `set zeros [lrepeat 4 0]
puts $zeros`

const buildPatternSequence = `set pattern [lrepeat 2 red green blue]
puts $pattern`

const zeroRepetitions = `puts [lrepeat 0 anything]`
</script>

### Repeat single value

<FeatherPlayground :code="repeatSingleValue" />

### Repeat multiple values

<FeatherPlayground :code="repeatMultipleValues" />

### Create initialized list

<FeatherPlayground :code="createInitializedList" />

### Build pattern sequence

<FeatherPlayground :code="buildPatternSequence" />

### Zero repetitions

<FeatherPlayground :code="zeroRepetitions" />

## See Also

- [list](./list) - Create a list
- [lappend](./lappend) - Append to list
- [concat](./concat) - Concatenate lists
