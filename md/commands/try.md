# try

Advanced exception handling with multiple handlers. Feather's `try` implementation is fully compatible with TCL 8.6+.

## Syntax

```tcl
try body ?handler...? ?finally script?
```

Where handlers can be:
- `on code varList script` - matches specific return codes
- `trap pattern varList script` - matches error codes by prefix

## Parameters

- **body**: The script to execute
- **on code varList script**: Handle specific return code
- **trap pattern varList script**: Handle errors matching errorcode pattern
- **finally script**: Script that always executes (regardless of body/handler outcome)

## Return Codes

The `on` handler matches these completion codes:

| Code | Name | Meaning |
|------|------|---------|
| 0 | ok | Normal completion |
| 1 | error | Error occurred |
| 2 | return | return command |
| 3 | break | break command |
| 4 | continue | continue command |

Integer code values are also supported.

## Trap Pattern Matching

The `trap` handler matches errors by `-errorcode` prefix:
- Empty pattern `{}` matches all errors
- Pattern `{A B}` matches errorcode `{A B}` or `{A B C}` (prefix matching)
- First matching handler is selected; `on error` will mask subsequent `trap` handlers

## Handler Variables

The `varList` can be:
- Empty: no variables bound
- One variable: receives the result or error message
- Two variables: receives result and options dictionary

Use `-` as script to fall through to next handler. The first matching handler's varList is used for variable binding.

## Finally Clause

- Always executed regardless of body/handler outcome
- If finally raises an error, it overrides previous result
- Otherwise, preserves the result from body or handler

## Exception Context

If an exception occurs during a handler or finally clause, the original exception's status dictionary is added to the new exception under the `-during` key. This preserves context about what exception was being handled when the new error occurred.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicTryWithErrorHandling = `# Basic try with error handling
try {
    set x [expr {10 / 0}]
} on error {msg} {
    puts "Caught error: $msg"
} finally {
    puts "Cleanup complete"
}`

const usingTrapForSpecificErrorTypes = `# Using trap for specific error types
proc divide {a b} {
    if {$b == 0} {
        throw {ARITH DIVZERO} "division by zero"
    }
    return [expr {$a / $b}]
}

try {
    divide 10 0
} trap {ARITH DIVZERO} {msg} {
    puts "Division by zero: $msg"
} trap {ARITH} {msg} {
    puts "Arithmetic error: $msg"
} on error {msg} {
    puts "Other error: $msg"
}`
</script>

<WasmPlayground :tcl="basicTryWithErrorHandling" />

<WasmPlayground :tcl="usingTrapForSpecificErrorTypes" />

## See Also

- [catch](./catch)
- [throw](./throw)
- [error](./error)

