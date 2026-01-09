# Command Completion

Feather provides intelligent command completion through the `usage complete` subcommand. This enables hosts to offer tab-completion, IDE autocomplete, or any interactive completion UI.

::: info Feather Extension
This is a **Feather-specific feature**, not part of standard TCL.
:::

## Why Use Completion?

When embedding Feather in an application (game console, CLI tool, REPL), you often want to help users discover available commands and their arguments. The completion system:

- Returns structured completion candidates based on usage specs
- Handles commands, subcommands, flags, and argument values
- Supports nested script contexts (completing inside `if` bodies, etc.)
- Works with any UI (terminal, GUI, web)

## Try It

Experience completion in this interactive REPL. Press **Tab** to trigger completions, **Shift+Tab** to cycle back, and **Enter** to select.

<FeatherRepl />

## Basic Usage

```tcl
usage complete script pos
```

Returns a list of completion candidates at byte position `pos` in `script`.

## Return Format

Each candidate is a dict with these keys:

```tcl
{text <string> type <category> help <description>}
```

For argument placeholders (when the user must provide a value):

```tcl
{text {} type arg-placeholder name <arg-name> help <description>}
```

## Completion Types

| Type | Description |
|------|-------------|
| `command` | Top-level command name |
| `subcommand` | Subcommand from usage spec |
| `flag` | Flag option (-v, --verbose) |
| `value` | Value from `choices` list |
| `arg-placeholder` | Indicates user must provide a value |

## Examples

<script setup>
import WasmPlayground from '../.vitepress/components/WasmPlayground.vue'
import FeatherRepl from '../.vitepress/components/FeatherRepl.vue'

const commandCompletion = `usage for puts {arg <text>}
usage for print {arg <msg>}
usage for printf {arg <format> arg ?values?...}

# Complete commands starting with "pr"
set candidates [usage complete {pr} 2]
foreach c $candidates {
    puts "[dict get $c text] ([dict get $c type])"
}`

const subcommandCompletion = `usage for git {
    cmd clone {arg <repo>}
    cmd commit {flag -m --message <msg>}
    cmd checkout {arg <branch>}
}

# Complete subcommands starting with "c"
set candidates [usage complete {git c} 5]
foreach c $candidates {
    puts "[dict get $c text]"
}`

const flagCompletion = `usage for compile {
    arg <source>
    flag --verbose {help {Enable verbose output}}
    flag -O --optimize ?level?
}

# Complete after the source file
set candidates [usage complete {compile file.c -} 15]
foreach c $candidates {
    puts "[dict get $c text]"
}`

const choicesCompletion = `usage for export {
    flag --format <fmt> {
        choices {json yaml toml xml}
        help {Output format}
    }
}

# Complete format values starting with "j"
set candidates [usage complete {export --format j} 18]
foreach c $candidates {
    puts "[dict get $c text] - [dict get $c help]"
}`

const placeholderExample = `usage for cmd {
    arg <input> {help {Input file}}
    arg ?output? {help {Output file}}
    flag --verbose
}

# When a required arg is expected
set candidates [usage complete {cmd } 4]
foreach c $candidates {
    if {[dict get $c type] eq "arg-placeholder"} {
        puts "<[dict get $c name]> - [dict get $c help]"
    } else {
        puts "[dict get $c text] ([dict get $c type])"
    }
}`
</script>

### Command Completion

<WasmPlayground :tcl="commandCompletion" />

### Subcommand Completion

<WasmPlayground :tcl="subcommandCompletion" />

### Flag Completion

<WasmPlayground :tcl="flagCompletion" />

### Value Completion (from choices)

<WasmPlayground :tcl="choicesCompletion" />

### Argument Placeholders

<WasmPlayground :tcl="placeholderExample" />

## Host Integration

### Terminal REPL Example (Go)

```go
func complete(script string, pos int) []Completion {
    // usage complete returns a native Go slice of objects
    result, _ := interp.Eval("usage", "complete", script, pos)
    
    candidates := result.ToList()
    var completions []Completion
    
    for _, c := range candidates {
        dict := c.ToDict()
        comp := Completion{
            Text: dict["text"].String(),
            Type: dict["type"].String(),
            Help: dict["help"].String(),
        }
        
        // Handle arg-placeholder specially
        if comp.Type == "arg-placeholder" {
            comp.Name = dict["name"].String()
            comp.Display = fmt.Sprintf("<%s>", comp.Name)
        } else {
            comp.Display = comp.Text
        }
        
        completions = append(completions, comp)
    }
    
    return completions
}
```

### Type Hints for File Completion

Arguments with `type file` or `type dir` are **not** completed by Feather. The host should intercept these and provide filesystem-based completion:

```tcl
usage for load {
    arg <script> {type file}
}
```

When you get an `arg-placeholder` with a file type, query the filesystem in your host application.

## Completion Rules

1. **Command matching**: Case-sensitive prefix match, alphabetically sorted
2. **Subcommand matching**: Prefix match, preserved spec order
3. **Flag matching**: Offered when valid, both short and long forms included
4. **Value matching**: Only when flag/arg has `choices` defined
5. **Hidden entries**: Entries with `hide` are excluded from completions
6. **Script contexts**: Recursively completes inside `type script` arguments

## Nested Script Completion

Feather can complete inside script arguments:

```tcl
usage for if {
    arg <condition>
    arg <body> {type script}
}

usage for puts {arg <text>}

# Completing "pu" inside the if body:
usage complete {if {$x > 0} {pu}} 17
# => {text puts type command help {}}
```

## See Also

- [usage](/commands/usage) - Full `usage` command reference
