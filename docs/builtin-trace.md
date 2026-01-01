# Feather `trace` Builtin Documentation

This document compares Feather's implementation of the `trace` command with the standard TCL implementation.

## Summary of Our Implementation

Feather implements the `trace` command with three subcommands:

- `trace add <type> <name> <ops> <command>` - Add a trace to a variable, command, or execution
- `trace remove <type> <name> <ops> <command>` - Remove an existing trace
- `trace info <type> <name>` - List traces on a variable, command, or execution

The implementation supports three trace types:
- `variable` - Traces on variable access
- `command` - Traces on command modification (rename/delete)
- `execution` - Traces on command execution

Traces are stored internally in dictionaries keyed by name, with each entry containing a list of `{ops script}` pairs.

## TCL Features We Support

### Subcommands

| Subcommand | Status |
|------------|--------|
| `trace add` | Supported |
| `trace remove` | Supported |
| `trace info` | Supported |

### Trace Types

| Type | Status |
|------|--------|
| `variable` | Supported (registration only) |
| `command` | Supported (registration only) |
| `execution` | Supported (registration only) |

### Basic Functionality

- **Trace registration**: Adding traces with `trace add` works correctly
- **Trace removal**: Removing traces with `trace remove` works correctly
- **Trace listing**: Querying traces with `trace info` works correctly
- **Namespace qualification**: Command and execution traces automatically qualify unqualified names with `::`
- **Multiple traces**: Multiple traces can be registered on the same target
- **Operations list**: Operations are stored as space-separated strings and returned as lists

## TCL Feature Support Details

### Variable Trace Operations

| Operation | TCL Behavior | Feather Status |
|-----------|-------------|----------------|
| `read` | Invoke callback when variable is read | **Implemented** |
| `write` | Invoke callback when variable is written | **Implemented** |
| `unset` | Invoke callback when variable is unset | **Implemented** |
| `array` | Invoke callback when variable accessed via `array` command | Not supported (no arrays) |

### Command Trace Operations

| Operation | TCL Behavior | Feather Status |
|-----------|-------------|----------------|
| `rename` | Invoke callback when command is renamed | **Implemented** |
| `delete` | Invoke callback when command is deleted | **Implemented** |

### Execution Trace Operations

| Operation | TCL Behavior | Feather Status |
|-----------|-------------|----------------|
| `enter` | Invoke callback before command executes | **Implemented** |
| `leave` | Invoke callback after command executes | **Implemented** |
| `enterstep` | Invoke callback before each command in a procedure | Not implemented |
| `leavestep` | Invoke callback after each command in a procedure | Not implemented |

### Callback Invocation

TCL appends specific arguments to the command prefix when invoking trace callbacks:

**Variable traces:**
```
commandPrefix name1 name2 op
```
- `name1`: Variable name being accessed
- `name2`: Array index (or empty string for scalars)
- `op`: Operation (`read`, `write`, `unset`)

**Command traces:**
```
commandPrefix oldName newName op
```
- `oldName`: Current command name (fully qualified)
- `newName`: New name (empty string for delete)
- `op`: Operation (`rename`, `delete`)

**Execution traces (enter/enterstep):**
```
commandPrefix command-string op
```
- `command-string`: Complete command with expanded arguments
- `op`: Operation (`enter`, `enterstep`)

**Execution traces (leave/leavestep):**
```
commandPrefix command-string code result op
```
- `command-string`: Complete command with expanded arguments
- `code`: Result code
- `result`: Result string
- `op`: Operation (`leave`, `leavestep`)

**Feather status:** Variable, command, and execution trace callbacks are implemented and fired with the correct arguments. The `array`, `enterstep`, and `leavestep` operations are not implemented.

### Advanced Features

| Feature | TCL Behavior | Feather Status |
|---------|-------------|----------------|
| Command existence check | `trace add command/execution` throws error if command does not exist | **Implemented** |
| Operation validation | Operations are validated for each trace type | **Implemented** |
| Variable creation | `trace add variable` creates variable if it does not exist (undefined but visible) | Not implemented |
| Trace disabling during callback | Traces on target are temporarily disabled while callback runs | **Implemented** (via `trace_firing` flag) |
| Error propagation | Errors in trace callbacks propagate to traced operation | Not implemented |
| Variable modification in callbacks | Read/write trace callbacks can modify variable value | Not implemented |
| Multiple trace ordering | Variable traces: most-recent first; execution traces: enter/enterstep reverse order, leave/leavestep original order | **Implemented** |
| Array element traces | Traces on individual array elements | Not supported (no arrays) |
| Array-wide traces | Traces on entire arrays that fire for any element access | Not supported (no arrays) |
| Trace removal on unset | Variable traces removed when variable is unset | Not implemented |
| upvar interaction | Traces fire with actual variable name, may differ from traced name | Not implemented |

## Notes on Implementation Differences

### Trace Firing

Feather implements trace firing for:
- **Variable traces**: `read`, `write`, `unset` operations fire the registered callbacks
- **Command traces**: `rename`, `delete` operations fire the registered callbacks
- **Execution traces**: `enter`, `leave` operations fire the registered callbacks

Callbacks are invoked in the correct order:
- Variable traces fire in LIFO order (most recently added first)
- Execution traces fire in LIFO order for enter, LIFO for leave

### Operation Validation

Feather validates that operations are valid for the given trace type:
- Variable traces: `array`, `read`, `unset`, `write`
- Command traces: `delete`, `rename`
- Execution traces: `enter`, `leave`, `enterstep`, `leavestep`

Invalid operations produce an error: `bad operation "X": must be ...`

### Command Existence Checks

For command and execution traces, Feather verifies the command exists:
- `trace add command/execution` on a non-existent command throws "unknown command"
- `trace remove command/execution` on a non-existent command throws "unknown command"
- `trace info command/execution` on a non-existent command throws "unknown command"

### Namespace Resolution

Feather automatically prepends `::` to unqualified command and execution trace names. TCL uses "the usual namespace resolution rules" which may be more sophisticated in edge cases.
