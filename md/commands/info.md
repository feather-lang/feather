# info

Queries information about the interpreter state.

## Syntax

```tcl
info subcommand ?arg ...?
```

## Subcommands

### Variables

- **info exists varName** - Returns 1 if variable exists, 0 otherwise
- **info locals ?pattern?** - Returns names of local variables matching pattern
- **info globals ?pattern?** - Returns names of global variables matching pattern
- **info vars ?pattern?** - Returns names of all visible variables matching pattern

### Procedures

- **info commands ?pattern?** - Returns names of all commands matching pattern
- **info procs ?pattern?** - Returns names of procedures matching pattern
- **info body procname** - Returns the body of a procedure
- **info args procname** - Returns the parameter list of a procedure
- **info default procname arg varname** - Sets varname to default value if arg has one

### Call Stack

- **info level ?number?** - Returns current level or command at specified level
- **info frame ?number?** - Returns dictionary with frame information

### Other

- **info script** - Returns filename of currently executing script
- **info type value** - Returns the type name of a value

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const checkVariableExists = `# Check if variable exists
set x 42
puts "x exists: [info exists x]"
puts "y exists: [info exists y]"`

const listCommandsPattern = `# List commands matching pattern
puts "String commands:"
foreach cmd [info commands str*] {
    puts "  $cmd"
}`

const inspectProcedure = `# Inspect a procedure
proc greet {name {greeting "Hello"}} {
    puts "$greeting, $name!"
}

puts "Args: [info args greet]"
puts "Body: [info body greet]"`

const checkDefaultParameters = `# Check default parameter values
proc example {a {b 10} {c "default"}} {}

if {[info default example b val]} {
    puts "b default: $val"
}
if {[info default example c val]} {
    puts "c default: $val"
}`

const callStackLevel = `# Get call stack level
proc outer {} {
    puts "outer level: [info level]"
    inner
}
proc inner {} {
    puts "inner level: [info level]"
}
outer`

const listLocalVariables = `# List local variables
proc demo {x y} {
    set z [expr {$x + $y}]
    puts "Locals: [info locals]"
}
demo 1 2`

const getValueType = `# Get value type
puts "Type of 42: [info type 42]"
puts "Type of 3.14: [info type 3.14]"
puts "Type of hello: [info type hello]"
puts "Type of {a b c}: [info type {a b c}]"`
</script>

<FeatherPlayground :code="checkVariableExists" />

<FeatherPlayground :code="listCommandsPattern" />

<FeatherPlayground :code="inspectProcedure" />

<FeatherPlayground :code="checkDefaultParameters" />

<FeatherPlayground :code="callStackLevel" />

<FeatherPlayground :code="listLocalVariables" />

<FeatherPlayground :code="getValueType" />

## See Also

- [trace](./trace)
- [namespace](./namespace)
