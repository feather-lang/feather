# for

C-style loop with initialization, test, and increment.

## Syntax

```tcl
for start test next command
```

## Parameters

- **start**: Initialization script, executed once before the loop
- **test**: Loop condition expression, evaluated with `expr` before each iteration
- **next**: Increment script, executed after each iteration
- **command**: Loop body to execute

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicCountingLoop = `for {set i 0} {$i < 5} {incr i} {
     puts {i = $i}
}`

const countingByTwos = `for {set i 0} {$i <= 10} {incr i 2} {
     puts $i
}`

const nestedLoops = `for {set row 1} {$row <= 3} {incr row} {
     set line {}
     for {set col 1} {$col <= 3} {incr col} {
         append line [expr {$row * $col}] { }
     }
     puts $line
}`

const usingBreakToExitEarly = `for {set i 0} {$i < 100} {incr i} {
     if {$i == 5} {
         puts {Stopping at 5}
         break
     }
     puts $i
}`
</script>

### Basic counting loop

<WasmPlayground :tcl="basicCountingLoop" />

### Counting by twos

<WasmPlayground :tcl="countingByTwos" />

### Nested loops

<WasmPlayground :tcl="nestedLoops" />

### Using break to exit early

<WasmPlayground :tcl="usingBreakToExitEarly" />

## See Also

- [foreach](./foreach) - Iterate over list elements
- [while](./while) - Conditional loop
- [break](./break) - Exit a loop
- [continue](./continue) - Skip to next iteration

