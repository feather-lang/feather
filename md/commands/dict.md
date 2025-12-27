# dict

Dictionary operations for key-value data structures.

## Syntax

```tcl
dict subcommand ?arg ...?
```

## Subcommands

### dict create

```tcl
dict create ?key value ...?
```

Creates and returns a new dictionary with the specified key-value pairs.

### dict get

```tcl
dict get dictionary ?key ...?
```

Returns the value for the specified key. Multiple keys navigate nested dictionaries.

### dict set

```tcl
dict set dictVarName key ?key ...? value
```

Sets a value in the dictionary variable. Multiple keys create nested dictionaries.

### dict exists

```tcl
dict exists dictionary key ?key ...?
```

Returns 1 if the key exists, 0 otherwise. Multiple keys check nested dictionaries.

### dict keys

```tcl
dict keys dictionary ?pattern?
```

Returns a list of all keys. Optional glob pattern filters results.

### dict values

```tcl
dict values dictionary ?pattern?
```

Returns a list of all values. Optional glob pattern filters results.

### dict size

```tcl
dict size dictionary
```

Returns the number of key-value pairs in the dictionary.

### dict remove

```tcl
dict remove dictionary ?key ...?
```

Returns a new dictionary with the specified keys removed.

### dict replace

```tcl
dict replace dictionary ?key value ...?
```

Returns a new dictionary with values replaced for the specified keys.

### dict merge

```tcl
dict merge ?dictionary ...?
```

Merges multiple dictionaries. Later dictionaries override earlier ones.

### dict append

```tcl
dict append dictVarName key ?value ...?
```

Appends values to the string value for the specified key in the variable.

### dict incr

```tcl
dict incr dictVarName key ?increment?
```

Increments the integer value for the specified key. Default increment is 1.

### dict lappend

```tcl
dict lappend dictVarName key ?value ...?
```

Appends values to the list value for the specified key in the variable.

### dict unset

```tcl
dict unset dictVarName key ?key ...?
```

Removes the specified key from the dictionary variable. Multiple keys for nested.

### dict for

```tcl
dict for {keyVar valueVar} dictionary body
```

Iterates over the dictionary, binding each key and value to variables.

### dict info

```tcl
dict info dictionary
```

Returns an implementation-specific information string about the dictionary.

### dict getdef

```tcl
dict getdef dictionary ?key ...? key default
```

Gets the value for the key, returning the default if the key doesn't exist.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const creatingAndAccessingDictionaries = `set person [dict create name "Alice" age 30 city "Paris"]
puts "Name: [dict get $person name]"
puts "Age: [dict get $person age]"
puts "Keys: [dict keys $person]"
puts "Size: [dict size $person]"`

const modifyingDictionaries = `set data [dict create count 0]
dict set data count 5
dict incr data count 3
puts "Count: [dict get $data count]"

dict set data nested key value
puts "Nested: [dict get $data nested key]"`

const iteratingOverDictionaries = `set colors [dict create red #ff0000 green #00ff00 blue #0000ff]
dict for {name hex} $colors {
    puts "$name = $hex"
}`

const usingDictGetdefForDefaults = `set config [dict create debug 1]
puts "Debug: [dict getdef $config debug 0]"
puts "Verbose: [dict getdef $config verbose 0]"`
</script>

### Creating and accessing dictionaries

<FeatherPlayground :code="creatingAndAccessingDictionaries" />

### Modifying dictionaries

<FeatherPlayground :code="modifyingDictionaries" />

### Iterating over dictionaries

<FeatherPlayground :code="iteratingOverDictionaries" />

### Using dict getdef for defaults

<FeatherPlayground :code="usingDictGetdefForDefaults" />

## See Also

- [list](./list) - List operations
