# variable

Declare and initialize namespace variables.

## Syntax

```tcl
variable ?name value ...? name ?value?
```

## Parameters

- **name**: Variable name (must not be namespace-qualified)
- **value**: Optional initial value for the preceding name

## Description

Creates a variable in the current namespace and links a local reference to it. If called inside a procedure within a namespace, creates both the namespace variable and a local alias. The name must be simple (not qualified with `::`) since the variable is always created in the current namespace.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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
</script>

### Declaring namespace variables

<FeatherPlayground :code="declaringNamespaceVariables" />

### Multiple variables at once

<FeatherPlayground :code="multipleVariablesAtOnce" />

### Variable without initial value

<FeatherPlayground :code="variableWithoutInitialValue" />

## See Also

- [global](./global) - Access global variables
- [namespace](./namespace) - Create and manipulate namespaces
- [upvar](./upvar) - Reference variables in other frames
