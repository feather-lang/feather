# namespace Builtin Comparison

This document compares Feather's `namespace` implementation against the TCL 9 reference manual.

## Summary of Our Implementation

Feather implements a subset of the TCL `namespace` command with 10 subcommands. The implementation is found in `/Users/dhamidi/projects/feather/src/builtin_namespace.c`.

Our implementation provides:

- Basic namespace creation and deletion
- Namespace hierarchy navigation (parent, children)
- Command evaluation within namespaces
- Export/import mechanisms for commands
- Name manipulation utilities (qualifiers, tail)

## TCL Features We Support

| Subcommand | Status | Notes |
|------------|--------|-------|
| `namespace children ?namespace? ?pattern?` | Partial | We support `?namespace?` but NOT the `?pattern?` argument for filtering |
| `namespace current` | Full | Returns fully-qualified name of current namespace |
| `namespace delete ?namespace ...?` | Full | Deletes namespaces and their contents |
| `namespace eval namespace arg ?arg ...?` | Full | Creates namespace if needed and evaluates script |
| `namespace exists namespace` | Full | Returns 1 if namespace exists, 0 otherwise |
| `namespace export ?-clear? ?pattern ...?` | Full | Manages export patterns for commands |
| `namespace import ?-force? ?pattern ...?` | Partial | Imports commands; does NOT support querying imports with no arguments |
| `namespace parent ?namespace?` | Full | Returns parent namespace |
| `namespace qualifiers string` | Full | Returns namespace qualifiers (path before last `::`) |
| `namespace tail string` | Full | Returns tail component (after last `::`) |

## TCL Features We Do NOT Support

The following subcommands are NOT implemented:

| Subcommand | Description |
|------------|-------------|
| `namespace code script` | Captures namespace context for later execution; returns script wrapped in `namespace inscope` |
| `namespace ensemble subcommand ?arg ...?` | Creates and manipulates ensemble commands (commands formed from subcommands) |
| `namespace forget ?pattern ...?` | Removes previously imported commands from a namespace |
| `namespace inscope namespace script ?arg ...?` | Executes script in namespace context with additional args appended as list elements |
| `namespace origin command` | Returns fully-qualified name of the original command for an imported command |
| `namespace path ?namespaceList?` | Gets/sets the command resolution path for the current namespace |
| `namespace unknown ?script?` | Gets/sets the unknown command handler for the current namespace |
| `namespace upvar namespace ?otherVar myVar ...?` | Creates local variables that refer to namespace variables |
| `namespace which ?-command? ?-variable? name` | Looks up name as command or variable and returns fully-qualified name |

## Notes on Implementation Differences

### namespace children

TCL supports an optional `pattern` argument to filter children using glob-style matching:

```tcl
namespace children ::foo ::foo::bar*
```

Our implementation only supports:

```tcl
namespace children ?namespace?
```

### namespace import

TCL allows calling `namespace import` with no arguments to query the list of imported commands in the current namespace. Our implementation requires at least one pattern argument.

TCL:
```tcl
namespace import  ;# Returns list of imported commands
```

Feather:
```
wrong # args: should be "namespace import ?-force? ?pattern pattern ...?"
```

### namespace delete

TCL allows calling `namespace delete` with no arguments (which is a no-op). Our implementation requires at least one namespace name.

TCL:
```tcl
namespace delete  ;# Does nothing, no error
```

Feather:
```
wrong # args: should be "namespace delete ?name name ...?"
```

### Subcommand Abbreviation

TCL allows abbreviation of subcommand names (e.g., `namespace cur` for `namespace current`). Our implementation requires exact subcommand names.

### Command Resolution Path

TCL's `namespace path` allows setting a list of namespaces to search when resolving commands. This affects how commands are found when not in the current or global namespace. Feather does not support this feature.

### Ensemble Commands

TCL's `namespace ensemble` is a major feature that allows creating command ensembles (like `string` or `dict` which have subcommands). This is not implemented in Feather.

### Unknown Handler

TCL allows customizing the unknown command handler per-namespace via `namespace unknown`. Feather uses a global unknown handler approach only.
