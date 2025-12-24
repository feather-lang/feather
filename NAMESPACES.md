# M10: Namespace Design

This document describes the design for namespace support in tclc.

## Scope

### Included in M10

- `namespace eval ns script` - evaluate script in namespace context
- `namespace current` - return current namespace path
- `namespace exists ns` - check if namespace exists
- `namespace children ?ns?` - list child namespaces
- `namespace parent ?ns?` - get parent namespace
- `namespace delete ns ?ns ...?` - delete namespaces
- `variable name ?value?` - declare/link namespace variables

### Deferred

- `namespace export` / `namespace import` - command visibility
- Relative namespace paths in command resolution (e.g., `foo::bar` resolving commands)

## Core Concepts

### Two Variable Storage Locations

| Location | Lifetime | Created by | Example |
|----------|----------|------------|---------|
| Frame-local | Per-call, temporary | `set x 1` (unqualified) | Local variable in proc |
| Namespace | Persistent | `set ::foo::x 1` or `variable x` | Namespace variable |

### Per-Frame Namespace Context

Each call frame has an associated namespace:

- Global frame (level 0): namespace is `::`
- Proc call: namespace is where the proc was defined
- `namespace eval`: temporarily changes current frame's namespace

### Variable Name Resolution

Three forms of variable names:

| Form | Example | Meaning |
|------|---------|---------|
| Unqualified | `x` | Frame-local variable |
| Absolute | `::foo::x` | Variable `x` in namespace `::foo` |
| Relative | `foo::x` | Variable `x` in `<current>::foo` |

Resolution happens in C code before calling host operations.

## FeatherHostOps Extensions

### New: FeatherNamespaceOps

```c
/**
 * FeatherNamespaceOps provides operations on the namespace hierarchy.
 *
 * Namespaces are containers for commands and persistent variables.
 * The global namespace "::" always exists and is the root.
 * Namespace paths use "::" as separator (e.g., "::foo::bar").
 */
typedef struct FeatherNamespaceOps {
    /**
     * create ensures a namespace exists, creating it and parents as needed.
     *
     * Returns TCL_OK on success.
     * Creating "::" is a no-op (always exists).
     */
    FeatherResult (*create)(FeatherInterp interp, FeatherObj path);

    /**
     * delete removes a namespace and all its children.
     *
     * Variables and commands in the namespace are destroyed.
     * Returns TCL_ERROR if path is "::" (cannot delete global).
     * Returns TCL_ERROR if namespace doesn't exist.
     */
    FeatherResult (*delete)(FeatherInterp interp, FeatherObj path);

    /**
     * exists checks if a namespace exists.
     *
     * Returns TCL_OK if it exists, TCL_ERROR if not.
     */
    FeatherResult (*exists)(FeatherInterp interp, FeatherObj path);

    /**
     * current returns the namespace path of the current call frame.
     *
     * Returns a string like "::" or "::foo::bar".
     */
    FeatherObj (*current)(FeatherInterp interp);

    /**
     * parent returns the parent namespace path.
     *
     * For "::", returns empty string.
     * For "::foo::bar", returns "::foo".
     * Returns TCL_ERROR if namespace doesn't exist.
     */
    FeatherResult (*parent)(FeatherInterp interp, FeatherObj ns, FeatherObj *result);

    /**
     * children returns a list of child namespace paths.
     *
     * Returns full paths (e.g., "::foo::bar" for child "bar" of "::foo").
     * Returns empty list if no children.
     */
    FeatherObj (*children)(FeatherInterp interp, FeatherObj ns);

    /**
     * get_var retrieves a variable from namespace storage.
     *
     * Returns nil if variable doesn't exist.
     * The 'name' parameter must be unqualified (just "x", not "::foo::x").
     * The namespace path must be absolute.
     */
    FeatherObj (*get_var)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

    /**
     * set_var sets a variable in namespace storage.
     *
     * Creates the variable if it doesn't exist.
     * Creates the namespace if it doesn't exist.
     * The 'name' parameter must be unqualified.
     */
    void (*set_var)(FeatherInterp interp, FeatherObj ns, FeatherObj name, FeatherObj value);

    /**
     * var_exists checks if a variable exists in namespace storage.
     *
     * Returns TCL_OK if exists, TCL_ERROR if not.
     */
    FeatherResult (*var_exists)(FeatherInterp interp, FeatherObj ns, FeatherObj name);

    /**
     * unset_var removes a variable from namespace storage.
     *
     * No-op if variable doesn't exist.
     */
    void (*unset_var)(FeatherInterp interp, FeatherObj ns, FeatherObj name);
} FeatherNamespaceOps;
```

### FeatherFrameOps Extension

```c
typedef struct FeatherFrameOps {
    /* ... existing operations ... */

    /**
     * set_namespace changes the namespace of the current frame.
     *
     * Used by 'namespace eval' to temporarily change context.
     * The namespace is created if it doesn't exist.
     */
    FeatherResult (*set_namespace)(FeatherInterp interp, FeatherObj ns);

    /**
     * get_namespace returns the namespace of the current frame.
     */
    FeatherObj (*get_namespace)(FeatherInterp interp);
} FeatherFrameOps;
```

### FeatherVarOps Extension

```c
typedef struct FeatherVarOps {
    /* ... existing operations ... */

    /**
     * link_ns creates a link from a local variable to a namespace variable.
     *
     * Used by 'variable' command. After linking, operations on the
     * local variable name affect the namespace variable:
     *   - get(local) returns ns.get_var(ns, name)
     *   - set(local, val) calls ns.set_var(ns, name, val)
     *   - exists(local) checks ns.var_exists(ns, name)
     *   - unset(local) calls ns.unset_var(ns, name)
     *
     * The 'local' parameter is the local name in the current frame.
     * The 'ns' parameter is the absolute namespace path.
     * The 'name' parameter is the variable name in the namespace.
     */
    void (*link_ns)(FeatherInterp interp, FeatherObj local, FeatherObj ns, FeatherObj name);
} FeatherVarOps;
```

### Updated FeatherHostOps

```c
typedef struct FeatherHostOps {
    FeatherFrameOps frame;
    FeatherVarOps var;
    FeatherProcOps proc;
    FeatherNamespaceOps ns;      /* NEW */
    FeatherStringOps string;
    FeatherListOps list;
    FeatherIntOps integer;
    FeatherDoubleOps dbl;
    FeatherInterpOps interp;
    FeatherBindOpts bind;
} FeatherHostOps;
```

## C-Side Resolution

All qualified name parsing happens in C code. The host only receives
unqualified names with explicit namespace paths.

### Resolution Function

```c
/**
 * feather_resolve_variable resolves a variable name to namespace + local parts.
 *
 * Three cases:
 *   1. Unqualified ("x") - no "::" in name
 *      -> ns_out = nil, local_out = "x"
 *      -> Caller uses var.get for frame-local lookup
 *
 *   2. Absolute ("::foo::x") - starts with "::"
 *      -> ns_out = "::foo", local_out = "x"
 *
 *   3. Relative ("foo::x") - contains "::" but doesn't start with it
 *      -> Prepends current namespace from ops->ns.current()
 *      -> If current is "::bar", resolves to ns="::bar::foo", local="x"
 *
 * Returns TCL_OK on success.
 */
FeatherResult feather_resolve_variable(const FeatherHostOps *ops, FeatherInterp interp,
                                const char *name, size_t len,
                                FeatherObj *ns_out, FeatherObj *local_out);

/**
 * feather_is_qualified returns 1 if name contains "::", 0 otherwise.
 */
int feather_is_qualified(const char *name, size_t len);
```

### Usage Pattern

The parser and all variable-touching builtins use this pattern:

```c
FeatherObj ns, local;
feather_resolve_variable(ops, interp, name, len, &ns, &local);

if (ops->list.is_nil(interp, ns)) {
    // Unqualified - use frame-local storage
    value = ops->var.get(interp, local);
} else {
    // Qualified - use namespace storage
    value = ops->ns.get_var(interp, ns, local);
}
```

## Affected Files

### C Interpreter

| File | Changes |
|------|---------|
| `src/feather.h` | Add FeatherNamespaceOps, extend FeatherFrameOps, FeatherVarOps |
| `src/internal.h` | Declare resolution helpers, new builtins |
| `src/resolve.c` | NEW: feather_resolve_variable, feather_is_qualified |
| `src/parse.c` | Update substitute_variable for qualified names |
| `src/builtin_namespace.c` | NEW: namespace command |
| `src/builtin_variable.c` | NEW: variable command |
| `src/builtin_set.c` | Add qualified name dispatch |
| `src/builtin_incr.c` | Add qualified name dispatch |
| `src/builtin_info.c` | Add qualified name dispatch in info exists |
| `src/builtin_catch.c` | Add qualified name dispatch for result var |
| `src/builtin_upvar.c` | Handle qualified target variables |
| `src/interp.c` | Register new builtins |

### Go Host

| File | Changes |
|------|---------|
| `interp/tclc.go` | Add Namespace struct, extend CallFrame |
| `interp/callbacks.go` | Implement FeatherNamespaceOps callbacks |
| `interp/callbacks.c` | Wire up new C callbacks |

## Go Host Implementation

### Data Structures

```go
// Namespace represents a namespace in the hierarchy
type Namespace struct {
    fullPath string
    parent   *Namespace
    children map[string]*Namespace
    vars     map[string]FeatherObj
}

// Extended CallFrame
type CallFrame struct {
    cmd       FeatherObj
    args      FeatherObj
    vars      map[string]FeatherObj
    links     map[string]varLink
    namespace *Namespace          // NEW: execution context
    level     int
}

// Extended varLink for namespace variable links
type varLink struct {
    // For upvar (frame-to-frame link)
    targetLevel int
    targetName  string

    // For namespace variable link (NEW)
    nsLink *nsVarLink
}

type nsVarLink struct {
    namespace string  // absolute path, e.g., "::foo"
    name      string  // variable name in namespace
}
```

### Variable Resolution in Host

When `var.get` is called (unqualified names only after C resolution):

1. Check `frame.links[name]`
   - If `nsLink != nil`: return `namespace.vars[nsLink.name]`
   - If `targetLevel` set: follow to target frame (existing upvar)
2. Check `frame.vars[name]`
3. Return nil if not found

## Command Semantics

### namespace eval

```tcl
namespace eval ns script
```

1. Create namespace `ns` if it doesn't exist (resolve relative to current)
2. Save current frame's namespace
3. Set current frame's namespace to `ns`
4. Evaluate `script`
5. Restore frame's namespace
6. Return script result

### namespace current

```tcl
namespace current
```

Returns the absolute path of the current frame's namespace.

### namespace exists

```tcl
namespace exists ns
```

Returns 1 if namespace exists, 0 otherwise.

### namespace children

```tcl
namespace children ?ns?
```

Returns list of child namespace paths. Defaults to current namespace.

### namespace parent

```tcl
namespace parent ?ns?
```

Returns parent namespace path. Returns empty string for "::".

### namespace delete

```tcl
namespace delete ns ?ns ...?
```

Deletes namespaces and their children. Error if deleting "::".

### variable

```tcl
variable name ?value?
variable name ?value? ?name value ...?
```

1. Resolve `name` in current namespace (e.g., if in `::foo`, name `x` -> `::foo::x`)
2. Create namespace variable if it doesn't exist
3. Set initial value if provided
4. Create link in current frame: local `name` -> namespace variable
5. Subsequent `set name` in this frame modifies the namespace variable

## Examples

### Basic Namespace Variable

```tcl
namespace eval foo {
    variable x 10      ;# Creates ::foo::x = 10, links local x
    set x 20           ;# Modifies ::foo::x through link
}
set ::foo::x           ;# Returns 20 - persisted
```

### Qualified Variable Access

```tcl
set ::config::debug 1           ;# Creates ::config::debug
proc log {msg} {
    if {$::config::debug} {     ;# Reads ::config::debug
        puts $msg
    }
}
```

### Relative Namespace Path

```tcl
namespace eval foo {
    namespace eval bar {
        variable x 1    ;# Creates ::foo::bar::x
    }
    set bar::x 2        ;# Modifies ::foo::bar::x (relative path)
}
```

### Variable Command in Proc

```tcl
namespace eval counter {
    variable count 0

    proc incr {} {
        variable count      ;# Links to ::counter::count
        incr count
    }

    proc get {} {
        variable count
        return $count
    }
}

counter::incr
counter::incr
counter::get  ;# Returns 2
```
