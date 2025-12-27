# while

Execute a command repeatedly while a condition is true.

## Syntax

```tcl
while test command
```

## Parameters

- **test**: Boolean expression evaluated with `expr` before each iteration
- **command**: Script to execute while test is true

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicCountdown = `set n 5
while {$n > 0} {
    puts $n
    incr n -1
}
puts Liftoff!`

const readingUntilCondition = `set total 0
set i 1
while {$total < 20} {
    set total [expr {$total + $i}]
    puts {Added $i, total is $total}
    incr i
}`

const usingBreakForComplexExit = `set i 0
while {1} {
    incr i
    if {$i > 10} {
        puts {Limit reached}
        break
    }
    if {$i % 3 != 0} {
        continue
    }
    puts {$i is divisible by 3}
}`

const fibonacciSequence = `set a 0
set b 1
while {$a < 100} {
    puts $a
    set temp $b
    set b [expr {$a + $b}]
    set a $temp
}`
</script>

### Basic countdown

<FeatherPlayground :code="basicCountdown" />

### Reading until condition

<FeatherPlayground :code="readingUntilCondition" />

### Using break for complex exit

<FeatherPlayground :code="usingBreakForComplexExit" />

### Fibonacci sequence

<FeatherPlayground :code="fibonacciSequence" />

## See Also

- [for](./for) - C-style loop
- [foreach](./foreach) - Iterate over list elements
- [break](./break) - Exit a loop
- [continue](./continue) - Skip to next iteration
