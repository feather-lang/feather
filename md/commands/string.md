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
| `string toupper string` | Converts to uppercase |
| `string tolower string` | Converts to lowercase |
| `string totitle string ?first? ?last?` | Converts to title case (optionally only a range) |
| `string trim string ?chars?` | Removes characters from both ends |
| `string trimleft string ?chars?` | Removes characters from the left |
| `string trimright string ?chars?` | Removes characters from the right |
| `string map ?-nocase? mapping string` | Replaces substrings according to mapping |
| `string cat ?string1? ?string2...?` | Concatenates strings |
| `string compare ?-nocase? ?-length len? string1 string2` | Lexicographic comparison returning -1, 0, or 1 |
| `string equal ?-nocase? ?-length len? string1 string2` | Equality test returning 0 or 1 |
| `string first needleString haystackString ?startIndex?` | Find first occurrence of substring |
| `string last needleString haystackString ?lastIndex?` | Find last occurrence of substring |
| `string repeat string count` | Repeat string N times |
| `string reverse string` | Reverse character order |
| `string insert string index insertString` | Insert substring at index |
| `string replace string first last ?newstring?` | Replace range with optional new string |
| `string is class ?-strict? ?-failindex var? string` | Character/value class testing |

## Parameters

- **string**: The string to operate on
- **charIndex**: Zero-based character position (or `end`, `end-N`)
- **first**, **last**: Range indices for substring operations
- **pattern**: Glob pattern with `*`, `?`, and `[chars]` wildcards
- **chars**: Characters to trim (defaults to whitespace)
- **mapping**: List of old/new pairs for replacement
- **class**: Character or value class for `string is` (see below)

### Supported `string is` Classes

**Character classes:** alnum, alpha, ascii, control, digit, graph, lower, print, punct, space, upper, wordchar, xdigit

**Value classes:** boolean, true, false, integer, double, list, dict

**Options:**
- `-strict` - Empty string returns false (default: empty returns true for character classes)
- `-failindex varname` - Sets variable to index of first non-matching character

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
puts [string totitle "hello world"]`

const stringTrim = `puts ">[string trim "  hello  "]<"
puts ">[string trimleft "  hello  "]<"
puts ">[string trimright "  hello  "]<"
puts [string trim "xxhelloxx" "x"]`

const stringMap = `puts [string map {foo bar baz qux} "foo and baz"]
puts [string map -nocase {hello hi} "Hello World"]`

const stringCat = `puts [string cat "Hello" ", " "World" "!"]`

const stringCompareEqual = `puts [string compare "abc" "abd"]
puts [string compare "abc" "abc"]
puts [string equal "hello" "hello"]
puts [string equal -nocase "Hello" "hello"]`

const stringFirstLast = `puts [string first "o" "Hello, World!"]
puts [string last "o" "Hello, World!"]
puts [string first "or" "Hello, World!"]`

const stringRepeatReverse = `puts [string repeat "ab" 3]
puts [string reverse "Hello"]`

const stringInsertReplace = `puts [string insert "Hello World" 5 ","]
puts [string replace "Hello World" 0 4 "Goodbye"]`

const stringIs = `puts [string is integer "123"]
puts [string is alpha "abc"]
puts [string is digit "123abc"]
puts [string is digit -strict ""]`
</script>

<WasmPlayground :tcl="stringLength" />

### string index

<WasmPlayground :tcl="stringIndex" />

### string range

<WasmPlayground :tcl="stringRange" />

### string match

<WasmPlayground :tcl="stringMatch" />

### string toupper / tolower / totitle

<WasmPlayground :tcl="stringToupperTolower" />

### string trim

<WasmPlayground :tcl="stringTrim" />

### string map

<WasmPlayground :tcl="stringMap" />

### string cat

<WasmPlayground :tcl="stringCat" />

### string compare / equal

<WasmPlayground :tcl="stringCompareEqual" />

### string first / last

<WasmPlayground :tcl="stringFirstLast" />

### string repeat / reverse

<WasmPlayground :tcl="stringRepeatReverse" />

### string insert / replace

<WasmPlayground :tcl="stringInsertReplace" />

### string is

<WasmPlayground :tcl="stringIs" />

## Notes

- `string toupper` and `string tolower` convert the entire string; the optional `?first? ?last?` range arguments are accepted but ignored in the current implementation.
- `string is` supports both character classes (alnum, alpha, etc.) and value classes (integer, double, list, dict, boolean).

## See Also

- [concat](./concat) - Concatenate strings
- [join](./join) - Join list elements
- [split](./split) - Split string into list
- [format](./format) - Format strings

