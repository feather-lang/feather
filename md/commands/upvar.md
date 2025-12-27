# upvar

Create references to variables in other stack frames.

## Syntax

```tcl
upvar ?level? otherVar localVar ?otherVar localVar ...?
```

## Parameters

- **level**: Stack level (default: 1). Use integer for relative levels or `#n` for absolute
- **otherVar**: Variable name in the target frame
- **localVar**: Local name to create as an alias

## Description

Creates local variables that are aliases to variables in another stack frame. Level `1` refers to the caller's frame, `2` to the caller's caller, and so on. Level `#0` refers to the global scope. Changes to the local alias affect the original variable.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const passByReference = `proc double {varName} {
    upvar 1 $varName var
    set var [expr {$var * 2}]
}

set x 21
double x
puts "x = $x"`

const swapTwoVariables = `proc swap {a b} {
    upvar 1 $a x $b y
    set temp $x
    set x $y
    set y $temp
}

set first 1
set second 2
swap first second
puts "first=$first, second=$second"`

const accessingGlobalScope = `set globalVar {I am global}

proc readGlobal {} {
    upvar #0 globalVar local
    puts $local
}

readGlobal`

const buildingDataStructures = `proc listAppend {listVar value} {
    upvar 1 $listVar lst
    lappend lst $value
}

set mylist {a b}
listAppend mylist c
listAppend mylist d
puts $mylist`
</script>

### Pass by reference

<FeatherPlayground :code="passByReference" />

### Swap two variables

<FeatherPlayground :code="swapTwoVariables" />

### Accessing global scope with #0

<FeatherPlayground :code="accessingGlobalScope" />

### Building data structures

<FeatherPlayground :code="buildingDataStructures" />

## See Also

- [global](./global) - Access global variables
- [variable](./variable) - Declare namespace variables
- [uplevel](./uplevel) - Execute script in another frame
