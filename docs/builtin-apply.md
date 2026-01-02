# Builtin: apply

## Summary of Our Implementation

The `apply` command in feather applies an anonymous function (lambda expression) to a set of arguments. Our implementation is located in `src/builtin_apply.c`.

Our implementation supports:

- **Lambda expression format**: Both 2-element `{args body}` and 3-element `{args body namespace}` forms
- **Required parameters**: Simple parameter names that must be provided
- **Optional parameters with defaults**: Parameters specified as `{name default}` pairs
- **Variadic parameters**: The special `args` parameter that collects remaining arguments into a list
- **Namespace support**: When a namespace is provided, the lambda body executes in that namespace
- **Proper call frame management**: Creates a new call frame with line number tracking and lambda expression storage
- **Return option handling**: Properly handles `-code` and `-level` return options for multi-level returns

## TCL Features We Support

### Lambda Expression Structure
- Two-element list: `{args body}` - arguments and body
- Three-element list: `{args body namespace}` - arguments, body, and namespace context

### Parameter Types
1. **Required parameters**: `{x y}` - simple names that must have values provided
2. **Optional parameters with defaults**: `{{x default}}` - if not provided, uses default value
3. **Variadic args**: `args` as the last parameter collects remaining arguments into a list

### Namespace Handling
- Namespace is interpreted relative to global namespace
- Non-absolute namespaces (not starting with `::`) are prefixed with `::`
- Namespace is created if it does not exist

### Call Frame Semantics
- Adds a call frame to the evaluation stack
- Local variables are created for each parameter
- Body is executed in the new call frame

### Error Handling
- Validates lambda expression has 2 or 3 elements
- Validates argument count matches parameter requirements
- Provides descriptive error messages matching TCL format

## TCL Features We Support

### Required Arguments After Optional Arguments

**Implemented.** The TCL manual states:

> "Arguments with default values that are followed by non-defaulted arguments become required arguments; enough actual arguments must be supplied to allow all arguments up to and including the last required formal argument."

Our implementation correctly enforces this rule. A parameter list like `{{x 1} y}` requires both arguments because the optional `x` becomes required due to required `y` following it.

```tcl
apply {{{x 1} y} {list $x $y}} 10       ;# Error: wrong # args
apply {{{x 1} y} {list $x $y}} 5 10     ;# Returns "5 10"
```

Note: The `args` parameter does NOT make preceding optionals required:

```tcl
apply {{{x 1} args} {list $x $args}}        ;# OK: returns "1 {}"
apply {{{x 1} args} {list $x $args}} 10     ;# OK: returns "10 {}"
```

## TCL Features We May Not Fully Support

### Variable Access Commands

Our implementation creates local variables but the manual notes that certain commands are needed to access non-local variables:

- `global` - for accessing global variables
- `upvar` - for accessing variables in calling frames
- `variable` - for accessing namespace variables

These commands may or may not be fully implemented in feather (depending on overall interpreter status), but the `apply` builtin itself does not directly provide this functionality - it depends on these other commands being available.

## Notes on Implementation Differences

### Namespace Creation

Our implementation automatically creates the namespace if it does not exist (`ops->ns.create`). The TCL documentation does not explicitly state whether this happens automatically, but our behavior ensures the namespace is available for the lambda execution.

### Error Message Format

Our error messages closely follow TCL's format:
- `wrong # args: should be "apply lambdaExpr ?arg ...?"` for missing lambda
- `can't interpret "..." as a lambda expression` for malformed lambda
- `wrong # args: should be "apply lambdaExpr ..."` for argument count mismatch

### Return Handling

Our implementation handles `return -code` and `return -level` options, decrementing the level and either returning the code directly (when level reaches 0) or propagating the return with adjusted options.

### Lambda Expression Storage

We store the lambda expression in the call frame via `ops->frame.set_lambda()` for debugging/introspection purposes. This is an implementation detail not specified in TCL but useful for stack traces.
