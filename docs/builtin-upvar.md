# upvar Builtin Comparison

This document compares feather's implementation of the `upvar` command with the official TCL specification.

## Summary of Our Implementation

Our `upvar` implementation in `src/builtin_upvar.c` provides the core functionality of creating links to variables in different stack frames. The command:

1. Accepts an optional level argument (defaults to 1, meaning caller's frame)
2. Supports both relative levels (e.g., `1`, `2`) and absolute levels (e.g., `#0` for global)
3. Processes pairs of `otherVar localVar` arguments
4. Creates variable links using `ops->var.link()`
5. Returns an empty string on success

The level parsing logic correctly handles TCL's rule: the first argument is only consumed as a level if doing so leaves an even number of remaining arguments (to form pairs).

## TCL Features We Support

- **Basic variable linking**: Creating links from local variables to variables in enclosing frames
- **Default level**: Level defaults to 1 (caller's frame) when omitted
- **Relative levels**: Numeric levels like `1`, `2`, etc. that reference frames relative to current
- **Absolute levels**: `#N` syntax for absolute frame references (e.g., `#0` for global level)
- **Multiple variable pairs**: Processing multiple `otherVar localVar` pairs in a single command
- **Lazy variable creation**: The `otherVar` need not exist at the time of the call; it will be created when `myVar` is first referenced
- **Empty return value**: Returns empty string on success

## TCL Features We Do NOT Support

### 1. Array Element References for otherVar

TCL allows `otherVar` to refer to an array element:

```tcl
upvar 1 myArray(key) localVar
```

Our implementation does not appear to have explicit array element handling. The `ops->var.link()` host operation would need to support parsing and linking to array elements.

### 2. Error on Array-like myVar

TCL explicitly returns an error if `myVar` looks like an array element (e.g., `a(b)`). From the manual:

> "An error is returned if the name looks like an array element, such as a(b)."

Our implementation does not validate that `localVar` is not an array element name.

### 3. Error if myVar Already Exists

TCL states:

> "There must not exist a variable by the name myVar at the time upvar is invoked."

Our implementation does not check whether the local variable already exists before creating the link.

### 4. Variable Traces Interaction

TCL has detailed semantics for how `upvar` interacts with variable traces:

- Traces defined on `otherVar` are triggered by actions on `myVar`
- The trace procedure receives the name of `myVar`, not `otherVar`
- Array element names are passed as the second argument to trace procedures

Our implementation does not appear to have trace support.

### 5. Unset Behavior

TCL specifies that:

> "If an upvar variable is unset, the unset operation affects the variable it is linked to, not the upvar variable. There is no way to unset an upvar variable except by exiting the procedure in which it is defined."

The behavior of `unset` on upvar variables in our implementation depends on the host's `ops->var.link()` semantics.

### 6. Retargeting an Upvar Variable

TCL allows executing another `upvar` command to retarget an existing upvar variable to a different target:

> "However, it is possible to retarget an upvar variable by executing another upvar command."

This may or may not work in our implementation depending on how the host handles repeated `ops->var.link()` calls on the same local variable.

### 7. Namespace Eval Interaction

TCL specifies that `namespace eval` adds call frames to the stack:

> "namespace eval is another way (besides procedure calls) that the Tcl naming context can change. It adds a call frame to the stack to represent the namespace context."

If feather supports `namespace eval`, the level counting needs to account for namespace frames. This is not explicitly handled in our implementation.

## Notes on Implementation Differences

### Level Detection Heuristic

Our implementation uses a specific heuristic for level detection (lines 41-62):

1. Only consider the first argument as a level if consuming it would leave an even number of remaining arguments
2. Check if the first argument starts with `#` or consists of pure digits
3. If it looks like a level, attempt to parse it

This matches TCL's behavior for distinguishing between level arguments and variable names.

### Error Messages

Our error message format matches TCL's expected format:

```
wrong # args: should be "upvar ?level? otherVar localVar ?otherVar localVar ...?"
```

### Level Parsing

The level parsing in `src/level_parse.c` correctly handles:
- Absolute levels (`#N`) with validation against stack size
- Relative levels with validation that the level doesn't exceed current frame depth
- Appropriate error messages for bad levels
