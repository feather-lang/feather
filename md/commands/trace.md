# trace

Traces variable access and command execution.

## Syntax

```tcl
trace subcommand ?arg ...?
```

## Subcommands

### trace add

Add a trace to a variable or command.

```tcl
trace add variable varName ops commandPrefix
trace add command cmdName ops commandPrefix
trace add execution cmdName ops commandPrefix
```

**Variable operations**: `read`, `write`, `unset`, `array`

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

List traces on a variable or command.

```tcl
trace info variable varName
trace info command cmdName
trace info execution cmdName
```

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="traceVariableWrites" />

<FeatherPlayground :code="traceVariableReads" />

<FeatherPlayground :code="traceCommandExecution" />

<FeatherPlayground :code="listActiveTraces" />

<FeatherPlayground :code="removeTrace" />

## See Also

- [info](./info)
- [namespace](./namespace)
