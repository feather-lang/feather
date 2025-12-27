# lreplace

Replaces a range of elements in a list with new values, returning the modified list.

## Syntax

```tcl
lreplace list first last ?value ...?
```

## Parameters

- **list**: The original list
- **first**: Starting index of range to replace (0-based, or `end`, `end-N`)
- **last**: Ending index of range to replace (inclusive)
- **value**: Zero or more replacement values

If no values are provided, the range is deleted. If first > last, values are inserted before first.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const replaceSingleElement = `set colors {red green blue}
puts [lreplace $colors 1 1 yellow]`

const replaceRangeWithDifferentCount = `set nums {1 2 3 4 5}
puts [lreplace $nums 1 3 a b]`

const deleteElements = `set items {a b c d e}
puts [lreplace $items 1 2]`

const insertElements = `set list {a d e}
puts [lreplace $list 1 0 b c]`

const appendToList = `set nums {1 2 3}
puts [lreplace $nums end+1 end x y z]`
</script>

### Replace single element

<WasmPlayground :tcl="replaceSingleElement" />

### Replace range with different count

<WasmPlayground :tcl="replaceRangeWithDifferentCount" />

### Delete elements (no replacement values)

<WasmPlayground :tcl="deleteElements" />

### Insert elements (first > last)

<WasmPlayground :tcl="insertElements" />

### Append to list

<WasmPlayground :tcl="appendToList" />

## See Also

- [lrange](./lrange) - Extract sublist
- [lset](./lset) - Set element by index
- [linsert](./linsert) - Insert elements

