# catch

Catch errors from script evaluation.

## Syntax

```tcl
catch script ?resultVar? ?optionsVar?
```

## Parameters

- **script**: The script to evaluate
- **resultVar**: Variable to store the result or error message (optional)
- **optionsVar**: Variable to store options dictionary (optional)

## Return Codes

| Code | Meaning |
|------|---------|
| 0 | TCL_OK - Normal completion |
| 1 | TCL_ERROR - Error occurred |
| 2 | TCL_RETURN - return command |
| 3 | TCL_BREAK - break command |
| 4 | TCL_CONTINUE - continue command |

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicErrorCatching = `# Basic error catching
set code [catch {expr {1 / 0}} result]
puts "Return code: $code"
puts "Result: $result"

# Check if command exists
if {[catch {unknownCmd}]} {
    puts "Command not found"
}

# Safe division
proc safeDiv {a b} {
    if {[catch {expr {$a / $b}} result]} {
        return "Error: division failed"
    }
    return $result
}

puts [safeDiv 10 2]
puts [safeDiv 10 0]`
</script>

<FeatherPlayground :code="basicErrorCatching" />

## See Also

- [try](./try)
- [throw](./throw)
- [error](./error)
