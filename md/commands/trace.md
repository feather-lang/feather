# trace

Traces variable access and command execution.

## Syntax

```tcl
trace subcommand ?arg ...?
```

## Subcommands

### trace add

Add a trace to a variable, command, or execution.

```tcl
trace add variable varName ops commandPrefix
trace add command cmdName ops commandPrefix
trace add execution cmdName ops commandPrefix
```

**Variable operations**: `read`, `write`, `unset`

Note: The `array` operation is not supported in Feather (no array support).

**Command operations**: `rename`, `delete`

**Execution operations**: `enter`, `leave`, `enterstep`, `leavestep`

### trace remove

Remove a previously added trace.

```tcl
trace remove variable varName ops commandPrefix
trace remove command cmdName ops commandPrefix
trace remove execution cmdName ops commandPrefix
```

### trace info

List traces on a variable, command, or execution.

```tcl
trace info variable varName
trace info command cmdName
trace info execution cmdName
```

## Callback Arguments

When a trace fires, Feather appends specific arguments to the command prefix:

### Variable Traces

```
commandPrefix name1 name2 op
```
- `name1`: Variable name being accessed
- `name2`: Array index (empty string for scalars)
- `op`: Operation (`read`, `write`, `unset`)

### Command Traces

```
commandPrefix oldName newName op
```
- `oldName`: Current command name (fully qualified)
- `newName`: New name (empty string for delete)
- `op`: Operation (`rename`, `delete`)

### Execution Traces (enter/enterstep)

```
commandPrefix command-string op
```
- `command-string`: Complete command with expanded arguments
- `op`: Operation (`enter`, `enterstep`)

### Execution Traces (leave/leavestep)

```
commandPrefix command-string code result op
```
- `command-string`: Complete command with expanded arguments
- `code`: Result code
- `result`: Result string
- `op`: Operation (`leave`, `leavestep`)

## Behavior Notes

- **Multiple traces**: Multiple traces can be registered on the same target
- **Trace ordering**: Variable traces fire in LIFO order (most recently added first)
- **Trace disabling**: Traces on a target are temporarily disabled while a callback runs
- **Error propagation**: Variable trace errors propagate as `can't read "varname": <error>` or `can't set "varname": <error>`; unset trace errors are silently ignored
- **Command existence**: `trace add command/execution` throws an error if the command does not exist
- **Namespace resolution**: Unqualified command/execution trace names are automatically prefixed with `::`
- **Trace removal on unset**: When a variable is unset, all traces on it are automatically removed
- **Step trace propagation**: `enterstep` and `leavestep` traces propagate through nested procedure calls

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const traceVariableWrites = `# Trace variable writes
proc on_write {name1 name2 op} {
    upvar $name1 var
    puts "Variable $name1 set to: $var"
}

trace add variable x write on_write
set x 10
set x 20`

const traceVariableReads = `# Trace variable reads
proc on_read {name1 name2 op} {
    puts "Reading variable $name1"
}

set y 100
trace add variable y read on_read
puts "Value: $y"
puts "Again: $y"`

const traceCommandExecution = `# Trace command execution
proc log_enter {cmd args} {
    puts "Entering: $cmd $args"
}

proc greet {name} {
    puts "Hello, $name!"
}

trace add execution greet enter log_enter
greet "World"`

const listActiveTraces = `# List active traces
proc my_trace {args} {}

set z 0
trace add variable z write my_trace
trace add variable z read my_trace

puts "Traces on z:"
foreach t [trace info variable z] {
    puts "  $t"
}`

const removeTrace = `# Remove a trace
proc counter {name1 name2 op} {
    upvar $name1 var
    puts "Count: $var"
}

trace add variable n write counter
set n 1
set n 2

trace remove variable n write counter
set n 3
puts "After removal: $n"`
</script>

<WasmPlayground :tcl="traceVariableWrites" />

<WasmPlayground :tcl="traceVariableReads" />

<WasmPlayground :tcl="traceCommandExecution" />

<WasmPlayground :tcl="listActiveTraces" />

<WasmPlayground :tcl="removeTrace" />

## See Also

- [info](./info)
- [namespace](./namespace)

