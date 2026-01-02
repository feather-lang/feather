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
- **Qualified names for cross-namespace access**: Supports qualified names like `::ns::varname` to link to variables in other namespaces

## TCL Features We Support

### Qualified names for accessing other namespaces' variables

**Implemented.** TCL allows using qualified names like `variable ::someNS::varname` inside a procedure to create a local link to a variable in a different namespace. Our implementation now supports this:

```tcl
namespace eval ns1 {
    variable myvar "original"
}

proc accessOther {} {
    variable ::ns1::myvar  ;# Creates local link 'myvar' to ::ns1::myvar
    return $myvar
}
```

If the target namespace doesn't exist, an error is raised: `can't access "::nonexistent::x": parent namespace doesn't exist`

## TCL Features We Do NOT Support

1. **Creating undefined variables**
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

Our implementation now supports qualified names when creating local links:
```tcl
proc spong {} {
    # Variable in another namespace
    variable ::someNS::someAry
    parray someAry
}
```
This creates a local variable `someAry` linked to `::someNS::someAry`.

### Namespace Context
Our implementation uses `ops->ns.current(interp)` to get the current namespace and `ops->var.link_ns()` to create links. This appears to correctly handle the basic case of linking local variables to namespace variables within the same namespace.
