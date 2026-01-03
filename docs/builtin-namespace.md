# namespace Builtin Comparison

This document compares Feather's `namespace` implementation against the TCL 9 reference manual.

## Summary of Our Implementation

Feather implements 15 of TCL's `namespace` subcommands. The implementation is found in `src/builtin_namespace.c`.

Our implementation provides:

- Basic namespace creation and deletion
- Namespace hierarchy navigation (parent, children)
- Command evaluation within namespaces
- Export/import mechanisms for commands
- Name manipulation utilities (qualifiers, tail)

## TCL Features We Support

| Subcommand | Status | Notes |
|------------|--------|-------|
| `namespace children ?namespace? ?pattern?` | Full | Lists child namespaces, optional pattern filtering |
| `namespace code script` | Full | Wraps script with `::namespace inscope` for deferred execution |
| `namespace current` | Full | Returns fully-qualified name of current namespace |
| `namespace delete ?namespace ...?` | Full | Deletes namespaces; no args is a no-op |
| `namespace eval namespace arg ?arg ...?` | Full | Creates namespace if needed and evaluates script |
| `namespace exists namespace` | Full | Returns 1 if namespace exists, 0 otherwise |
| `namespace export ?-clear? ?pattern ...?` | Full | Manages export patterns for commands |
| `namespace forget ?pattern ...?` | Full | Removes previously imported commands |
| `namespace import ?-force? ?pattern ...?` | Full | Imports commands; no args returns list of imports |
| `namespace inscope namespace script ?arg...?` | Full | Executes script in namespace context with extra args |
| `namespace origin command` | Full | Returns fully-qualified name of original command for imports |
| `namespace parent ?namespace?` | Full | Returns parent namespace |
| `namespace qualifiers string` | Full | Returns namespace qualifiers (path before last `::`) |
| `namespace tail string` | Full | Returns tail component (after last `::`) |
| `namespace which ?-command? ?-variable? name` | Full | Looks up command or variable, returns fully-qualified name |

## TCL Features We Do NOT Support

The following subcommands are NOT implemented:

| Subcommand | Description |
|------------|-------------|
| `namespace ensemble subcommand ?arg ...?` | Creates and manipulates ensemble commands (commands formed from subcommands) |
| `namespace path ?namespaceList?` | Gets/sets the command resolution path for the current namespace |
| `namespace unknown ?script?` | Gets/sets the unknown command handler for the current namespace |
| `namespace upvar namespace ?otherVar myVar ...?` | Creates local variables that refer to namespace variables |

## Notes on Implementation Differences

### Subcommand Abbreviation

TCL allows abbreviation of subcommand names (e.g., `namespace cur` for `namespace current`). Our implementation requires exact subcommand names.

### Command Resolution Path

TCL's `namespace path` allows setting a list of namespaces to search when resolving commands. This affects how commands are found when not in the current or global namespace. Feather does not support this feature.

### Ensemble Commands

TCL's `namespace ensemble` is a major feature that allows creating command ensembles (like `string` or `dict` which have subcommands). This is not implemented in Feather.

### Unknown Handler

TCL allows customizing the unknown command handler per-namespace via `namespace unknown`. Feather uses a global unknown handler approach only.

## Implementation Notes

### Import Tracking

Import information is stored in the `::tcl` namespace using variables of the form `::tcl::imports::<namespace>`. Each such variable is a dict mapping imported command names to their fully-qualified origin paths (e.g., `{cmd1 ::src::cmd1 cmd2 ::src::cmd2}`). This enables:

- `namespace import` with no args to list imported commands
- `namespace origin` to return the original command for imports
- `namespace forget` to remove imported commands by matching origin patterns
