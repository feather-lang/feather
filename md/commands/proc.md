# proc

Define a procedure (function).

## Syntax

```tcl
proc name args body
```

## Parameters

- **name**: The procedure name. Can be namespace-qualified (e.g., `::myns::myproc`).
- **args**: Parameter list. Supports default values with `{param default}` and variadic arguments with `args`.
- **body**: The script to execute when the procedure is called.

## Parameter Types

### Simple parameters

```tcl
proc greet {name} { ... }
```

### Parameters with defaults

```tcl
proc greet {name {greeting "Hello"}} { ... }
```

### Variadic parameters

```tcl
proc log {level args} { ... }
```

The special parameter `args` captures all remaining arguments as a list.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicProcedure = `proc greet {name} {
    return "Hello, $name!"
}

puts [greet "World"]
puts [greet "Feather"]`

const defaultParameterValues = `proc greet {name {greeting "Hello"}} {
    return "$greeting, $name!"
}

puts [greet "Alice"]
puts [greet "Bob" "Hi"]`

const variadicArguments = `proc sum {args} {
    set total 0
    foreach n $args {
        set total [expr {$total + $n}]
    }
    return $total
}

puts "Sum: [sum 1 2 3 4 5]"`

const recursiveProcedure = `proc factorial {n} {
    if {$n <= 1} {
        return 1
    }
    return [expr {$n * [factorial [expr {$n - 1}]]}]
}

puts "5! = [factorial 5]"`
</script>

### Basic procedure

<FeatherPlayground :code="basicProcedure" />

### Default parameter values

<FeatherPlayground :code="defaultParameterValues" />

### Variadic arguments

<FeatherPlayground :code="variadicArguments" />

### Recursive procedure

<FeatherPlayground :code="recursiveProcedure" />

## See Also

- [apply](./apply) - Apply anonymous functions
- [return](./return) - Return from procedure
- [tailcall](./tailcall) - Tail call optimization
