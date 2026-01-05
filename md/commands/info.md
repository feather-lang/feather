# info

Queries information about the interpreter state.

## Syntax

```tcl
info subcommand ?arg ...?
```

## Subcommands

### Variables

- **info exists varName** - Returns 1 if variable exists, 0 otherwise. Handles qualified names (e.g., `::ns::var`).
- **info locals ?pattern?** - Returns names of local variables matching pattern. Excludes linked variables (global/upvar/variable).
- **info globals ?pattern?** - Returns names of global variables matching pattern.
- **info vars ?pattern?** - Returns names of all visible variables matching pattern. Namespace-aware pattern matching.

### Procedures

- **info commands ?pattern?** - Returns names of all commands matching pattern. Searches current namespace plus global namespace. Qualified patterns (containing `::`) search the specified namespace.
- **info procs ?pattern?** - Returns names of user-defined procedures matching pattern. Namespace-aware, excludes builtins.
- **info body procname** - Returns the body of a procedure.
- **info args procname** - Returns the parameter names of a procedure.
- **info default procname arg varname** - Sets varname to default value if arg has one. Returns 1 if default exists, 0 otherwise.

### Call Stack

- **info level ?number?** - Returns current level or command at specified level. Supports relative (negative) and absolute levels.
- **info frame ?number?** - Returns dictionary with frame information including: `type` (proc, source, or eval), `cmd`, `proc`, `level`, `file`, `namespace`, `line`, `lambda`.

### Other

- **info script** - Returns filename of currently executing script. Note: Unlike TCL, Feather only supports reading the script path, not setting it.
- **info type value** - Returns the type name of a value. Feather extension: returns "list", "dict", "int", "double", "string", or the registered type name for foreign objects.
- **info methods value** - Returns list of methods available on a foreign object. Feather extension for introspecting foreign object methods.

### Pattern Matching

All subcommands that accept a pattern use glob-style pattern matching (same as `string match`). For namespace-aware subcommands like `info commands`, `info procs`, and `info vars`, only the final component of a qualified pattern is treated as a pattern.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

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

const getMethodsJs = `// Register a Counter type with methods
feather.registerType(interp, 'Counter', {
  methods: {
    incr: (state) => { state.value++ },
    get: (state) => state.value,
  },
});

// Factory command to create counter instances
let id = 0;
register('counter', () => {
  const name = 'counter' + (++id);
  feather.createForeign(interp, 'Counter', { value: 0 }, name);
  return name;
});`

const getMethodsTcl = `set c [counter]
puts "Methods: [info methods $c]"
puts "Type: [info type $c]"

$c incr
$c incr
puts "Value: [$c get]"`
</script>

<FeatherPlayground :code="checkVariableExists" />

<FeatherPlayground :code="listCommandsPattern" />

<FeatherPlayground :code="inspectProcedure" />

<FeatherPlayground :code="checkDefaultParameters" />

<FeatherPlayground :code="callStackLevel" />

<FeatherPlayground :code="listLocalVariables" />

<FeatherPlayground :code="getValueType" />

<WasmPlayground :js="getMethodsJs" :tcl="getMethodsTcl" />

## See Also

- [trace](./trace)
- [namespace](./namespace)
