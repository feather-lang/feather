# Feather `rename` Builtin

This document compares feather's implementation of the `rename` command against the official TCL manual.

## Summary of Our Implementation

The feather implementation of `rename` is located in `src/builtin_rename.c`. It provides the core functionality of renaming or deleting commands in the interpreter.

**Syntax:** `rename oldName newName`

**Behavior:**
- Renames a command from `oldName` to `newName`
- If `newName` is an empty string, the command is deleted
- Supports namespace-qualified names (e.g., `::ns::cmd`)
- Resolves unqualified names relative to the current namespace
- Fires command traces on successful rename/delete operations

**Error conditions:**
- `wrong # args: should be "rename oldName newName"` - incorrect argument count
- `can't rename "oldName": command doesn't exist` - source command not found
- `can't rename to "newName": command already exists` - target name already in use

## TCL Features We Support

| Feature | Supported | Notes |
|---------|-----------|-------|
| Basic rename (`rename old new`) | Yes | Core functionality |
| Delete command (`rename old ""`) | Yes | Empty string deletes the command |
| Namespace-qualified names | Yes | Supports `::ns::cmd` syntax |
| Namespace resolution | Yes | Unqualified names resolved in current namespace |
| Error on non-existent command | Yes | Matches TCL error message format |
| Error on existing target | Yes | Prevents overwriting existing commands |
| Empty string result | Yes | Returns empty string on success |
| Command traces | Yes | Fires "rename" or "delete" traces |

## TCL Features We Do NOT Support

Based on comparison with the TCL 9 manual, our implementation appears to be **feature-complete** with respect to the documented behavior. The TCL manual specifies:

1. Basic rename functionality - **Supported**
2. Delete via empty string - **Supported**
3. Namespace qualifiers in both names - **Supported**
4. Command executes in new namespace after rename - **Supported** (commands are stored with fully qualified names)

There are no documented features in TCL's `rename` command that are missing from our implementation.

## Notes on Implementation Differences

### Namespace Resolution

Our implementation performs explicit namespace resolution:

1. First tries the name as provided
2. If not found and unqualified, tries with `::` prefix (global namespace)
3. When renaming to a new name, qualifies it relative to the current namespace

This matches TCL's behavior where unqualified names are resolved in the current namespace context.

### Command Trace Integration

Our implementation fires command traces after a successful rename operation:
- Operation `"rename"` when renaming to a non-empty name
- Operation `"delete"` when renaming to an empty string (deletion)

This integrates with the `trace` command infrastructure.

### Error Message Format

Our error messages follow TCL's standard format:
- `can't rename "oldName": command doesn't exist`
- `can't rename to "newName": command already exists`

These match the expected TCL error message patterns.
