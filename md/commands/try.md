# try

Advanced exception handling with multiple handlers.

## Syntax

```tcl
try body ?on code varList script ...? ?trap pattern varList script ...? ?finally script?
```

## Parameters

- **body**: The script to execute
- **on code varList script**: Handle specific return code
- **trap pattern varList script**: Handle errors matching errorcode pattern
- **finally script**: Script that always executes

## Handler Variables

The `varList` receives up to two values:
1. The result or error message
2. The options dictionary

Use `-` as script to fall through to next handler.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="basicTryWithErrorHandling" />

<FeatherPlayground :code="usingTrapForSpecificErrorTypes" />

## See Also

- [catch](./catch)
- [throw](./throw)
- [error](./error)
