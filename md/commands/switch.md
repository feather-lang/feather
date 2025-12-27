# switch

Multi-way branching based on pattern matching.

## Syntax

```tcl
switch ?options? string ?pattern body ...? ?default body?
```

## Parameters

- **options**: Matching mode flags
  - **-exact**: Exact string matching (default)
  - **-glob**: Glob-style pattern matching
  - **-regexp**: Regular expression matching
- **string**: Value to match against patterns
- **pattern**: Pattern to match
- **body**: Script to execute if pattern matches; use `-` for fall-through
- **default**: Keyword for the fallback case

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const basicExactMatching = `set day Monday

switch $day {
    Monday { puts {Start of week} }
    Friday { puts {Almost weekend} }
    Saturday - Sunday { puts Weekend! }
    default { puts Midweek }
}`

const globPatternMatching = `set file report.txt

switch -glob $file {
    *.txt { puts {Text file} }
    *.jpg - *.png { puts {Image file} }
    default { puts {Unknown type} }
}`

const fallThroughWithDash = `set grade B

switch $grade {
    A { puts Excellent! }
    B -
    C { puts {Good job} }
    D -
    F { puts {Needs improvement} }
}`

const usingListSyntax = `set fruit apple

switch $fruit apple {
    puts {It is an apple}
} banana {
    puts {It is a banana}
} default {
    puts {Unknown fruit}
}`
</script>

### Basic exact matching

<WasmPlayground :tcl="basicExactMatching" />

### Glob pattern matching

<WasmPlayground :tcl="globPatternMatching" />

### Fall-through with dash

<WasmPlayground :tcl="fallThroughWithDash" />

### Using list syntax

<WasmPlayground :tcl="usingListSyntax" />

## See Also

- [if](./if) - Conditional execution
- [expr](./expr) - Expression evaluation

