# apply

Apply an anonymous function (lambda expression).

## Syntax

```tcl
apply lambdaExpr ?arg ...?
```

## Parameters

- **lambdaExpr**: A list containing `{params body ?namespace?}`
  - **params**: Parameter list (same format as `proc`)
    - **Required parameters**: Simple names that must have values provided
    - **Optional parameters with defaults**: Parameters specified as `{name default}` pairs
    - **Variadic args**: The special `args` parameter as the last parameter collects remaining arguments into a list
  - **body**: The script to execute
  - **namespace**: Optional namespace context for the lambda (relative to global namespace)
- **arg ...**: Arguments to pass to the lambda

## Returns

The result of evaluating body with args bound to params.

## Parameter Behavior

Arguments with default values that are followed by non-defaulted arguments become required arguments. Enough actual arguments must be supplied to allow all arguments up to and including the last required formal argument.

```tcl
apply { {{x 1} y} {list $x $y} } 10       ;# Error: wrong # args
apply { {{x 1} y} {list $x $y} } 5 10     ;# Returns "5 10"
```

Note: The `args` parameter does NOT make preceding optionals required:

```tcl
apply { {{x 1} args} {list $x $args} }        ;# Returns "1 {}"
apply { {{x 1} args} {list $x $args} } 10     ;# Returns "10 {}"
```

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicLambda = `set double {{x} {expr {$x * 2}}}
puts [apply $double 5]
puts [apply $double 21]`

const multiParams = `set add {{a b} {expr {$a + $b}}}
puts [apply $add 3 4]

set greet {{name greeting} {return "$greeting, $name!"}}
puts [apply $greet "World" "Hello"]`

const inlineLambda = `puts [apply {{x y} {expr {$x * $y}}} 6 7]`

const defaultParams = `set power {{base {exp 2}} {
    set result 1
    for {set i 0} {$i < $exp} {incr i} {
        set result [expr {$result * $base}]
    }
    return $result
}}

puts "3^2 = [apply $power 3]"
puts "2^8 = [apply $power 2 8]"`

const higherOrder = `proc map {lambda list} {
    set result {}
    foreach item $list {
        lappend result [apply $lambda $item]
    }
    return $result
}

set numbers {1 2 3 4 5}
set squared [map {{x} {expr {$x * $x}}} $numbers]
puts "Squared: $squared"`

const variadicArgs = `# Lambda with variadic args parameter
set sum { {first args} {
    set total $first
    foreach n $args {
        set total [expr {$total + $n}]
    }
    return $total
} }

puts "Sum of 5: [apply $sum 5]"
puts "Sum of 1 2 3: [apply $sum 1 2 3]"
puts "Sum of 10 20 30 40: [apply $sum 10 20 30 40]"`
</script>

### Basic lambda

<WasmPlayground :tcl="basicLambda" />

### Lambda with multiple parameters

<WasmPlayground :tcl="multiParams" />

### Inline lambda

<WasmPlayground :tcl="inlineLambda" />

### Lambda with default parameters

<WasmPlayground :tcl="defaultParams" />

### Using lambdas with higher-order functions

<WasmPlayground :tcl="higherOrder" />

### Variadic args parameter

<WasmPlayground :tcl="variadicArgs" />

## See Also

- [proc](./proc) - Define named procedures
- [return](./return) - Return from procedure
- [global](./global) - Access global variables from within lambda
- [upvar](./upvar) - Access variables in calling frames
- [variable](./variable) - Access namespace variables
- [namespace](./namespace) - Namespace operations

