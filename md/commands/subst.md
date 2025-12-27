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

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="allSubstitutions" />

### Disable command substitution

<FeatherPlayground :code="disableCommandSubstitution" />

### Disable variable substitution

<FeatherPlayground :code="disableVariableSubstitution" />

### Disable backslash substitution

<FeatherPlayground :code="disableBackslashSubstitution" />

### Multiple options

<FeatherPlayground :code="multipleOptions" />

### Building dynamic strings

<FeatherPlayground :code="buildingDynamicStrings" />

## See Also

- [format](./format) - Printf-style formatting
- [string](./string) - String operations
- [eval](./eval) - Evaluate a script
