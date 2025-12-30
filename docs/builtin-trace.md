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

## TCL Features We Do NOT Support

### Variable Trace Operations

| Operation | TCL Behavior | Feather Status |
|-----------|-------------|----------------|
| `read` | Invoke callback when variable is read | Not implemented (registration only) |
| `write` | Invoke callback when variable is written | Not implemented (registration only) |
| `unset` | Invoke callback when variable is unset | Not implemented (registration only) |
| `array` | Invoke callback when variable accessed via `array` command | Not implemented |

### Command Trace Operations

| Operation | TCL Behavior | Feather Status |
|-----------|-------------|----------------|
| `rename` | Invoke callback when command is renamed | Not implemented (registration only) |
| `delete` | Invoke callback when command is deleted | Not implemented (registration only) |

### Execution Trace Operations

| Operation | TCL Behavior | Feather Status |
|-----------|-------------|----------------|
| `enter` | Invoke callback before command executes | Not implemented (registration only) |
| `leave` | Invoke callback after command executes | Not implemented (registration only) |
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

**Feather status:** None of these callback invocation mechanisms are implemented. Traces can be registered but are never actually fired.

### Advanced Features

| Feature | TCL Behavior | Feather Status |
|---------|-------------|----------------|
| Command existence check | `trace add command/execution` throws error if command does not exist | Not implemented |
| Variable creation | `trace add variable` creates variable if it does not exist (undefined but visible) | Not implemented |
| Trace disabling during callback | Traces on target are temporarily disabled while callback runs | Not implemented |
| Error propagation | Errors in trace callbacks propagate to traced operation | Not implemented |
| Variable modification in callbacks | Read/write trace callbacks can modify variable value | Not implemented |
| Multiple trace ordering | Variable traces: most-recent first; execution traces: enter/enterstep reverse order, leave/leavestep original order | Not implemented |
| Array element traces | Traces on individual array elements | Not implemented |
| Array-wide traces | Traces on entire arrays that fire for any element access | Not implemented |
| Trace removal on unset | Variable traces removed when variable is unset | Not implemented |
| upvar interaction | Traces fire with actual variable name, may differ from traced name | Not implemented |

## Notes on Implementation Differences

### Registration-Only Implementation

The current Feather implementation is a **registration-only** implementation. This means:

1. Traces can be added, removed, and queried
2. Trace information is stored correctly in internal dictionaries
3. **Traces are never actually fired** - the interpreter does not invoke the registered callbacks when the traced operations occur

This is a significant difference from TCL, where trace callbacks are actively invoked during variable access, command modification, and command execution.

### Operation Validation

The current implementation does not validate that the operations in `ops` are valid for the given trace type. For example:
- Variable traces should only accept `read`, `write`, `unset`, `array`
- Command traces should only accept `rename`, `delete`
- Execution traces should only accept `enter`, `leave`, `enterstep`, `leavestep`

Feather currently accepts any operation list without validation.

### Error Handling

TCL throws errors in certain cases that Feather does not:
- `trace add command/execution` on a non-existent command
- `trace remove command/execution` on a non-existent command
- `trace info command/execution` on a non-existent command

Feather silently accepts these operations without error.

### Namespace Resolution

Feather automatically prepends `::` to unqualified command and execution trace names. TCL uses "the usual namespace resolution rules" which may be more sophisticated in edge cases.
