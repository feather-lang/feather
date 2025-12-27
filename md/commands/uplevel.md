# uplevel

Execute a script in the context of a caller's stack frame.

## Syntax

```tcl
uplevel ?level? command ?arg ...?
```

## Parameters

- **level**: Stack level to execute in (default: 1). Can be a number or `#N` for absolute frame
- **command**: The command to execute
- **arg**: Additional arguments concatenated with command

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const myWhile = `proc myWhile {condition body} {
    while {[uplevel 1 expr $condition]} {
        uplevel 1 $body
    }
}

set x 0
myWhile {$x < 3} {
    puts "x = $x"
    incr x
}`
</script>

<WasmPlayground :tcl="myWhile" />

## See Also

- [upvar](./upvar)
- [proc](./proc)

