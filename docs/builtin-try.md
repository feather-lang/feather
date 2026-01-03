# Feather `try` Builtin Implementation

This document compares the Feather implementation of the `try` command with the official TCL 8.6+ specification.

## Summary of Our Implementation

The Feather `try` command is implemented in `src/builtin_try.c`. It provides structured exception handling with the following syntax:

```tcl
try body ?handler...? ?finally script?
```

Where handlers can be:
- `on code variableList script` - matches specific return codes
- `trap pattern variableList script` - matches error codes by prefix

The implementation supports:
- Body evaluation with result/options capture
- Multiple handler types (on/trap)
- Fallthrough with "-" marker
- Finally clause execution
- Variable binding for result and options
- Return code handling with -code/-level options

## TCL Features We Support

### Core Syntax
- `try body` - basic body evaluation
- `try body finally script` - body with cleanup
- `try body handler... finally script` - full form

### Handler Types

#### `on code variableList script`
- Matches completion codes: `ok` (0), `error` (1), `return` (2), `break` (3), `continue` (4)
- Supports integer code values
- Variable list can be empty, one variable (result), or two variables (result and options)

#### `trap pattern variableList script`
- Matches errors by `-errorcode` prefix
- Empty pattern `{}` matches all errors
- Pattern `{A B}` matches errorcode `{A B C}` or `{A B}`
- Same variable binding as `on`

### Fallthrough
- Script value of `-` causes fallthrough to next handler
- First matching handler's variableList is used for variable binding

### Finally Clause
- Always executed regardless of body/handler outcome
- If finally raises an error, it overrides previous result
- Otherwise, preserves the result from body or handler

### Return Code Handling
- Correctly processes `return -code` and `return -level` options
- Decrements level appropriately per TCL semantics

### `-during` Key in Exception Dictionary
- If an exception (any non-ok result) occurs during evaluation of a handler or finally clause, the original exception's status dictionary is added to the new exception's status dictionary under the `-during` key
- This preserves context about what exception was being handled when the new error occurred
- Supports nested `-during` for multiple levels of exception handling
- Only added when an exception occurs; successful handlers/finally clauses do not add `-during`

## TCL Features We Do NOT Support

None. Feather's `try` implementation is fully compatible with TCL 8.6+.

## Notes on Implementation Differences

### Error Message Formatting
Our error messages closely follow TCL's format for argument count errors and invalid completion codes, ensuring compatibility with existing TCL code that may parse error messages.

### Handler Matching Order
Both implementations match handlers in order, with the first matching handler being selected. This means `on error` will mask any subsequent `trap` handlers, as documented in TCL.

### Options Dictionary
We construct options dictionaries with `-code` and `-errorcode` keys as needed. The format should be compatible with TCL's `return -options` mechanism.

### Inter-word Space Normalization
The TCL manual mentions that inter-word spaces are normalized in both `-errorcode` and pattern before comparison for `trap` handlers. Our implementation relies on list parsing which provides equivalent normalization behavior.

### Return from Try Block
Our implementation correctly handles `return` within a try block, including proper processing of `-level` decrementation. The finally clause is still executed when returning.
