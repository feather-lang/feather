# variable

Declare and initialize namespace variables.

## Syntax

```tcl
variable ?name value ...? name ?value?
```

## Parameters

- **name**: Variable name (simple or namespace-qualified)
- **value**: Optional initial value for the preceding name

## Description

Creates a variable in the current namespace and links a local reference to it. If called inside a procedure within a namespace, creates both the namespace variable and a local alias.

When `name` is a simple name (not qualified with `::`), the variable is created in the current namespace. When `name` is a fully-qualified name (e.g., `::other::varname`), `variable` creates a local link to that variable in the specified namespace, enabling cross-namespace access. The parent namespace must already exist; otherwise an error is raised.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const declaringNamespaceVariables = `namespace eval counter {
    variable count 0
    
    proc increment {} {
        variable count
        incr count
    }
    
    proc get {} {
        variable count
        return $count
    }
}

counter::increment
counter::increment
puts "Count: [counter::get]"`

const multipleVariablesAtOnce = `namespace eval config {
    variable host localhost port 8080 timeout 30
}

puts "Server: $config::host:$config::port"
puts "Timeout: $config::timeout"`

const variableWithoutInitialValue = `namespace eval app {
    variable name

    proc init {n} {
        variable name
        set name $n
    }

    proc greet {} {
        variable name
        puts "Hello, $name!"
    }
}

app::init {Feather User}
app::greet`

const crossNamespaceAccess = `namespace eval target {
    variable data "shared data"
}

namespace eval other {
    proc access {} {
        variable ::target::data
        puts "Accessed: $data"
    }

    proc modify {} {
        variable ::target::data
        set data "modified by other"
    }
}

other::access
other::modify
puts "After modify: $target::data"`
</script>

### Declaring namespace variables

<WasmPlayground :tcl="declaringNamespaceVariables" />

### Multiple variables at once

<WasmPlayground :tcl="multipleVariablesAtOnce" />

### Variable without initial value

<WasmPlayground :tcl="variableWithoutInitialValue" />

### Cross-namespace access with qualified names

<WasmPlayground :tcl="crossNamespaceAccess" />

## See Also

- [global](./global) - Access global variables
- [namespace](./namespace) - Create and manipulate namespaces
- [upvar](./upvar) - Reference variables in other frames

