# catch Command Implementation

## Summary of Our Implementation

Our `catch` command implementation in `src/builtin_catch.c` provides the basic functionality of the TCL `catch` command:

- Syntax: `catch script ?resultVar? ?optionsVar?`
- Evaluates the script in a local context
- Returns an integer return code (0 for OK, 1 for ERROR, 2 for RETURN, etc.)
- Optionally stores the result in `resultVar`
- Optionally stores return options in `optionsVar`
- Handles `TCL_RETURN` by unwrapping `-code` and `-level` options
- Finalizes error state before capturing options (transfers accumulated trace)

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic syntax `catch script` | Supported | Returns return code as integer |
| `resultVar` argument | Supported | Stores result/error message in variable |
| `optionsVar` argument | Supported | Stores return options dictionary |
| Return codes (TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, TCL_CONTINUE) | Supported | Returns 0, 1, 2, 3, 4 respectively |
| `-code` option in options dict | Supported | Set in options dictionary |
| `-level` option handling for TCL_RETURN | Supported | Decrements level and unwraps code |
| Error message in resultVar | Supported | On TCL_ERROR, stores error message |
| Normal result in resultVar | Supported | On TCL_OK, stores return value |

## TCL Features We Do NOT Support

### 1. `-errorinfo` Option

TCL stores a formatted stack trace in the `-errorinfo` entry of the options dictionary when an error occurs. This is meant to be human-readable and shows the context where the error happened.

**TCL behavior:**
```tcl
catch {error "something went wrong"} result opts
dict get $opts -errorinfo
# Returns formatted stack trace
```

**Our implementation:** We call `feather_error_finalize()` but do not appear to populate `-errorinfo` in the options dictionary.

### 2. `-errorcode` Option

TCL stores machine-readable error information as a list in the `-errorcode` entry.

**TCL behavior:**
```tcl
catch {error "msg" {} {POSIX ENOENT}} result opts
dict get $opts -errorcode
# Returns: POSIX ENOENT
```

**Our implementation:** Not populated in options dictionary.

### 3. `-errorline` Option

TCL stores the line number within the script where the error occurred.

**TCL behavior:**
```tcl
catch {
    set x 1
    error "oops"
} result opts
dict get $opts -errorline
# Returns: 3
```

**Our implementation:** Not populated in options dictionary.

### 4. `-errorstack` Option

TCL 8.6+ provides a machine-readable stack trace as an even-sized list of token-parameter pairs. Tokens can be:
- `CALL` - with parameter being a list of proc name and arguments
- `UP` - with parameter being relative level

**TCL behavior:**
```tcl
catch {someProc arg1 arg2} result opts
dict get $opts -errorstack
# Returns: CALL {someProc arg1 arg2} ...
```

**Our implementation:** Not populated in options dictionary.

### 5. Global Variables `::errorInfo` and `::errorCode`

TCL automatically sets these global variables with the most recent error information.

**Our implementation:** These global variables are not automatically set.

### 6. `-level` in Default Options

When `optionsVar` is provided and the return code is not `TCL_RETURN`, TCL always includes both `-code` and `-level` (with `-level` being 0).

**Our implementation:** We only create a default options list with `-code` when options are nil, but do not include `-level 0`.

## Notes on Implementation Differences

1. **Script Evaluation Context:** Our implementation uses `TCL_EVAL_LOCAL` flag when evaluating the script. This matches typical TCL behavior where catch evaluates in the caller's scope.

2. **Return Code Unwrapping:** We correctly handle the TCL_RETURN case by examining `-code` and `-level` options and decrementing the level. When level reaches 0, we use the actual `-code` value as the return code.

3. **Options Dictionary Format:** TCL returns options as a dictionary (key-value pairs). Our implementation creates a list with `-code` and the code value, which is compatible with TCL's dict representation as a list.

4. **Error Finalization:** We call `feather_error_finalize()` before getting options, which transfers accumulated trace information. However, the resulting error information may not be formatted the same way as TCL's `-errorinfo`.

5. **Custom Return Codes:** TCL allows packages and scripts to return custom integer return codes outside the reserved range (0-4). Our implementation should handle these correctly since we treat the code as a generic integer.
