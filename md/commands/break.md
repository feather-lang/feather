# break

Exit the innermost enclosing loop immediately.

## Syntax

```tcl
break
```

## Parameters

None. The `break` command takes no arguments.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const exitForLoopEarly = `for {set i 0} {$i < 10} {incr i} {
    if {$i == 5} {
        puts {Breaking at $i}
        break
    }
    puts $i
}
puts Done`

const searchAndStop = `set target 7
set found 0
foreach n {3 5 7 9 11} {
    if {$n == $target} {
        set found 1
        puts {Found target!}
        break
    }
    puts {Checked $n}
}
if {!$found} {
    puts {Not found}
}`

const breakingFromWhileLoop = `set i 0
while {1} {
    incr i
    if {$i > 5} {
        break
    }
    puts $i
}
puts {Exited loop}`

const breakOnlyExitsInnermostLoop = `for {set i 1} {$i <= 3} {incr i} {
    puts {Outer: $i}
    for {set j 1} {$j <= 5} {incr j} {
        if {$j == 3} {
            break
        }
        puts {  Inner: $j}
    }
}`
</script>

### Exit a for loop early

<FeatherPlayground :code="exitForLoopEarly" />

### Search and stop

<FeatherPlayground :code="searchAndStop" />

### Breaking from while loop

<FeatherPlayground :code="breakingFromWhileLoop" />

### Break only exits innermost loop

<FeatherPlayground :code="breakOnlyExitsInnermostLoop" />

## See Also

- [continue](./continue) - Skip to next iteration
- [for](./for) - C-style loop
- [foreach](./foreach) - Iterate over list elements
- [while](./while) - Conditional loop
