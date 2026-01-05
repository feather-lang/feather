# proc

Define a procedure (function).

## Syntax

```tcl
proc name args body
```

## Parameters

- **name**: The procedure name. Can be namespace-qualified (e.g., `::myns::myproc`). Intermediate namespaces are created automatically if they don't exist.
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

### Mixed required and optional arguments

```tcl
proc example {required1 required2 {optional1 default1} {optional2 default2}} {
    # ...
}
```

When optional parameters are followed by required ones, the optional parameters become effectively required.

## Return Values

- The return value is the value from an explicit `return` command
- If no explicit `return`, the return value is the result of the last command executed
- Errors in the procedure body propagate to the caller
- The `return` command with `-code` and `-level` options is supported for non-local returns

## Local Variables and Namespace Context

- Each procedure invocation creates a new call frame with its own local variables
- When a procedure is invoked, the current namespace is set to the namespace where the procedure was defined

## Replacing Existing Commands

Defining a proc with the same name as an existing command (user-defined or builtin) replaces it.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

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

const namespaceProcedure = `proc ::math::double {x} {
    return [expr {$x * 2}]
}

puts [::math::double 21]`

const optionalWithArgs = `proc vardefault {required {opt "default"} args} {
    list $required $opt $args
}

puts [vardefault R]
puts [vardefault R O]
puts [vardefault R O A B]`

const replacingCommands = `proc foo {} { return "original" }
puts [foo]

proc foo {} { return "replaced" }
puts [foo]`
</script>

### Basic procedure

<WasmPlayground :tcl="basicProcedure" />

### Default parameter values

<WasmPlayground :tcl="defaultParameterValues" />

### Variadic arguments

<WasmPlayground :tcl="variadicArguments" />

### Recursive procedure

<WasmPlayground :tcl="recursiveProcedure" />

### Namespace-qualified procedure

<WasmPlayground :tcl="namespaceProcedure" />

### Optional parameters with args

<WasmPlayground :tcl="optionalWithArgs" />

### Replacing existing commands

<WasmPlayground :tcl="replacingCommands" />

## See Also

- [apply](./apply) - Apply anonymous functions
- [return](./return) - Return from procedure
- [tailcall](./tailcall) - Tail call optimization
- [namespace](./namespace) - Namespace management

