# error

Raise an error with a message.

## Syntax

```tcl
error message ?info? ?code?
```

## Parameters

- **message**: The error message string
- **info**: Initial stack trace information (optional)
- **code**: Machine-readable error code (optional)

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const errorHandling = `proc requirePositive {value} {
    if {$value < 0} {
        error "Value must be positive, got $value"
    }
    return $value
}

catch {requirePositive -5} msg
puts "Error: $msg"`

const errorWithCode = `# Error with code for programmatic handling
proc openFile {filename} {
    if {$filename eq ""} {
        error "Filename required" "" {FILE MISSING_NAME}
    }
    # File operations would go here
    return "opened $filename"
}

try {
    openFile ""
} trap {FILE MISSING_NAME} {msg} {
    puts "Missing filename: $msg"
} on error {msg} {
    puts "Error: $msg"
}`
</script>

<FeatherPlayground :code="errorHandling" />

<FeatherPlayground :code="errorWithCode" />

## See Also

- [throw](./throw)
- [catch](./catch)
- [try](./try)
