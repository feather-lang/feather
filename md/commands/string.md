# string

Perform various operations on strings.

## Syntax

```tcl
string subcommand arg ?arg ...?
```

## Subcommands

| Subcommand | Description |
|------------|-------------|
| `string length string` | Returns the number of characters in the string |
| `string index string charIndex` | Returns the character at the specified index |
| `string range string first last` | Returns a substring from first to last index |
| `string match ?-nocase? pattern string` | Returns 1 if string matches the glob pattern |
| `string toupper string ?first? ?last?` | Converts to uppercase (optionally only a range) |
| `string tolower string ?first? ?last?` | Converts to lowercase (optionally only a range) |
| `string trim string ?chars?` | Removes characters from both ends |
| `string trimleft string ?chars?` | Removes characters from the left |
| `string trimright string ?chars?` | Removes characters from the right |
| `string map ?-nocase? mapping string` | Replaces substrings according to mapping |

## Parameters

- **string**: The string to operate on
- **charIndex**: Zero-based character position (or `end`, `end-N`)
- **first**, **last**: Range indices for substring operations
- **pattern**: Glob pattern with `*`, `?`, and `[chars]` wildcards
- **chars**: Characters to trim (defaults to whitespace)
- **mapping**: List of old/new pairs for replacement

## Examples

### string length

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const stringLength = `puts [string length "Hello, World!"]`

const stringIndex = `puts [string index "Feather" 0]
puts [string index "Feather" end]
puts [string index "Feather" end-1]`

const stringRange = `puts [string range "Hello, World!" 0 4]
puts [string range "Hello, World!" 7 end]`

const stringMatch = `puts [string match "*.txt" "readme.txt"]
puts [string match "*.txt" "readme.md"]
puts [string match -nocase "HELLO" "hello"]`

const stringToupperTolower = `puts [string toupper "hello"]
puts [string tolower "WORLD"]
puts [string toupper "hello" 0 0]`

const stringTrim = `puts ">[string trim "  hello  "]<"
puts ">[string trimleft "  hello  "]<"
puts ">[string trimright "  hello  "]<"
puts [string trim "xxhelloxx" "x"]`

const stringMap = `puts [string map {foo bar baz qux} "foo and baz"]
puts [string map -nocase {hello hi} "Hello World"]`
</script>

<WasmPlayground :tcl="stringLength" />

### string index

<WasmPlayground :tcl="stringIndex" />

### string range

<WasmPlayground :tcl="stringRange" />

### string match

<WasmPlayground :tcl="stringMatch" />

### string toupper / tolower

<WasmPlayground :tcl="stringToupperTolower" />

### string trim

<WasmPlayground :tcl="stringTrim" />

### string map

<WasmPlayground :tcl="stringMap" />

## See Also

- [concat](./concat) - Concatenate strings
- [join](./join) - Join list elements
- [split](./split) - Split string into list
- [format](./format) - Format strings

