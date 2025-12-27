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
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

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

<FeatherPlayground :code="renameExample" />

## See Also

- [proc](./proc)
- [info](./info)
