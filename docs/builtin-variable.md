# `variable` Builtin Comparison

This document compares our implementation of the `variable` builtin command with the official TCL manual.

## Summary of Our Implementation

Our implementation in `src/builtin_variable.c` provides the core functionality of the `variable` command:

1. Accepts name/value pairs to create and initialize namespace variables
2. Creates links from local variables to namespace variables when called inside a procedure
3. Rejects qualified names (names containing `::`) with an appropriate error message
4. Works with the current namespace context
5. Returns an empty result on success

## TCL Features We Support

- **Basic variable creation**: `variable name value` creates a namespace variable with an initial value
- **Multiple name/value pairs**: `variable name1 value1 name2 value2` works correctly
- **Optional final value**: The value for the last variable is optional (e.g., `variable name1 value1 name2`)
- **Local variable linking**: When executed inside a procedure, creates local variables linked to namespace variables
- **Error on qualified names when defining**: Properly rejects qualified names like `ns::varname` with the error message "can't define ... name refers to an element in another namespace"

## TCL Features We Do NOT Support

1. **Qualified names for accessing other namespaces' variables**
   - TCL allows using qualified names like `variable ::someNS::someAry` inside a procedure to create a local link to a variable in a different namespace
   - Our implementation rejects all qualified names with an error
   - This is a significant limitation for accessing variables from other namespaces within procedures

2. **Creating undefined variables**
   - In TCL, `variable name` (without a value) creates the variable in an undefined state
   - Such variables are visible to `namespace which` but not to `info exists`
   - Our implementation may not correctly handle this undefined/exists distinction

3. **Array support considerations**
   - TCL explicitly notes that variable names cannot reference array elements
   - TCL recommends declaring the array variable without a value, then using `array set`
   - We do not have explicit validation to reject array element syntax (e.g., `name(element)`)

## Notes on Implementation Differences

### Error Message Format
Our error message format closely matches TCL:
- Ours: `can't define "ns::var": name refers to an element in another namespace`
- This aligns with standard TCL error conventions

### Qualified Name Handling
TCL's behavior with qualified names is context-dependent:
- **When defining a namespace variable** (inside `namespace eval`): qualified names should work and create the variable in the specified namespace
- **When creating a local link in a procedure**: qualified names create a link to a variable in another namespace

Our implementation treats all qualified names as errors, which is more restrictive than TCL. The TCL manual example shows:
```tcl
proc spong {} {
    # Variable in another namespace
    variable ::someNS::someAry
    parray someAry
}
```
This use case is not supported by our implementation.

### Namespace Context
Our implementation uses `ops->ns.current(interp)` to get the current namespace and `ops->var.link_ns()` to create links. This appears to correctly handle the basic case of linking local variables to namespace variables within the same namespace.
