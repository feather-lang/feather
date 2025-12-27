# continue

Skip the rest of the current loop iteration and continue with the next.

## Syntax

```tcl
continue
```

## Parameters

None. The `continue` command takes no arguments.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const skipEvenNumbers = `for {set i 1} {$i <= 10} {incr i} {
    if {$i % 2 == 0} {
        continue
    }
    puts {Odd: $i}
}`

const filterItemsInForeach = `foreach name {Alice Bob skip Carol skip Dave} {
    if {$name == {skip}} {
        continue
    }
    puts {Processing: $name}
}`

const skipInvalidData = `foreach value {5 -3 10 0 8 -1 4} {
    if {$value <= 0} {
        puts {Skipping $value}
        continue
    }
    puts {Valid: $value}
}`

const continueInWhileLoop = `set i 0
while {$i < 10} {
    incr i
    if {$i % 3 != 0} {
        continue
    }
    puts {$i is divisible by 3}
}`

const continueOnlyAffectsInnermostLoop = `foreach letter {A B C} {
    puts {Letter: $letter}
    for {set i 1} {$i <= 4} {incr i} {
        if {$i == 2} {
            continue
        }
        puts {  $letter$i}
    }
}`
</script>

### Skip even numbers

<WasmPlayground :tcl="skipEvenNumbers" />

### Filter items in foreach

<WasmPlayground :tcl="filterItemsInForeach" />

### Skip invalid data

<WasmPlayground :tcl="skipInvalidData" />

### Continue in while loop

<WasmPlayground :tcl="continueInWhileLoop" />

### Continue only affects innermost loop

<WasmPlayground :tcl="continueOnlyAffectsInnermostLoop" />

## See Also

- [break](./break) - Exit a loop
- [for](./for) - C-style loop
- [foreach](./foreach) - Iterate over list elements
- [while](./while) - Conditional loop

