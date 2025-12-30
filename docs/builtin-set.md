# set

Read and write variables.

## Summary of Our Implementation

The `set` command in feather implements basic variable reading and writing:

```tcl
set varName ?newValue?
```

- With one argument: returns the current value of the variable
- With two arguments: sets the variable to the new value and returns it
- Creates a new variable if one does not already exist

Our implementation supports:
- Namespace-qualified variable names (e.g., `::foo::bar`)
- Frame-local (procedure-local) variables for unqualified names
- Variable traces (read and write traces are fired appropriately)

## TCL Features We Support

1. **Basic get/set operations**: Reading and writing scalar variables works as expected.

2. **Namespace-qualified names**: Variables can be accessed using namespace qualifiers like `::foo::bar`. The resolution follows standard TCL namespace rules.

3. **Error handling**: Proper error messages are returned when:
   - Wrong number of arguments: `wrong # args: should be "set varName ?newValue?"`
   - Reading non-existent variable: `can't read "varName": no such variable`

4. **Variable traces**: Both read and write traces are fired via `feather_fire_var_traces()`.

5. **Variable creation**: Setting a non-existent variable creates it automatically.

## TCL Features We Do NOT Support

1. **Array element syntax**: TCL's `set` command supports array notation where `set arr(index) value` sets an array element. The parenthesis syntax `varName(index)` is parsed specially in standard TCL:
   - Characters before the first `(` are the array name
   - Characters between `(` and `)` are the index

   Our implementation does not parse this syntax; a variable name like `arr(foo)` would be treated as a literal scalar variable name rather than accessing element `foo` of array `arr`.

2. **Array-related features**: Since arrays are not implemented, the following are also unavailable:
   - Array variable traces (traces on array elements)
   - Array element iteration via `array` command
   - Dynamic array element access like `set anAry($elemName) value`

## Notes on Implementation Differences

1. **Variable resolution**: Our implementation uses `feather_obj_resolve_variable()` to split qualified names into namespace and local parts. For unqualified names, frame-local storage is used; for qualified names, namespace storage is used.

2. **Trace ordering**: Write traces are fired after the value is set, and read traces are fired after the value is retrieved. This matches standard TCL behavior.

3. **Return value**: Both get and set operations set the interpreter result to the value (via `ops->interp.set_result()`), which becomes the command's return value.

4. **No `global`, `variable`, or `upvar` integration in `set` itself**: The `set` command relies on the variable resolution done by `feather_get_var` and `feather_set_var`, which handle namespace lookups but not the special binding created by `global`, `variable`, or `upvar`. These commands may need separate support in the variable system.
