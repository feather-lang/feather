# global

Access global variables from within a procedure.

## Syntax

```tcl
global ?varName ...?
```

## Parameters

- **varName**: One or more global variable names to link

## Description

Creates local variables that refer to global namespace variables of the same name. Only meaningful inside procedures—at the global level, it has no effect. Changes to the local variable affect the global variable.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const accessingGlobalVariable = `set count 0
proc incrementCount {} {
    global count
    incr count
}
incrementCount
incrementCount
puts {Global count: $count}`

const multipleGlobalVariables = `set total 0
set calls 0
proc addToTotal {n} {
    global total calls
    incr total $n
    incr calls
}
addToTotal 10
addToTotal 25
puts {Total: $total (calls: $calls)}`

const globalVsLocalScope = `set x {global value}
proc showX {} {
    global x
    puts {Inside proc: $x}
}
proc localX {} {
    set x {local value}
    puts {Local x: $x}
}
showX
localX
puts {Global x: $x}`
</script>

### Accessing a global variable

<WasmPlayground :tcl="accessingGlobalVariable" />

### Multiple global variables

<WasmPlayground :tcl="multipleGlobalVariables" />

### Global vs local scope

<WasmPlayground :tcl="globalVsLocalScope" />

## See Also

- [variable](./variable) - Declare namespace variables
- [upvar](./upvar) - Reference variables in other frames
- [namespace](./namespace) - Create and manipulate namespaces

