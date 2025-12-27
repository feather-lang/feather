# return

Return from a procedure with a value or special control flow.

## Syntax

```tcl
return ?options? ?value?
```

## Parameters

- **value**: The value to return. Defaults to empty string.

## Options

- **-code code**: Return code. Can be:
  - `ok` (0) - Normal return
  - `error` (1) - Raise an error
  - `return` (2) - Cause calling proc to return
  - `break` (3) - Break from loop in caller
  - `continue` (4) - Continue loop in caller
  - Or any integer
- **-level level**: Number of stack levels to return through
- **-options dict**: Dictionary of return options

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicReturn = `proc double {x} {
    return [expr {$x * 2}]
}

puts [double 21]`

const earlyReturn = `proc abs {x} {
    if {$x < 0} {
        return [expr {-$x}]
    }
    return $x
}

puts [abs -5]
puts [abs 3]`

const returnWithoutValue = `proc greet {name} {
    puts "Hello, $name!"
    return
}

set result [greet "World"]
puts "Result: '$result'"`

const returningFromNestedProcedures = `proc inner {} {
    return -level 2 "from inner"
}

proc outer {} {
    inner
    return "from outer"
}

puts [outer]`

const returnWithErrorCode = `proc divide {a b} {
    if {$b == 0} {
        return -code error "division by zero"
    }
    return [expr {$a / $b}]
}

puts [divide 10 2]`
</script>

### Basic return

<WasmPlayground :tcl="basicReturn" />

### Early return

<WasmPlayground :tcl="earlyReturn" />

### Return without value

<WasmPlayground :tcl="returnWithoutValue" />

### Returning from nested procedures

<WasmPlayground :tcl="returningFromNestedProcedures" />

### Return with error code

<WasmPlayground :tcl="returnWithErrorCode" />

## See Also

- [proc](./proc) - Define procedures
- [error](./error) - Raise errors
- [catch](./catch) - Catch errors

