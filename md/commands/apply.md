# apply

Apply an anonymous function (lambda expression).

## Syntax

```tcl
apply lambdaExpr ?arg ...?
```

## Parameters

- **lambdaExpr**: A list containing `{params body ?namespace?}`
  - **params**: Parameter list (same format as `proc`)
  - **body**: The script to execute
  - **namespace**: Optional namespace context for the lambda
- **arg ...**: Arguments to pass to the lambda

## Returns

The result of evaluating body with args bound to params.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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
</script>

### Basic lambda

<FeatherPlayground :code="basicLambda" />

### Lambda with multiple parameters

<FeatherPlayground :code="multiParams" />

### Inline lambda

<FeatherPlayground :code="inlineLambda" />

### Lambda with default parameters

<FeatherPlayground :code="defaultParams" />

### Using lambdas with higher-order functions

<FeatherPlayground :code="higherOrder" />

## See Also

- [proc](./proc) - Define named procedures
- [return](./return) - Return from procedure
