# incr Builtin Command

## Summary of Our Implementation

Our implementation of `incr` is located in `src/builtin_incr.c`. It provides the core functionality of incrementing integer variables with an optional increment value.

The command signature is:

```
incr varName ?increment?
```

Key implementation details:
- Uses `feather_get_var` and `feather_set_var` for variable access, which handle qualified names and fire read/write traces
- Requires the variable to exist before incrementing
- Requires both the current value and the increment to be valid integers
- Default increment is 1 when not specified
- Returns the new value as the result

## TCL Features We Support

1. **Basic increment by 1**: `incr x` adds 1 to the variable x
2. **Custom increment value**: `incr x 42` adds 42 to the variable x
3. **Negative increments**: `incr x -5` subtracts 5 from the variable x (works via signed integer)
4. **Zero increment**: `incr x 0` can be used to validate that x contains an integer
5. **Proper error messages**: Returns appropriate error messages for:
   - Wrong number of arguments
   - Variable does not exist
   - Variable value is not an integer
   - Increment value is not an integer
6. **Variable traces**: Uses `feather_get_var` and `feather_set_var` which fire read/write traces
7. **Qualified variable names**: Variable access functions handle namespace-qualified names

## TCL Features We Do NOT Support

1. **Auto-initialization of unset variables**: Starting with TCL 8.5, if the variable does not exist, `incr` will create it and set it to the increment value (or 1 if no increment is specified). Our implementation requires the variable to exist first and returns an error "can't read ... : no such variable" if it does not.

2. **Array default values**: TCL 8.5+ supports array default values. If an array element does not exist but the array has a default value set, TCL will use the sum of the default value and the increment. Our implementation does not support this feature.

## Notes on Implementation Differences

1. **Error on unset variable**: This is the most significant behavioral difference. In standard TCL 8.5+:
   ```tcl
   # Assuming x is unset
   incr x      ;# Sets x to 1, returns 1
   incr x 5    ;# Sets x to 5, returns 5
   ```
   In our implementation, both of these would return an error.

2. **Integer representation**: Our implementation uses 64-bit signed integers (`int64_t`). TCL uses arbitrary precision integers (bignums) for values that exceed the native integer range. Our implementation does not handle overflow conditions explicitly.

3. **Result format**: TCL specifies that "The new value is stored as a decimal string". Our implementation creates an integer object and sets it as the result, relying on the object system's string representation when needed.
