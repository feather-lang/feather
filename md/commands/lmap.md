# lmap

Maps a command over one or more lists, executing a body script for each iteration and collecting results into an accumulator list.

## Syntax

```tcl
lmap varList list ?varList list ...? command
```

## Parameters

- **varList**: Variable name(s) for iteration (must be non-empty)
- **list**: List to iterate over
- **command**: Body script executed for each iteration

## Supported Features

- **Basic single-variable iteration**: `lmap varname list body` - Iterates through a list, assigning each element to a variable
- **Multiple variables per list**: `lmap {a b} list body` - Assigns consecutive list elements to multiple variables per iteration
- **Multiple varlist/list pairs**: `lmap var1 list1 var2 list2 body` - Supports parallel iteration over multiple lists
- **Empty value padding**: When a list runs out of elements, remaining variables receive empty strings
- **break statement**: Exits the loop early, returning accumulated results so far
- **continue statement**: Skips appending the current body result to the accumulator and proceeds to the next iteration
- **Error propagation**: Errors in the body script are propagated appropriately
- **return propagation**: Return statements in the body are propagated

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

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

const earlyExit = `set numbers {1 2 3 4 5 6 7 8 9 10}
set result [lmap n $numbers {
    if {$n > 5} {break}
    expr {$n * 2}
}]
puts $result`
</script>

### Transform each element

<WasmPlayground :tcl="transformEachElement" />

### Filter and transform

<WasmPlayground :tcl="filterAndTransform" />

### Multiple variables per iteration

<WasmPlayground :tcl="multipleVariablesPerIteration" />

### Multiple lists

<WasmPlayground :tcl="multipleLists" />

### Early exit with break

<WasmPlayground :tcl="earlyExit" />

## Implementation Notes

- The iteration count is calculated as `ceiling(listLen / numVars)` and uses the maximum across all varlist/list pairs, ensuring all values from all lists are used.
- When `break` or `continue` is invoked, the body does not complete normally and the result is not appended to the accumulator list.
- Variables are set in the current scope following standard TCL semantics.

## See Also

- [foreach](./foreach) - Iterate without collecting results
- [list](./list) - Create a list
- [lindex](./lindex) - Get element by index

