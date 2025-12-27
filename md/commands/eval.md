# eval

Concatenates arguments and evaluates the result as a script.

## Syntax

```tcl
eval arg ?arg ...?
```

## Parameters

- **arg**: One or more arguments to concatenate and evaluate

## Description

The `eval` command concatenates all its arguments (using the same rules as `concat`), then evaluates the result as a Tcl script. This is useful for constructing commands dynamically or evaluating lists as scripts.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicEval = `# Basic eval
set cmd "puts"
set msg "Hello from eval!"
eval $cmd $msg`

const buildingCommandsDynamically = `# Building commands dynamically
set varname "x"
set value 42
eval set $varname $value
puts "x = $x"`

const evaluatingListAsCommand = `# Evaluating a list as a command
set cmdlist [list puts "Multiple" "arguments" "joined"]
eval $cmdlist`
</script>

<FeatherPlayground :code="basicEval" />

<FeatherPlayground :code="buildingCommandsDynamically" />

<FeatherPlayground :code="evaluatingListAsCommand" />

## See Also

- [expr](./expr)
- [namespace](./namespace)
