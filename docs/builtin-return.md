# Feather `return` Builtin Documentation

This document compares our implementation of the `return` builtin command with the TCL 8.5+ specification.

## Summary of Our Implementation

Our implementation in `src/builtin_return.c` provides basic `return` functionality with support for:

- Returning a result value (or empty string if not provided)
- The `-code` option with named codes (`ok`, `error`, `return`, `break`, `continue`) and integer values
- The `-level` option to control which stack level the return code applies to
- The `-options` option for extracting `-code`, `-level`, `-errorcode`, and `-errorinfo` from a dictionary
- The `-errorcode` option for machine-readable error codes
- The `-errorinfo` option for custom stack traces
- Proper level-based return code handling (level 0 returns code directly, level > 0 returns TCL_RETURN)
- Building a return options dictionary with `-code` and `-level` entries

## TCL Features We Support

### Fully Supported

1. **Basic return with optional result**
   - `return` - returns empty string
   - `return value` - returns the specified value

2. **The `-code` option**
   - Named codes: `ok`, `error`, `return`, `break`, `continue`
   - Integer codes (0-4 and custom values)
   - Proper error messages for invalid codes

3. **The `-level` option**
   - Non-negative integer values
   - Level 0: code takes effect immediately
   - Level 1 (default): return code applies to enclosing procedure
   - Level > 1: TCL_RETURN propagates up the call stack

4. **Return options dictionary**
   - We build and store a dictionary with `-code` and `-level` entries
   - Uses `interp.set_return_options()` to store the options

5. **The `-options` option**
   - Parses the value as a dictionary
   - Extracts `-code`, `-level`, `-errorcode`, and `-errorinfo` values from the dictionary
   - Order matters: later options override earlier ones
   - Enables the standard error re-raising pattern:
     ```tcl
     catch {command} result opts
     return -options $opts $result
     ```

6. **The `-errorcode` option**
   - Sets a machine-readable error code when `-code error` is used
   - Value is stored in return options dictionary as `-errorcode`
   - Also sets the global `::errorCode` variable
   - Defaults to `NONE` when not specified for error returns
   - Preserved through error re-raising via `-options`
   - Example:
     ```tcl
     return -code error -errorcode {POSIX ENOENT} "File not found"
     ```

7. **The `-errorinfo` option**
   - Sets custom stack trace information when `-code error` is used
   - Value is stored in return options dictionary as `-errorinfo`
   - Also sets the global `::errorInfo` variable
   - Preserved through error re-raising via `-options`
   - Example:
     ```tcl
     return -code error -errorinfo "Custom trace\n    at line 5" "Something failed"
     ```

8. **Multiple trailing arguments**
   - When multiple non-option arguments are provided, only the last one is used as the result
   - Example: `return a b c` returns `c`
   - This matches TCL's behavior

## TCL Features We Do NOT Support

### 1. The `-errorstack` Option (TCL 8.6+)

TCL supports `-errorstack list` which records actual argument values passed to each proc level during errors. Our implementation does not support this option.

### 2. Arbitrary Return Options

TCL allows any option-value pairs in the return options dictionary, not just the recognized ones. These become available through `catch`. Our implementation only accepts `-code`, `-level`, `-errorcode`, `-errorinfo`, and `-options`.

**TCL behavior:**
```tcl
return -myoption myvalue "result"  ;# Allowed in TCL
```

**Our behavior:** Returns error "bad option \"-myoption\""

## Notes on Implementation Differences

### Error Message Format

Our error messages closely match TCL's format:
- `bad completion code "xyz": must be ok, error, return, break, continue, or an integer`
- `bad option "-xyz": must be -code, -level, -errorcode, -errorinfo, or -options`

### Return Options Storage

We store return options as a list `{-code X -level Y -errorcode Z -errorinfo W ...}` using `ops->interp.set_return_options()`. TCL stores these as a proper dictionary with additional entries like `-errorstack` when applicable.

### Level Processing

Our level handling matches TCL:
- Level 0: The code takes effect immediately (return code is the `-code` value)
- Level >= 1: Returns TCL_RETURN, and the procedure invocation mechanism decrements the level

### Integer Code Range

TCL documentation recommends applications use values 5-1073741823 (0x3fffffff) for custom return codes. Our implementation accepts any integer value without range validation.

## Recommendations for Future Work

1. **Low Priority:** Allow arbitrary option-value pairs in return options
2. **Low Priority:** Add `-errorstack` support (TCL 8.6+ feature)

## Implementation Notes

### `-options` Processing

The `-options` option was implemented to extract `-code` and `-level` values from a dictionary. Other options in the dictionary (like `-errorcode`, `-errorinfo`, `-errorstack`) are currently ignored but will be passed through when those features are implemented.

The order of option processing matters: options appearing later in the command line override earlier values. This is important for the error re-raising pattern where `return -options $opts $result` should preserve the original error's code and level.
