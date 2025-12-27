# foreach

Iterate over elements of one or more lists.

## Syntax

```tcl
foreach varList list ?varList2 list2 ...? command
```

## Parameters

- **varList**: Variable name or list of variable names to bind
- **list**: List of values to iterate over
- **varList2, list2**: Additional variable/list pairs for synchronized iteration
- **command**: Script to execute for each iteration

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const simpleIteration = `foreach fruit {apple banana cherry} {
    puts "I like $fruit"
}`

const multipleVariablesPerIteration = `foreach {name age} {Alice 30 Bob 25 Carol 35} {
    puts "$name is $age years old"
}`

const parallelListIteration = `foreach name {Alice Bob Carol} age {30 25 35} {
    puts "$name is $age"
}`

const buildingAResult = `set sum 0
foreach n {1 2 3 4 5} {
    set sum [expr {$sum + $n}]
}
puts "Sum: $sum"`

const usingContinueToSkipItems = `foreach n {1 2 3 4 5 6} {
    if {$n % 2 == 0} {
        continue
    }
    puts "Odd: $n"
}`
</script>

### Simple iteration

<WasmPlayground :tcl="simpleIteration" />

### Multiple variables per iteration

<WasmPlayground :tcl="multipleVariablesPerIteration" />

### Parallel list iteration

<WasmPlayground :tcl="parallelListIteration" />

### Building a result

<WasmPlayground :tcl="buildingAResult" />

### Using continue to skip items

<WasmPlayground :tcl="usingContinueToSkipItems" />

## See Also

- [for](./for) - C-style loop
- [while](./while) - Conditional loop
- [break](./break) - Exit a loop
- [continue](./continue) - Skip to next iteration

