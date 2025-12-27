# throw

Throw an exception with a type and message.

## Syntax

```tcl
throw type message
```

## Parameters

- **type**: Error type as a list (e.g., `{ARITH DIVZERO}`)
- **message**: Human-readable error message

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const bankExample = `proc withdraw {amount balance} {
    if {$amount > $balance} {
        throw {BANK INSUFFICIENT_FUNDS} \
            "Cannot withdraw $amount from balance $balance"
    }
    return [expr {$balance - $amount}]
}

try {
    set newBalance [withdraw 100 50]
} trap {BANK INSUFFICIENT_FUNDS} {msg} {
    puts "Transaction failed: $msg"
}`

const validationExample = `# Hierarchical error types
proc validate {value} {
    if {![string is integer $value]} {
        throw {VALIDATION TYPE} "Expected integer"
    }
    if {$value < 0} {
        throw {VALIDATION RANGE} "Value must be positive"
    }
    return $value
}

try {
    validate "abc"
} trap {VALIDATION TYPE} {msg} {
    puts "Type error: $msg"
} trap {VALIDATION} {msg} {
    puts "Validation error: $msg"
}`
</script>

<FeatherPlayground :code="bankExample" />

<FeatherPlayground :code="validationExample" />

## See Also

- [try](./try)
- [catch](./catch)
- [error](./error)
