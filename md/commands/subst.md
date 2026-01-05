# subst

Perform substitutions on a string.

## Syntax

```tcl
subst ?options? string
```

## Parameters

- **string**: The string to perform substitutions on

## Options

| Option | Description |
|--------|-------------|
| `-nocommands` | Do not substitute command results (text in `[]`) |
| `-novariables` | Do not substitute variable values (text with `$`) |
| `-nobackslashes` | Do not substitute backslash sequences |

## Description

Performs command substitution, variable substitution, and backslash substitution on the given string, similar to how the Tcl interpreter processes double-quoted strings. The options allow selective disabling of specific substitution types.

Note that `subst` does not give any special treatment to double quotes or curly braces (except within command substitutions) - they are treated as literal characters.

### Variable Substitution

The following variable syntax is supported:
- Simple variables: `$name`
- Braced variables: `${name}`
- Namespace-qualified variables: `$namespace::name`
- Array variables with index: `$name(index)` (the index is also substituted)

### Backslash Escape Sequences

The following backslash escape sequences are supported:
- Standard escapes: `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\\`
- Backslash-newline: `\<newline>` collapses to a single space (including trailing whitespace)
- Hexadecimal: `\xNN` (up to 2 hex digits)
- Octal: `\NNN` (up to 3 octal digits)
- Unicode 16-bit: `\uNNNN` (exactly 4 hex digits, e.g., `\u00A9` produces the copyright symbol)
- Unicode 32-bit: `\UNNNNNNNN` (exactly 8 hex digits, e.g., `\U0001F44B` produces the waving hand emoji)

### Exception Handling in Command Substitution

When a command within `[...]` raises an exception:
- `break` - stops substitution and returns the result up to that point
- `continue` - substitutes an empty string for that command and continues processing
- `return` - substitutes the returned value
- Custom return codes (>= 5) - substitutes the returned value

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const allSubstitutions = `set name "World"
puts [subst {Hello, $name!}]
puts [subst {2 + 2 = [expr {2 + 2}]}]`

const disableCommandSubstitution = `set x 10
puts [subst -nocommands {x=$x, expr=[expr {$x * 2}]}]`

const disableVariableSubstitution = `set name "Alice"
puts [subst -novariables {Hello, $name!}]`

const disableBackslashSubstitution = `puts [subst -nobackslashes {Line1\nLine2}]
puts [subst {Line1\nLine2}]`

const multipleOptions = `set x 5
puts [subst -nocommands -novariables {$x = [expr {$x}]}]`

const buildingDynamicStrings = `set items {apple banana cherry}
set result {}
foreach item $items {
    append result [subst {- $item\n}]
}
puts $result`
</script>

### All substitutions (default)

<WasmPlayground :tcl="allSubstitutions" />

### Disable command substitution

<WasmPlayground :tcl="disableCommandSubstitution" />

### Disable variable substitution

<WasmPlayground :tcl="disableVariableSubstitution" />

### Disable backslash substitution

<WasmPlayground :tcl="disableBackslashSubstitution" />

### Multiple options

<WasmPlayground :tcl="multipleOptions" />

### Building dynamic strings

<WasmPlayground :tcl="buildingDynamicStrings" />

## See Also

- [format](./format) - Printf-style formatting
- [string](./string) - String operations
- [eval](./eval) - Evaluate a script

