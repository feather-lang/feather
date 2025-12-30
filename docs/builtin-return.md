# Feather `return` Builtin Documentation

This document compares our implementation of the `return` builtin command with the TCL 8.5+ specification.

## Summary of Our Implementation

Our implementation in `src/builtin_return.c` provides basic `return` functionality with support for:

- Returning a result value (or empty string if not provided)
- The `-code` option with named codes (`ok`, `error`, `return`, `break`, `continue`) and integer values
- The `-level` option to control which stack level the return code applies to
- The `-options` option (parsed but currently ignored/consumed without processing)
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

## TCL Features We Do NOT Support

### 1. The `-errorcode` Option

TCL supports `-errorcode list` which provides a machine-readable error code when `-code error` is used. This is stored in the global variable `errorCode`. Our implementation does not support this option.

**TCL behavior:**
```tcl
return -code error -errorcode {POSIX ENOENT "No such file"} "File not found"
```

### 2. The `-errorinfo` Option

TCL supports `-errorinfo info` which provides stack trace information when `-code error` is used. This is stored in the global variable `errorInfo`. Our implementation does not support this option.

**TCL behavior:**
```tcl
return -code error -errorinfo "Error in myProc\n    at line 5" "Something failed"
```

### 3. The `-errorstack` Option (TCL 8.6+)

TCL supports `-errorstack list` which records actual argument values passed to each proc level during errors. Our implementation does not support this option.

### 4. Proper `-options` Dictionary Processing

While our implementation accepts the `-options` flag and consumes its value, it does not actually process the dictionary entries. In TCL, `-options dictionary` should merge all entries from the dictionary as additional option-value pairs.

**TCL behavior:**
```tcl
# Re-raise a caught error with its original options
catch { ... } result opts
return -options $opts $result
```

**Our behavior:** The `-options` value is consumed but ignored.

### 5. Arbitrary Return Options

TCL allows any option-value pairs in the return options dictionary, not just the recognized ones. These become available through `catch`. Our implementation only accepts `-code`, `-level`, and `-options`.

**TCL behavior:**
```tcl
return -myoption myvalue "result"  ;# Allowed in TCL
```

**Our behavior:** Returns error "bad option \"-myoption\""

### 6. Multiple Result Arguments

TCL concatenates multiple trailing arguments with spaces. Our implementation does support this, but the behavior should be verified for edge cases.

## Notes on Implementation Differences

### Error Message Format

Our error messages closely match TCL's format:
- `bad completion code "xyz": must be ok, error, return, break, continue, or an integer`
- `bad option "-xyz": must be -code, -level, or -options`

### Return Options Storage

We store return options as a list `{-code X -level Y}` using `ops->interp.set_return_options()`. TCL stores these as a proper dictionary with additional entries like `-errorcode` and `-errorinfo` when applicable.

### Level Processing

Our level handling matches TCL:
- Level 0: The code takes effect immediately (return code is the `-code` value)
- Level >= 1: Returns TCL_RETURN, and the procedure invocation mechanism decrements the level

### Integer Code Range

TCL documentation recommends applications use values 5-1073741823 (0x3fffffff) for custom return codes. Our implementation accepts any integer value without range validation.

## Recommendations for Future Work

1. **High Priority:** Implement `-options` dictionary processing for proper error re-raising support
2. **Medium Priority:** Add `-errorcode` support for machine-readable error codes
3. **Medium Priority:** Add `-errorinfo` support for custom stack traces
4. **Low Priority:** Allow arbitrary option-value pairs in return options
5. **Low Priority:** Add `-errorstack` support (TCL 8.6+ feature)
