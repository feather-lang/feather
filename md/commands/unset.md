# unset

Delete one or more variables.

## Syntax

```tcl
unset ?-nocomplain? ?--? ?name ...?
```

## Parameters

- **-nocomplain**: Suppress errors if variable doesn't exist
- **--**: Marks end of options (use if variable name starts with dash)
- **name**: One or more variable names to delete

## Description

Removes the specified variables from the current scope. By default, raises an error if a variable doesn't exist. Use `-nocomplain` to silently ignore missing variables.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicUnset = `set x 10
set y 20
puts "Before: x=$x, y=$y"
unset x
puts "After unset x: y=$y"`

const usingNocomplain = `unset -nocomplain doesNotExist
puts {No error occurred}`

const unsettingMultiple = `set a 1
set b 2
set c 3
unset a b c
puts {All variables removed}`
</script>

### Basic unset

<FeatherPlayground :code="basicUnset" />

### Using -nocomplain

<FeatherPlayground :code="usingNocomplain" />

### Unsetting multiple variables

<FeatherPlayground :code="unsettingMultiple" />

## See Also

- [set](./set) - Get or set variable value
- [info exists](./info) - Check if variable exists
