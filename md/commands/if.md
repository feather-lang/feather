# if

Conditional execution based on boolean expressions.

## Syntax

```tcl
if expr1 ?then? body1 ?elseif expr2 ?then? body2 ...? ?else bodyN?
```

## Parameters

- **expr1**: Boolean expression evaluated with `expr`
- **then**: Optional keyword for readability
- **body1**: Script to execute if expr1 is true
- **elseif**: Keyword introducing additional conditions
- **expr2**: Additional condition expression
- **body2**: Script to execute if expr2 is true
- **else**: Keyword introducing the fallback branch
- **bodyN**: Script to execute if no conditions are true

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicIfElse = `set x 10

if {$x > 5} {
    puts {x is greater than 5}
} else {
    puts {x is 5 or less}
}`

const multipleConditionsWithElseif = `set grade 85

if {$grade >= 90} {
    puts A
} elseif {$grade >= 80} {
    puts B
} elseif {$grade >= 70} {
    puts C
} else {
    puts F
}`

const usingOptionalThenKeyword = `set logged_in 1

if {$logged_in} then {
    puts {Welcome back!}
} else {
    puts {Please log in}
}`
</script>

### Basic if-else

<FeatherPlayground :code="basicIfElse" />

### Multiple conditions with elseif

<FeatherPlayground :code="multipleConditionsWithElseif" />

### Using optional then keyword

<FeatherPlayground :code="usingOptionalThenKeyword" />

## See Also

- [switch](./switch) - Multi-way branching
- [expr](./expr) - Expression evaluation
