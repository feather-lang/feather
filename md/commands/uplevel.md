# uplevel

Execute a script in the context of a caller's stack frame.

## Syntax

```tcl
uplevel ?level? command ?arg ...?
```

## Parameters

- **level**: Stack level to execute in (default: 1). Can be:
  - A relative level `N` - moves N levels up the call stack
  - An absolute level `#N` - targets absolute stack frame number
- **command**: The command to execute
- **arg**: Additional arguments concatenated with command (joined with spaces)

## Description

The `uplevel` command evaluates a script in a different stack frame context:

1. Parses an optional level specifier (relative `N` or absolute `#N`)
2. Defaults to level `1` (caller's frame) when level is omitted
3. Concatenates multiple arguments with spaces to form the script
4. Temporarily switches the active frame to the target level
5. Evaluates the script in that frame's variable context
6. Returns the result of the script evaluation

## Supported Features

| Feature | Notes |
|---------|-------|
| Relative level (`N`) | Moves N levels up the call stack |
| Absolute level (`#N`) | Targets absolute stack frame number |
| Default level of `1` | Used when level is omitted |
| Multiple argument concatenation | Arguments are joined with spaces |
| Variable context switching | Script executes with target frame's variables |
| Result propagation | Returns result of evaluated script |
| Error propagation | Errors from script evaluation are propagated |
| Namespace interaction | Namespace eval and procs add call frames correctly |
| Apply command interaction | Apply adds call frames that count for uplevel |

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
- [apply](./apply)
- [namespace](./namespace)

