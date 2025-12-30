# lappend Builtin Comparison

This document compares our implementation of `lappend` with the official TCL 9 manual.

## Summary of Our Implementation

Our implementation in `src/builtin_lappend.c` provides the core functionality of `lappend`:

1. Takes a variable name and zero or more values to append
2. If the variable does not exist, creates a new list with the provided values
3. If the variable exists, copies it to a new list and appends the values
4. Stores the result back in the variable
5. Returns the resulting list as the command result

The implementation uses `feather_get_var` and `feather_set_var` which handle qualified variable names and variable traces.

## TCL Features We Support

- **Basic syntax**: `lappend varName ?value value value ...?`
- **Variable creation**: If `varName` does not exist, it is created as a list with the given values
- **Multiple value appending**: All provided values are appended as separate list elements
- **Proper list handling**: Values are appended as list elements (not raw text, unlike `append`)
- **Variable name handling**: Qualified variable names are supported via `feather_get_var`/`feather_set_var`
- **Variable traces**: Read and write traces are fired through the var accessor functions
- **Return value**: The resulting list is returned as the command result

## TCL Features We Do NOT Support

- **Array element default values**: TCL 9 mentions that if `varName` indicates an element that does not exist of an array that has a default value set, a list comprised of the default value with all values appended will be stored. Our implementation does not appear to handle array default values specially.

## Notes on Implementation Differences

1. **Error message format**: Our error message `"wrong # args: should be \"lappend varName ?value ...?\""` matches TCL's conventional format.

2. **Efficiency**: TCL documentation emphasizes that `lappend a $b` is more efficient than `set a [concat $a [list $b]]` when `$a` is long. Our implementation creates a copy of the list via `ops->list.from()` and then appends elements, which should provide similar efficiency benefits by avoiding string concatenation.

3. **List representation**: Our implementation uses `ops->list.from()` to ensure we work with a proper list representation, which handles the shimmering from string to list if needed.

4. **Nil handling**: We explicitly check for nil (unset variable) with `ops->list.is_nil()` and create an empty list in that case, which matches TCL's behavior of creating a new list when the variable does not exist.
