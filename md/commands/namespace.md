# namespace

Creates and manages namespaces for organizing commands and variables.

## Syntax

```tcl
namespace subcommand ?arg ...?
```

## Subcommands

### namespace eval

Evaluate a script in the context of a namespace, creating it if necessary.

```tcl
namespace eval ns script
```

### namespace current

Returns the fully-qualified name of the current namespace.

```tcl
namespace current
```

### namespace delete

Deletes one or more namespaces and their contents.

```tcl
namespace delete ?ns ...?
```

### namespace exists

Returns 1 if the namespace exists, 0 otherwise.

```tcl
namespace exists ns
```

### namespace children

Returns a list of child namespaces.

```tcl
namespace children ?ns? ?pattern?
```

### namespace parent

Returns the parent namespace of a namespace.

```tcl
namespace parent ?ns?
```

### namespace qualifiers

Extracts the namespace portion from a qualified name.

```tcl
namespace qualifiers string
```

### namespace tail

Extracts the tail (command name) from a qualified name.

```tcl
namespace tail string
```

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const createNamespaceWithProcedures = `# Create a namespace with procedures
namespace eval math {
    proc square {x} {
        expr {$x * $x}
    }
    proc cube {x} {
        expr {$x * $x * $x}
    }
}

puts "Square of 5: [math::square 5]"
puts "Cube of 3: [math::cube 3]"`

const namespaceVariables = `# Namespace variables
namespace eval counter {
    variable count 0
    
    proc increment {} {
        variable count
        incr count
    }
    
    proc get {} {
        variable count
        return $count
    }
}

counter::increment
counter::increment
puts "Count: [counter::get]"`

const checkCurrentNamespace = `# Check current namespace
puts "Global: [namespace current]"

namespace eval myns {
    puts "Inside myns: [namespace current]"
}`

const checkNamespaceExistence = `# Check namespace existence
namespace eval test {}

puts "test exists: [namespace exists test]"
puts "other exists: [namespace exists other]"

namespace delete test
puts "test after delete: [namespace exists test]"`

const nestedNamespaces = `# Nested namespaces
namespace eval outer {
    namespace eval inner1 {}
    namespace eval inner2 {}
}

puts "Children of outer:"
foreach ns [namespace children outer] {
    puts "  $ns"
}`

const parseQualifiedNames = `# Parse qualified names
set fullname "::foo::bar::baz"

puts "Full: $fullname"
puts "Qualifiers: [namespace qualifiers $fullname]"
puts "Tail: [namespace tail $fullname]"`

const getParentNamespace = `# Get parent namespace
namespace eval a::b::c {}

puts "Parent of ::a::b::c: [namespace parent ::a::b::c]"
puts "Parent of ::a::b: [namespace parent ::a::b]"
puts "Parent of ::a: [namespace parent ::a]"`
</script>

<FeatherPlayground :code="createNamespaceWithProcedures" />

<FeatherPlayground :code="namespaceVariables" />

<FeatherPlayground :code="checkCurrentNamespace" />

<FeatherPlayground :code="checkNamespaceExistence" />

<FeatherPlayground :code="nestedNamespaces" />

<FeatherPlayground :code="parseQualifiedNames" />

<FeatherPlayground :code="getParentNamespace" />

## See Also

- [info](./info)
- [trace](./trace)
- [eval](./eval)
