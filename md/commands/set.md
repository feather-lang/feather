# set

Read and write variables.

## Syntax

```tcl
set varName ?newValue?
```

## Parameters

- **varName**: Name of the variable, optionally namespace-qualified (e.g., `::foo::bar`)
- **newValue**: Optional new value to assign

## Description

The `set` command reads and writes variables:

- With one argument: returns the current value of the variable
- With two arguments: sets the variable to the new value and returns it
- Creates a new variable if one does not already exist

### Supported Features

- **Namespace-qualified variable names**: Variables can be accessed using namespace qualifiers like `::foo::bar`. The resolution follows standard TCL namespace rules.
- **Frame-local variables**: For unqualified names, frame-local (procedure-local) storage is used.
- **Variable traces**: Both read and write traces are fired appropriately.
- **Variable creation**: Setting a non-existent variable creates it automatically.

### Error Handling

- Wrong number of arguments: `wrong # args: should be "set varName ?newValue?"`
- Reading non-existent variable: `can't read "varName": no such variable`

### Limitations

- **Array element syntax**: TCL's standard `set arr(index) value` syntax for arrays is not supported. A variable name like `arr(foo)` is treated as a literal scalar variable name rather than accessing element `foo` of array `arr`.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const settingAndGettingVariable = `set greeting {Hello, World!}
puts [set greeting]`

const usingSetToReadVariable = `set x 42
set y [set x]
puts "x = $x, y = $y"`

const namespaceQualifiedNames = `namespace eval myns {
    set myns::counter 0
}
puts $myns::counter`

const variableCreation = `# Setting a non-existent variable creates it
set newVar {I was just created}
puts $newVar`
</script>

### Setting and getting a variable

<WasmPlayground :tcl="settingAndGettingVariable" />

### Using set to read a variable

<WasmPlayground :tcl="usingSetToReadVariable" />

### Namespace-qualified names

<WasmPlayground :tcl="namespaceQualifiedNames" />

### Variable creation

<WasmPlayground :tcl="variableCreation" />

## See Also

- [unset](./unset) - Delete variables
- [append](./append) - Append to variable
- [incr](./incr) - Increment integer variable
- [global](./global) - Access global variables
- [variable](./variable) - Declare namespace variables
- [upvar](./upvar) - Create variable links

