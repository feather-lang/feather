# Instructions: Document the `usage` Builtin and Completion System

This document provides instructions for creating documentation for Feather's `usage` builtin command, with special focus on the new completion system.

## Overview

The `usage` command is a **Feather-specific feature** (not from standard TCL) that provides:
1. Declarative CLI argument specification
2. Automatic argument parsing
3. Help text generation  
4. **Command completion** (the new feature)

## Files to Create

### 1. `md/commands/usage.md` - Main Command Reference

Add to the Built-in Commands section. Document all subcommands:

```tcl
usage for command ?spec?     # Define or get usage spec for a command
usage parse command args     # Parse args and set local variables
usage help command           # Generate help text
usage complete script pos    # Get completion candidates (NEW)
```

#### Spec Format

Document the TCL-native block syntax:

```tcl
usage for mycommand {
    arg <input>              # Required positional argument
    arg ?output?             # Optional positional argument  
    arg <files>...           # Variadic required (1 or more)
    arg ?extras?...          # Variadic optional (0 or more)
    flag -v --verbose        # Boolean flag
    flag -o --output <file>  # Flag with required value
    flag -l --level ?n?      # Flag with optional value
    cmd subname { ... }      # Subcommand with its own spec
    
    help {Short description}
    long_help {Extended description with more detail}
}
```

#### Options Block

Each arg/flag/cmd can have an options block:

```tcl
arg <format> {
    help {Output format}
    choices {json yaml toml xml}
    default {json}
    type {file}              # Type hints: file, dir, script
    hide                     # Exclude from help/completion
}
```

### 2. `md/completion.md` - Completion System Guide (NEW SIDEBAR SECTION)

Create a new guide explaining **why** and **how** to use completion. This is the main new documentation.

#### Structure for completion.md:

```markdown
# Command Completion

Feather provides intelligent command completion through the `usage complete` 
subcommand. This enables hosts to offer tab-completion, IDE autocomplete, 
or any interactive completion UI.

## Why Use Completion?

When embedding Feather in an application (game console, CLI tool, REPL), 
you often want to help users discover available commands and their arguments.
The completion system:

- Returns structured completion candidates based on usage specs
- Handles commands, subcommands, flags, and argument values
- Supports nested script contexts (completing inside `if` bodies, etc.)
- Works with any UI (terminal, GUI, web)

## Basic Usage

```tcl
usage complete script pos
```

Returns a list of completion candidates at byte position `pos` in `script`.

### Return Format

Each candidate is a dict:
```tcl
{text <string> type <category> help <description>}
```

For argument placeholders:
```tcl
{text {} type arg-placeholder name <arg-name> help <description>}
```

### Completion Types

| Type | Description |
|------|-------------|
| `command` | Top-level command name |
| `subcommand` | Subcommand from usage spec |
| `flag` | Flag option (-v, --verbose) |
| `value` | Value from `choices` list |
| `arg-placeholder` | Indicates user must provide a value |

## Examples

### Command Completion

```tcl
usage for puts {arg <text>}
usage for print {arg <msg>}

usage complete {pr} 2
# => {text print type command help {}} {text puts type command help {}}
```

### Subcommand Completion

```tcl
usage for git {
    cmd clone {arg <repo>}
    cmd commit {flag -m --message <msg>}
    cmd checkout {arg <branch>}
}

usage complete {git co} 6
# => {text commit type subcommand help {}}
#    {text checkout type subcommand help {}}
```

### Flag Completion

```tcl
usage for compile {
    arg <source>
    flag --verbose {help {Enable verbose output}}
    flag -O --optimize ?level?
}

usage complete {compile file.c } 15
# => {text --verbose type flag help {Enable verbose output}}
#    {text -O type flag help {}}
#    {text --optimize type flag help {}}
```

### Value Completion (from choices)

```tcl
usage for compile {
    flag --format <fmt> {
        choices {json yaml toml}
        help {Output format}
    }
}

usage complete {compile --format } 18
# => {text json type value help {Output format}}
#    {text yaml type value help {Output format}}
#    {text toml type value help {Output format}}

usage complete {compile --format j} 19
# => {text json type value help {Output format}}
```

### Argument Placeholders

When a positional argument is expected:

```tcl
usage for cmd {
    arg <input> {help {Input file}}
    arg ?output? {help {Output file}}
    flag --verbose
}

usage complete {cmd } 4
# => {text {} type arg-placeholder name input help {Input file}}
#    {text --verbose type flag help {}}
```

## Host Integration

### Terminal REPL Example (Go)

```go
func complete(script string, pos int) []Completion {
    result := interp.Eval(fmt.Sprintf(
        "usage complete {%s} %d", 
        tclEscape(script), pos))
    
    candidates := parseCompletionList(result)
    
    // Handle arg-placeholder specially
    for _, c := range candidates {
        if c.Type == "arg-placeholder" {
            // Show as hint, not insertable text
            c.Display = fmt.Sprintf("<%s>", c.Name)
        }
    }
    
    return candidates
}
```

### Type Hints for File Completion

Arguments with `type file` or `type dir` are **not** completed by Feather.
The host should intercept these and provide filesystem-based completion:

```tcl
usage for load {
    arg <script> {type file}
}
```

When you get an `arg-placeholder` with a file type, query the filesystem.

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
```

### 3. Update `.vitepress/config.mjs`

Add the new pages to the sidebar:

```javascript
// In the "Built-in Commands" section items array, add:
{ text: "usage", link: "/commands/usage" },

// Add a new sidebar section after "Built-in Commands":
{
  text: "Feather Extensions",
  items: [
    { text: "Command Completion", link: "/completion" },
  ],
},
```

## Reference Materials

The following files in `/home/exedev/w/feather` contain the authoritative implementation details:

| File | Contents |
|------|----------|
| `ROADMAP.md` | Full specification for `usage complete` |
| `src/builtin_usage.c` | C implementation (4100+ lines) |
| `testcases/feather/usage-complete-*.html` | Test cases showing expected behavior |

### Key Test Files

- `usage-complete-basic.html` - Command prefix matching, position handling
- `usage-complete-advanced.html` - Subcommands, flags, nested specs
- `usage-complete-choices.html` - Value completion from choices lists
- `usage-complete-placeholders.html` - Argument placeholder behavior

Run tests with:
```bash
cd ~/w/feather
mise build
bin/harness run --host bin/feather-tester testcases/feather/usage-complete-basic.html
```

## Style Notes

- Follow the existing documentation style in `md/commands/dict.md` for command reference
- Use TCL code blocks for examples
- Keep examples simple and self-contained
- Emphasize that this is a Feather-specific feature, not standard TCL
- Focus on practical host integration for the completion guide

## Checklist

- [ ] Create `md/commands/usage.md` with full command reference
- [ ] Create `md/completion.md` with the new guide
- [ ] Update `.vitepress/config.mjs` to add both pages to sidebar
- [ ] Add "Feather Extensions" sidebar section for non-TCL features
- [ ] Test that the site builds: `bun run docs:build`
- [ ] Preview: `bun run docs:dev`
