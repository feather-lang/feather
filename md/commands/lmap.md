# lmap

Maps an expression over a list, returning a list of results.

## Syntax

```tcl
lmap varList list ?varList list ...? expression
```

## Parameters

- **varList**: Variable name(s) for iteration
- **list**: List to iterate over
- **expression**: Expression evaluated for each iteration

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const transformEachElement = `set numbers {1 2 3 4 5}
set squares [lmap n $numbers {expr {$n * $n}}]
puts $squares`

const filterAndTransform = `set values {1 2 3 4 5 6}
set evens [lmap n $values {
    if {$n % 2 == 0} {set n} else {continue}
}]
puts $evens`

const multipleVariablesPerIteration = `set pairs {a 1 b 2 c 3}
set result [lmap {key val} $pairs {
    list $key $val
}]
puts $result`

const multipleLists = `set names {Alice Bob Carol}
set ages {25 30 35}
set result [lmap name $names age $ages {
    list $name $age
}]
puts $result`
</script>

### Transform each element

<FeatherPlayground :code="transformEachElement" />

### Filter and transform

<FeatherPlayground :code="filterAndTransform" />

### Multiple variables per iteration

<FeatherPlayground :code="multipleVariablesPerIteration" />

### Multiple lists

<FeatherPlayground :code="multipleLists" />

## See Also

- [foreach](./foreach) - Iterate without collecting results
- [list](./list) - Create a list
- [lindex](./lindex) - Get element by index
