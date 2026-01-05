# throw Builtin Command

Generate a machine-readable error.

## Synopsis

```
throw <type> <message>
```

## Description

This command causes the current evaluation to be unwound with an error. The error created is described by the `type` and `message` arguments:

- **type** - A list of words describing the error in a form that is machine-readable (forms the error-code part of the result dictionary)
- **message** - Text intended for display to a human being

The stack will be unwound until the error is trapped by a suitable `catch` or `try` command.

By convention, the words in the type argument should go from most general to most specific.

## Arguments

| Argument | Description |
|----------|-------------|
| `<type>` | A non-empty list of words classifying the error. Convention suggests ordering from general to specific (e.g., `{ARITH DIVZERO}`). |
| `<message>` | Human-readable error message describing what went wrong. |

## Examples

Throw an arithmetic division-by-zero error:

```tcl
throw {ARITH DIVZERO} "division by zero"
```

Use throw in a procedure to signal invalid input:

```tcl
proc divide {a b} {
    if {$b == 0} {
        throw {ARITH DIVZERO} "cannot divide by zero"
    }
    expr {$a / $b}
}
```

Throw and catch a custom error type:

```tcl
try {
    throw {MYAPP NOTFOUND} "resource not found"
} trap {MYAPP NOTFOUND} err {
    puts "Caught: $err"
}
```

## See Also

catch, error, return, try

## Implementation Notes

Our implementation in `src/builtin_throw.c` is **feature-complete** with respect to the TCL `throw` command specification:

- Accepts exactly two arguments: `type` and `message`
- Validates that `type` is a non-empty list
- Sets up return options with `-code 1` and `-errorcode` containing the type
- Sets the error message as the interpreter result
- Integrates with `error_trace.h` for stack trace support

### Differences from TCL

1. **Type validation error**: When the type list is empty, we return `"type must be non-empty"`. TCL's exact error message may differ slightly.

2. **Error trace integration**: Our implementation integrates with `error_trace.h` for stack trace support, which aligns with TCL 8.6+'s `-errorinfo` functionality.
