# rename

Rename or delete a command.

## Syntax

```tcl
rename oldName newName
```

## Parameters

- **oldName**: The current name of the command
- **newName**: The new name for the command (empty string to delete)

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const renameExample = `proc greet {name} {
    return "Hello, $name!"
}

puts [greet "World"]

# Rename the command
rename greet sayHello
puts [sayHello "Feather"]

# Delete a command by renaming to empty string
rename sayHello ""`
</script>

<WasmPlayground :tcl="renameExample" />

## See Also

- [proc](./proc)
- [info](./info)

