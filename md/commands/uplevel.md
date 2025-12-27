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
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="myWhile" />

## See Also

- [upvar](./upvar)
- [proc](./proc)
