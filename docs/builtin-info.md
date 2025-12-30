# Feather `info` Builtin Comparison

This document compares Feather's `info` command implementation against the standard TCL `info` command as documented in the TCL 9 manual.

## Summary of Our Implementation

Feather implements the following `info` subcommands:

- `info args procname` - Returns the parameter names of a procedure
- `info body procname` - Returns the body of a procedure
- `info commands ?pattern?` - Returns visible command names
- `info default procname arg varname` - Checks for parameter default values
- `info exists varName` - Checks if a variable exists
- `info frame ?number?` - Returns call frame information
- `info globals ?pattern?` - Returns global variable names
- `info level ?number?` - Returns call stack level information
- `info locals ?pattern?` - Returns local variable names
- `info methods value` - Returns method names for foreign objects (Feather extension)
- `info procs ?pattern?` - Returns procedure names
- `info script` - Returns the current script file path
- `info type value` - Returns the type of a value (Feather extension)
- `info vars ?pattern?` - Returns visible variable names

## TCL Features We Support

| Subcommand | Status | Notes |
|------------|--------|-------|
| `info args procname` | Supported | Fully compatible |
| `info body procname` | Supported | Fully compatible |
| `info commands ?pattern?` | Supported | Namespace-aware pattern matching |
| `info default procname arg varname` | Supported | Fully compatible |
| `info exists varName` | Supported | Handles qualified names |
| `info frame ?number?` | Supported | Returns dict with type, cmd, proc, level, file, namespace, line, lambda |
| `info globals ?pattern?` | Supported | Fully compatible |
| `info level ?number?` | Supported | Supports relative (negative) and absolute levels |
| `info locals ?pattern?` | Supported | Excludes linked variables (global/upvar/variable) |
| `info procs ?pattern?` | Supported | Namespace-aware, returns only user-defined procs |
| `info script` | Partial | Returns path but does NOT support setting filename (TCL allows `info script ?filename?`) |
| `info vars ?pattern?` | Supported | Namespace-aware pattern matching |

## TCL Features We Do NOT Support

### Core Subcommands Not Implemented

| Subcommand | TCL Description |
|------------|-----------------|
| `info cmdcount` | Returns total number of commands evaluated in interpreter |
| `info cmdtype commandName` | Returns the type of a command (alias, coroutine, ensemble, import, native, object, privateObject, proc, interp, zlibStream) |
| `info complete command` | Returns 1 if command is syntactically complete (useful for multi-line input) |
| `info constant varName` | Returns 1 if variable is a constant |
| `info consts ?pattern?` | Returns list of constant variables |
| `info coroutine` | Returns name of current coroutine |
| `info errorstack ?interp?` | Returns description of active command at each level for last error |
| `info functions ?pattern?` | Returns list of math functions |
| `info hostname` | Returns name of current host |
| `info library` | Returns value of tcl_library |
| `info loaded ?interp? ?prefix?` | Returns info about loaded shared libraries |
| `info nameofexecutable` | Returns absolute pathname of interpreter executable |
| `info patchlevel` | Returns value of tcl_patchLevel |
| `info sharedlibextension` | Returns shared library extension for platform (e.g., .so) |
| `info tclversion` | Returns TCL version number |

### Class Introspection (`info class`) Not Implemented

TCL provides extensive class introspection via `info class`:

- `info class call class method`
- `info class constructor class`
- `info class definition class method`
- `info class definitionnamespace class ?kind?`
- `info class destructor class`
- `info class filters class`
- `info class forward class method`
- `info class instances class ?pattern?`
- `info class methods class ?options...?`
- `info class methodtype class method`
- `info class mixins class`
- `info class properties class ?options...?`
- `info class subclasses class ?pattern?`
- `info class superclasses class`
- `info class variables class ?-private?`

### Object Introspection (`info object`) Not Implemented

TCL provides object introspection via `info object`:

- `info object call object method`
- `info object class object ?className?`
- `info object creationid object`
- `info object definition object method`
- `info object filters object`
- `info object forward object method`
- `info object isa ...` (various class/mixin/object checks)
- `info object methods object ?options...?`
- `info object methodtype object method`
- `info object mixins object`
- `info object namespace object`
- `info object properties object ?options...?`
- `info object variables object ?-private?`

## Notes on Implementation Differences

### `info script`

- **TCL**: Supports both reading (`info script`) and setting (`info script filename`) the script path
- **Feather**: Only supports reading; setting is not implemented

### `info frame`

- **TCL**: Returns keys including `type`, `line`, `file`, `cmd`, `proc`, `lambda`, `level`
- **Feather**: Returns same keys plus `namespace`; type values are `proc`, `source`, or `eval` (TCL also has `precompiled`)

### `info type` (Feather Extension)

This is a Feather-specific extension that does not exist in standard TCL. It returns the type of a value:
- For foreign objects: the registered type name
- For collections: "list" or "dict"
- For numbers: "int" or "double"
- For everything else: "string"

### `info methods` (Feather Extension)

This is a Feather-specific extension for introspecting foreign object methods. Standard TCL uses `info object methods` for similar functionality on TclOO objects.

### Pattern Matching

Both Feather and TCL use `string match` glob-style patterns for filtering results in subcommands like `info commands`, `info procs`, `info vars`, etc. Feather correctly implements namespace-aware patterns where only the final component is treated as a pattern.

### Namespace Integration

Feather's implementation properly integrates with namespaces:
- `info commands` and `info procs` search current namespace plus global namespace
- Qualified patterns (containing `::`) search the specified namespace
- Results include fully qualified names when patterns are qualified
