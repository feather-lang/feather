# set

Get or set the value of a variable.

## Syntax

```tcl
set varName ?newValue?
```

## Parameters

- **varName**: Name of the variable, optionally namespace-qualified
- **newValue**: Optional new value to assign

## Description

If `newValue` is provided, assigns it to `varName` and returns the value. If omitted, returns the current value of `varName`. The variable must exist when reading.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const settingAndGettingVariable = `set greeting {Hello, World!}
puts [set greeting]`

const usingSetToReadVariable = `set x 42
set y [set x]
puts "x = $x, y = $y"`

const namespaceQualifiedNames = `namespace eval myns {
    set myns::counter 0
}
puts $myns::counter`
</script>

### Setting and getting a variable

<WasmPlayground :tcl="settingAndGettingVariable" />

### Using set to read a variable

<WasmPlayground :tcl="usingSetToReadVariable" />

### Namespace-qualified names

<WasmPlayground :tcl="namespaceQualifiedNames" />

## See Also

- [unset](./unset) - Delete variables
- [append](./append) - Append to variable
- [incr](./incr) - Increment integer variable
- [global](./global) - Access global variables

