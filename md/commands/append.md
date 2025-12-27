# append

Append values to a variable.

## Syntax

```tcl
append varName ?value ...?
```

## Parameters

- **varName**: Name of the variable to append to
- **value**: One or more values to concatenate

## Description

Concatenates all `value` arguments to the end of the variable `varName`. If the variable doesn't exist, it is created as an empty string before appending. Returns the new value.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const buildingAString = `set msg {Hello}
append msg {, } {World} {!}
puts $msg`

const creatingVariableViaAppend = `append newVar {first}
append newVar { second}
puts $newVar`

const buildingOutputInALoop = `set result {}
foreach n {1 2 3 4 5} {
    append result $n { }
}
puts $result`
</script>

### Building a string

<FeatherPlayground :code="buildingAString" />

### Creating variable via append

<FeatherPlayground :code="creatingVariableViaAppend" />

### Building output in a loop

<FeatherPlayground :code="buildingOutputInALoop" />

## See Also

- [set](./set) - Get or set variable value
- [concat](./concat) - Concatenate lists
- [string cat](./string) - Concatenate strings
