# dict Builtin Command Comparison

This document compares feather's `dict` builtin implementation against the standard TCL `dict` command as documented in TCL 8.5+.

## Summary of Our Implementation

Feather implements the `dict` command in `src/builtin_dict.c`. The implementation provides 21 subcommands for dictionary manipulation, including basic CRUD operations, iteration, filtering, mapping, and modification of dictionary values stored in variables.

Key characteristics:
- Dictionaries are order-preserving key-value mappings
- Nested dictionary access is supported via multiple key arguments
- Variable-modifying subcommands (set, unset, append, incr, lappend) operate on a variable name, not a value
- Glob pattern filtering is supported for `keys` and `values` subcommands
- Maximum nesting depth is 64 levels (implementation limit)

## TCL Features We Support

The following `dict` subcommands are fully implemented:

| Subcommand | Syntax | Description |
|------------|--------|-------------|
| `append` | `dict append dictVar key ?string ...?` | Appends strings to the value for a key in a variable |
| `create` | `dict create ?key value ...?` | Creates a new dictionary from key-value pairs |
| `exists` | `dict exists dictValue key ?key ...?` | Tests if a key path exists in a dictionary |
| `for` | `dict for {keyVar valueVar} dictValue body` | Iterates over key-value pairs, supports break/continue |
| `get` | `dict get dictValue ?key ...?` | Retrieves a value by key path; returns all pairs if no key given |
| `getdef` | `dict getdef dictValue ?key ...? key default` | Like `get` but returns default if key not found |
| `getwithdefault` | `dict getwithdefault dictValue ?key ...? key default` | Alias for `getdef` |
| `incr` | `dict incr dictVar key ?increment?` | Increments an integer value for a key |
| `info` | `dict info dictValue` | Returns information about the dictionary (simplified) |
| `keys` | `dict keys dictValue ?globPattern?` | Returns list of keys, optionally filtered by glob |
| `lappend` | `dict lappend dictVar key ?value ...?` | Appends values to a list stored at a key |
| `merge` | `dict merge ?dictValue ...?` | Merges multiple dictionaries (later keys win) |
| `remove` | `dict remove dictValue ?key ...?` | Returns dictionary with specified keys removed |
| `replace` | `dict replace dictValue ?key value ...?` | Returns dictionary with updated/added key-value pairs |
| `set` | `dict set dictVar key ?key ...? value` | Sets a value in a nested dictionary path |
| `size` | `dict size dictValue` | Returns the number of key-value pairs |
| `unset` | `dict unset dictVar key ?key ...?` | Removes a key from a nested dictionary path |
| `values` | `dict values dictValue ?globPattern?` | Returns list of values, optionally filtered by glob |
| `filter` | `dict filter dictValue filterType ?arg ...?` | Filters dictionary by key patterns, value patterns, or script |
| `map` | `dict map {keyVar valueVar} dictValue body` | Transforms dictionary values using a script |
| `update` | `dict update dictVar key varName ?key varName ...? body` | Binds keys to variables, updates after body |
| `with` | `dict with dictVar ?key ...? body` | Opens dict keys as variables, updates after body |

### `dict filter` Details

Supports three filter types:
- `dict filter dictValue key ?globPattern ...?` - Filter by key patterns (OR'd together)
- `dict filter dictValue value ?globPattern ...?` - Filter by value patterns (OR'd together)
- `dict filter dictValue script {keyVar valueVar} script` - Filter using a script that returns boolean

Supports `break` (stops filtering, returns results so far) and `continue` (skips current key) in script mode.

### `dict map` Details

Transforms dictionary values by evaluating a body script for each key-value pair. The result of each script evaluation becomes the new value for that key. Supports `break` (returns empty dict) and `continue` (skips key-value pair).

### `dict update` Details

Binds specified dictionary keys to local variables, executes the body, then writes the (potentially modified) values back to the dictionary. If a variable is unset, the corresponding key is removed from the dictionary. If a key doesn't exist initially, the variable is not created, but if set during the body, the key is added.

### `dict with` Details

Opens up a dictionary (or nested dictionary at the given key path) so that all its keys become local variables. After the body executes, any changes to those variables are written back to the dictionary. Unsetting a variable removes the key. New variables created during the body are NOT added as new keys (only existing keys are tracked).

## TCL Features We Do NOT Support

All major `dict` subcommands are now implemented.

## Notes on Implementation Differences

### `dict info`

Our implementation returns a simplified string: `"N entries in table"` where N is the dictionary size. Standard TCL returns detailed hash table statistics (bucket counts, chain lengths, etc.) intended for debugging hash table performance. Since our implementation may use different internal structures, this simplified output is acceptable.

### Error Messages

Our error messages closely follow TCL conventions but may have minor wording differences. For example:
- We use `"wrong # args: should be ..."` format consistent with TCL
- Key not found errors follow the format `key "X" not known in dictionary`

### Array Default Values

TCL 8.6+ added support for array default values that interact with dict commands. When a dict variable refers to a non-existent array element that has a default value set, TCL uses that default. We do not implement this feature as it depends on TCL's array system which is outside the scope of the dict command itself.

### Return Values for Variable-Modifying Commands

Commands like `dict set`, `dict unset`, `dict append`, `dict incr`, and `dict lappend` all return the updated dictionary value (stored in the variable). This matches TCL behavior.

### Nested Dictionary Depth Limit

Our implementation has a hard-coded limit of 64 levels of nesting for `dict set` and `dict unset` operations. Standard TCL has no documented limit (bounded only by available memory). This should be sufficient for all practical use cases.
