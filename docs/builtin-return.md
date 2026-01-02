# Feather `return` Builtin Documentation

This document compares our implementation of the `return` builtin command with the TCL 8.5+ specification.

## Summary of Our Implementation

Our implementation in `src/builtin_return.c` provides full `return` functionality with support for:

- Returning a result value (or empty string if not provided)
- The `-code` option with named codes (`ok`, `error`, `return`, `break`, `continue`) and integer values
- The `-level` option to control which stack level the return code applies to
- The `-options` option for extracting all standard options from a dictionary
- The `-errorcode` option for machine-readable error codes
- The `-errorinfo` option for custom stack traces
- The `-errorstack` option for call stack information with automatic generation
- Proper level-based return code handling (level 0 returns code directly, level > 0 returns TCL_RETURN)
- Building a return options dictionary with all set options
- Arbitrary user-defined options

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

9. **Arbitrary return options**
   - Any `-option value` pairs are accepted, not just the known options
   - Custom options are stored in the return options dictionary
   - They become available through `catch {cmd} result opts`
   - Custom options are preserved through error re-raising via `-options`
   - Example:
     ```tcl
     proc test {} {
         return -custom "mydata" -another 42 "result"
     }
     catch {test} msg opts
     dict get $opts -custom  ;# Returns "mydata"
     ```

10. **The `-errorstack` option**
    - Accepts `-errorstack` as a command line option
    - Stores the value in the return options dictionary
    - Preserved through error re-raising via `-options`
    - **Automatic generation:** When errors propagate through procs, `-errorstack` is
      automatically populated with `INNER` and `CALL` entries containing proc names
      and actual argument values
    - Example of automatic generation:
      ```tcl
      proc inner {x y} { error "broke" }
      proc outer {a} { inner $a [expr {$a * 2}] }
      catch {outer 5} msg opts
      dict get $opts -errorstack
      # Returns: INNER {error broke} CALL {inner 5 10} CALL {outer 5}
      ```

## TCL Features We Do NOT Support

*All major return features are now fully supported.*

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

## Implementation Notes

### `-options` Processing

The `-options` option extracts `-code`, `-level`, `-errorcode`, and `-errorinfo` values from a dictionary. Any other options in the dictionary (custom options) are preserved and passed through to the return options dictionary.

The order of option processing matters: options appearing later in the command line override earlier values. This is important for the error re-raising pattern where `return -options $opts $result` should preserve the original error's code, level, and any custom options.
