# lreverse

Returns a list with all elements in reverse order.

## Syntax

```tcl
lreverse list
```

## Parameters

- **list**: The list to reverse

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const reverseAList = `set nums {1 2 3 4 5}
puts [lreverse $nums]`

const reverseStrings = `set words {hello world foo bar}
puts [lreverse $words]`

const reverseAndIterate = `set items {first second third}
foreach item [lreverse $items] {
    puts $item
}`

const originalUnchanged = `set original {a b c}
set reversed [lreverse $original]
puts "Original: $original"
puts "Reversed: $reversed"`
</script>

### Reverse a list

<WasmPlayground :tcl="reverseAList" />

### Reverse strings

<WasmPlayground :tcl="reverseStrings" />

### Reverse and iterate

<WasmPlayground :tcl="reverseAndIterate" />

### Original unchanged

<WasmPlayground :tcl="originalUnchanged" />

## See Also

- [lsort](./lsort) - Sort list
- [lrange](./lrange) - Extract sublist
- [list](./list) - Create a list

