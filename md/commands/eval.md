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

### Argument Handling

- **Single argument**: When exactly one argument is provided, it is used directly as the script to evaluate.
- **Multiple arguments**: When multiple arguments are provided, they are concatenated using the same rules as the `concat` command before evaluation.

### Error and Result Propagation

Any error generated during evaluation is propagated back to the caller. Similarly, the result of the evaluated script becomes the result of the `eval` command.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

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

<WasmPlayground :tcl="basicEval" />

<WasmPlayground :tcl="buildingCommandsDynamically" />

<WasmPlayground :tcl="evaluatingListAsCommand" />

## See Also

- [concat](./concat) - The concatenation rules used when multiple arguments are provided
- [catch](./catch) - Capture errors from evaluated scripts
- [subst](./subst) - Perform substitutions without evaluation
- [uplevel](./uplevel) - Evaluate script in a different stack level
- [namespace](./namespace) - Namespace management commands

