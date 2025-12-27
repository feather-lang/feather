# incr

Increment an integer variable.

## Syntax

```tcl
incr varName ?increment?
```

## Parameters

- **varName**: Name of the variable containing an integer
- **increment**: Amount to add (default: 1)

## Description

Adds `increment` to the integer value stored in `varName` and stores the result back in the variable. The variable must exist and contain a valid integer. Returns the new value.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicIncrement = `set counter 0
incr counter
incr counter
incr counter
puts "Counter: $counter"`

const customIncrementValue = `set value 10
incr value 5
puts "After +5: $value"
incr value -3
puts "After -3: $value"`

const loopCounter = `set sum 0
set i 1
while {$i <= 5} {
    incr sum $i
    incr i
}
puts "Sum of 1-5: $sum"`
</script>

### Basic increment

<WasmPlayground :tcl="basicIncrement" />

### Custom increment value

<WasmPlayground :tcl="customIncrementValue" />

### Loop counter

<WasmPlayground :tcl="loopCounter" />

## See Also

- [set](./set) - Get or set variable value
- [expr](./expr) - Evaluate mathematical expressions

