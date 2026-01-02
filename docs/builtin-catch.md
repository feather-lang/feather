# catch Command Implementation

## Summary of Our Implementation

Our `catch` command implementation in `src/builtin_catch.c` provides full functionality of the TCL `catch` command:

- Syntax: `catch script ?resultVar? ?optionsVar?`
- Evaluates the script in a local context
- Returns an integer return code (0 for OK, 1 for ERROR, 2 for RETURN, etc.)
- Optionally stores the result in `resultVar`
- Optionally stores complete return options in `optionsVar`
- Handles `TCL_RETURN` by unwrapping `-code` and `-level` options
- Finalizes error state and populates all error-related options

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic syntax `catch script` | Supported | Returns return code as integer |
| `resultVar` argument | Supported | Stores result/error message in variable |
| `optionsVar` argument | Supported | Stores return options dictionary |
| Return codes (TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, TCL_CONTINUE) | Supported | Returns 0, 1, 2, 3, 4 respectively |
| `-code` option in options dict | Supported | Always set in options dictionary |
| `-level` option in options dict | Supported | Always set (0 for non-RETURN cases) |
| `-errorinfo` option | Supported | Human-readable stack trace |
| `-errorcode` option | Supported | Machine-readable error code (defaults to NONE) |
| `-errorstack` option | Supported | Call stack with argument values (INNER/CALL entries) |
| `-errorline` option | Supported | Line number where error occurred |
| Global `::errorInfo` variable | Supported | Set on error |
| Global `::errorCode` variable | Supported | Set on error (defaults to NONE) |

### Options Dictionary Contents

When an error occurs, the options dictionary contains:

```tcl
proc foo {} { error "something broke" }
catch {foo} msg opts
# opts contains:
#   -code 1
#   -level 0
#   -errorinfo "something broke\n    while executing..."
#   -errorstack {INNER {...} CALL {foo}}
#   -errorline N
#   -errorcode NONE
```

For successful execution:

```tcl
catch {set x 1} msg opts
# opts contains:
#   -code 0
#   -level 0
```

### Global Variables

On error, `catch` automatically sets:
- `::errorInfo` - Same content as `-errorinfo` option
- `::errorCode` - Same content as `-errorcode` option (defaults to NONE)

### Custom Error Codes

The `-errorcode` option preserves custom error codes set via `return -code error -errorcode`:

```tcl
proc foo {} { return -code error -errorcode {POSIX ENOENT} "not found" }
catch {foo} msg opts
dict get $opts -errorcode  ;# Returns: POSIX ENOENT
```

## TCL Features We Do NOT Support

*All major catch features are now fully supported.*

## Notes on Implementation Differences

1. **Script Evaluation Context:** Our implementation uses `TCL_EVAL_LOCAL` flag when evaluating the script. This matches typical TCL behavior where catch evaluates in the caller's scope.

2. **Return Code Unwrapping:** We correctly handle the TCL_RETURN case by examining `-code` and `-level` options and decrementing the level. When level reaches 0, we use the actual `-code` value as the return code.

3. **Options Dictionary Format:** TCL returns options as a dictionary (key-value pairs). Our implementation creates a list with alternating keys and values, which is compatible with TCL's dict representation.

4. **Error Finalization:** We call `feather_error_finalize()` before getting options, which transfers accumulated trace information from `::tcl::errors` namespace to the return options.

5. **Custom Return Codes:** TCL allows packages and scripts to return custom integer return codes outside the reserved range (0-4). Our implementation handles these correctly since we treat the code as a generic integer.

## Implementation Details

### Error State Management

Error state is tracked in the `::tcl::errors` namespace with these variables:
- `active` - Flag indicating error is propagating
- `info` - Accumulated errorinfo string
- `stack` - Accumulated errorstack list
- `line` - Error line number

When `catch` captures an error:
1. `feather_error_finalize()` is called to transfer accumulated state to return options
2. `-errorinfo`, `-errorstack`, `-errorline` are added from accumulated state
3. `-errorcode` is added (from return options or defaults to NONE)
4. Global `::errorInfo` and `::errorCode` are set
5. Error state is cleared (active = 0)
