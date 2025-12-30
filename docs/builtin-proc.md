# Builtin: proc

Comparison of feather's `proc` implementation with TCL 9.

## Summary of Our Implementation

The `proc` command in feather creates a new TCL procedure. The implementation is found in `src/builtin_proc.c` and consists of two main functions:

1. **`feather_builtin_proc`** - Handles the `proc name args body` command itself
2. **`feather_invoke_proc`** - Handles invocation of user-defined procedures

Key aspects of our implementation:
- Validates exactly 3 arguments are provided (name, args, body)
- Supports namespace-qualified procedure names (e.g., `::foo::bar`)
- Creates intermediate namespaces automatically if a qualified name is used
- Supports the special `args` parameter for variadic procedures
- Pushes a new call frame for each procedure invocation
- Sets the namespace context based on where the procedure was defined
- Handles `return` with `-code` and `-level` options
- Provides stack traces on errors

## TCL Features We Support

### Basic Procedure Definition
```tcl
proc name args body
```
Creates a procedure with the given name, argument list, and body.

### Namespace-Qualified Names
```tcl
proc ::myns::myproc {x} { ... }
```
Procedures can be defined with fully qualified namespace paths. The namespace is created automatically if it doesn't exist.

### Variadic Procedures with `args`
```tcl
proc printAll args {
    foreach arg $args {
        puts $arg
    }
}
```
When the last formal argument is named `args`, it collects all remaining actual arguments into a list.

### Procedure Return Values
- The return value is the value from an explicit `return` command
- If no explicit `return`, the return value is the result of the last command executed
- Errors in the procedure body propagate to the caller

### Return Options
The `return` command with `-code` and `-level` options is properly handled, allowing for non-local returns and exception-like behavior.

### Local Variables
Each procedure invocation creates a new call frame with its own local variables.

### Namespace Context
When a procedure is invoked, the current namespace is set to the namespace where the procedure was defined (the namespace part of its qualified name).

## TCL Features We Do NOT Support

### Default Parameter Values
TCL supports default values for parameters:
```tcl
proc mult {varName {multiplier 2}} {
    upvar 1 $varName var
    set var [expr {$var * $multiplier}]
}
```
In this example, `multiplier` defaults to `2` if not provided.

**Our implementation does not support default parameter values.** All parameters (except `args`) are required.

### Mixed Required and Optional Arguments
TCL allows mixing required and optional (defaulted) arguments:
```tcl
proc example {required1 required2 {optional1 default1} {optional2 default2}} {
    # ...
}
```
Arguments with defaults followed by non-defaulted arguments become required in TCL.

**We do not support this feature** since we don't support default values at all.

### Replacing Existing Commands
TCL's `proc` replaces any existing command or procedure with the same name. While our implementation does register the new procedure, we should verify the replacement behavior is complete (including replacing built-in commands).

## Notes on Implementation Differences

### Error Messages
Our error messages for wrong argument count follow the TCL convention:
```
wrong # args: should be "procname param1 param2 ?arg ...?"
```
The `?arg ...?` notation is used for variadic procedures.

### Namespace Creation
We automatically create intermediate namespaces when defining a procedure with a qualified name like `::foo::bar::baz`. TCL may behave differently depending on the version.

### Stack Traces
We implement error stack traces that include the procedure name, arguments, and line numbers. This is done through `feather_error_append_frame()`.

### Frame Management
Our implementation explicitly manages call frames, including:
- Pushing a new frame for each procedure call
- Setting the namespace context for the frame
- Copying line number information from the parent frame
- Popping the frame on return

### Return Handling
We handle `TCL_RETURN` with `-code` and `-level` options, decrementing the level at each procedure boundary until it reaches 0, at which point the specified code (OK, ERROR, etc.) is applied.
