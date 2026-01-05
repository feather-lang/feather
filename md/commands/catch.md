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

## Options Dictionary

When using the `optionsVar` parameter, `catch` populates it with a dictionary containing detailed information about the script execution:

### For Successful Execution

| Key | Description |
|-----|-------------|
| `-code` | The return code (0 for OK) |
| `-level` | Return level (0 for normal) |

### For Errors

| Key | Description |
|-----|-------------|
| `-code` | The return code (1 for ERROR) |
| `-level` | Return level (0 for normal) |
| `-errorinfo` | Human-readable stack trace |
| `-errorcode` | Machine-readable error code (defaults to NONE) |
| `-errorstack` | Call stack with argument values (INNER/CALL entries) |
| `-errorline` | Line number where error occurred |

## Global Variables

On error, `catch` automatically sets these global variables:

- `::errorInfo` - Same content as `-errorinfo` option
- `::errorCode` - Same content as `-errorcode` option (defaults to NONE)

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

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

const optionsDictExample = `# Using the options dictionary
proc foo {} { error "something broke" }

catch {foo} msg opts
puts "Message: $msg"
puts "Return code: [dict get $opts -code]"
puts "Error code: [dict get $opts -errorcode]"

# Successful execution
catch {set x 1} msg2 opts2
puts "\\nSuccess result: $msg2"
puts "Success code: [dict get $opts2 -code]"`

const customErrorCodes = `# Custom error codes with return
proc openFile {filename} {
    if {$filename eq ""} {
        return -code error -errorcode {POSIX ENOENT} "file not found"
    }
    return "opened $filename"
}

catch {openFile ""} msg opts
puts "Error: $msg"
puts "Error code: [dict get $opts -errorcode]"`
</script>

<WasmPlayground :tcl="basicErrorCatching" />

<WasmPlayground :tcl="optionsDictExample" />

<WasmPlayground :tcl="customErrorCodes" />

## See Also

- [try](./try)
- [throw](./throw)
- [error](./error)
