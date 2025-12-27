# concat

Concatenate arguments with spaces.

## Syntax

```tcl
concat ?arg ...?
```

## Parameters

- **arg**: One or more values to concatenate

## Description

Joins all arguments together with spaces. Leading and trailing whitespace is trimmed from each argument before joining. This makes it useful for merging lists.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicConcatenation = `puts [concat a b c]
puts [concat hello world]`

const trimmingWhitespace = `puts [concat "  hello  " "  world  "]`

const merginglists = `set list1 {a b c}
set list2 {d e f}
puts [concat $list1 $list2]`

const emptyArguments = `puts ">[concat]<"
puts [concat {} {a b} {} {c}]`
</script>

### Basic concatenation

<FeatherPlayground :code="basicConcatenation" />

### Trimming whitespace

<FeatherPlayground :code="trimmingWhitespace" />

### Merging lists

<FeatherPlayground :code="merginglists" />

### Empty arguments

<FeatherPlayground :code="emptyArguments" />

## See Also

- [join](./join) - Join list with custom separator
- [string](./string) - String operations
- [list](./list) - Create a list
- [append](./append) - Append to a variable