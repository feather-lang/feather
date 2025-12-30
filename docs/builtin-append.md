# append Builtin Command

## Summary of Our Implementation

Our implementation of `append` is located in `src/builtin_append.c`. It:

1. Validates that at least one argument (the variable name) is provided
2. Retrieves the current value of the variable (or empty string if unset)
3. Concatenates all provided values to the current value
4. Stores the result back in the variable
5. Returns the new value as the command result

The implementation uses `feather_get_var` and `feather_set_var` which handle qualified variable names and fire read/write traces.

## TCL Features We Support

- **Basic append operation**: Appending one or more values to a variable
- **Variable creation**: If the variable does not exist, it is created with the concatenated values
- **Multiple values**: Multiple values can be appended in a single call (`append var a b c`)
- **Variable traces**: Read and write traces are fired via `feather_get_var` and `feather_set_var`
- **Qualified variable names**: Namespace-qualified names (e.g., `::foo::bar`) are supported
- **Return value**: The new value is returned as the command result

## TCL Features We Do NOT Support

- **Array default values**: TCL 9 supports arrays with default values. When appending to a non-existent array element where the array has a default value set, TCL concatenates the default value with the appended values. Our implementation does not have this feature as it relies on the basic variable get/set operations.

## Notes on Implementation Differences

1. **Error message format**: Our error message `"wrong # args: should be \"append varName ?value ...?\""` matches the standard TCL format.

2. **Efficiency claim**: TCL documentation notes that `append a $b` is more efficient than `set a $a$b` for long strings because it can potentially modify the string in-place. Our implementation uses `ops->string.concat()` which may or may not have this optimization depending on the host implementation.

3. **Empty value handling**: When no values are provided (`append varName` with no additional arguments), our implementation:
   - Gets the current value (or empty string if unset)
   - Appends nothing (zero iterations of the loop)
   - Stores and returns the unchanged/empty value

   This matches TCL behavior where `append varName` returns the current value or creates the variable with an empty string.
