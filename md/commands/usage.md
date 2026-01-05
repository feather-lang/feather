# usage

Provides CLI argument parsing inspired by [usage.jdx.dev](https://usage.jdx.dev).

## Syntax

```tcl
usage subcommand ?arg ...?
```

## Description

The `usage` command provides:

- Declarative specification of command-line arguments, flags, and subcommands
- Automatic parsing of argument lists into local variables
- Help text generation
- Validation of required arguments and flag values
- Support for choices, defaults, variadic arguments, and nested subcommands

## Subcommands

| Subcommand | Syntax | Description |
|------------|--------|-------------|
| `for` | `usage for cmdName ?spec?` | Define or retrieve a usage spec for a command |
| `parse` | `usage parse cmdName argsList` | Parse arguments and create local variables |
| `help` | `usage help cmdName` | Generate help text for a command |

### usage for

Defines or retrieves a usage spec for a command.

```tcl
# Define a spec
usage for mycommand {
    arg <input>
    arg ?output?
    flag -v --verbose
}

# Retrieve the spec (same format as input)
set spec [usage for mycommand]
# => arg <input> arg ?output? flag -v --verbose

# Round-trip: copy spec to another command
usage for anothercommand $spec
```

### usage parse

Parses arguments and creates local variables in the caller's scope.

```tcl
proc mycommand {args} {
    usage parse mycommand $args
    # Variables are now set: $input, $output, $verbose
    puts "Input: $input"
    if {$verbose} {
        puts "Verbose mode enabled"
    }
}
```

Special variables created:
- `$subcommand` - A list containing the path of matched subcommands (e.g., `{remote add}` for nested commands)

### usage help

Generates formatted help text for a command.

```tcl
puts [usage help mycommand]
# Output:
# Usage: mycommand <input> ?output? ?-v?
#
# Arguments:
#   input     Input file to process
#   output    Output destination
#
# Flags:
#   -v, --verbose    Enable verbose output
```

## Spec Format

The spec uses a TCL-native block syntax with four entry types: `arg`, `flag`, `cmd`, and `example`.

### Arguments (arg)

```tcl
arg <name>              # Required argument
arg ?name?              # Optional argument
arg <name>...           # Required variadic (1 or more)
arg ?name?...           # Optional variadic (0 or more)
```

**Note:** We use `?name?` instead of `[name]` for optional arguments because square brackets trigger command substitution in TCL.

### Flags (flag)

```tcl
flag -s                 # Short flag only (boolean)
flag --long             # Long flag only (boolean)
flag -s --long          # Both short and long (boolean)
flag -f --file <path>   # Flag with required value
flag -o --output ?path? # Flag with optional value
```

Variable names are derived from flags:
- Long flags: `--verbose` -> `$verbose`
- Short flags only: `-v` -> `$v`
- Hyphens converted to underscores: `--ignore-case` -> `$ignore_case`

Boolean flags are set to `1` when present, `0` when absent.

### Subcommands (cmd)

```tcl
usage for git {
    cmd clone {
        arg <repository>
        arg ?directory?
    }
    cmd commit {
        flag -m --message <msg>
        flag -a --all
    }
}
```

When parsing, the `$subcommand` variable is set to a list of matched subcommand names.

### Examples (example)

```tcl
usage for mytool {
    arg <file>
    flag -v --verbose
    example {mytool myfile.txt}
    example {mytool -v myfile.txt} {
        header {Verbose mode}
        help {Process a file with verbose output}
    }
}
```

Examples are shown in the `usage help` output in a dedicated "Examples:" section.

### Options Blocks

Each entry can have an options block with additional configuration:

```tcl
arg <input> {
    help {The input file to process}
    long_help {
        Extended description that appears in detailed help.
        Can span multiple lines.
    }
    default {stdin}
    choices {json csv xml}
    type script
    hide
}

flag -f --format <fmt> {
    help {Output format}
    choices {json yaml toml}
}

cmd subcommand {
    # subcommand body
} {
    help {Description of subcommand}
    before_help {Prerequisites: setup required}
    after_help {See also: other commands}
    hide
}
```

| Option | Applies To | Description |
|--------|------------|-------------|
| `help` | All | Short help text displayed in usage output |
| `long_help` | All | Extended help for detailed documentation |
| `default` | `arg` | Default value when argument is omitted |
| `choices` | `arg`, `flag` | Space-separated list of valid values |
| `type` | `arg`, `flag` | Type hint for validation (e.g., `script`) |
| `hide` | All | Hide from help output |
| `before_help` | `cmd` | Text shown before the help content |
| `after_help` | `cmd` | Text shown after the help content |
| `before_long_help` | `cmd` | Text shown before extended help |
| `after_long_help` | `cmd` | Text shown after extended help |

### Text Trimming

Multi-line text in `help`, `long_help`, and `example` entries is automatically trimmed:
- Leading and trailing newlines are removed
- Common leading whitespace (indentation) is dedented

### Type Validation

The `type` option enables validation:

```tcl
arg <code> {type script}
```

Currently supported types:
- `script` - Validates that the value is a syntactically complete TCL script (balanced braces, quotes, brackets)

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicUsage = `# Define a simple command with usage
usage for greet {
    arg <name> {
        help {Name of the person to greet}
    }
    flag -l --loud {
        help {Use uppercase}
    }
}

proc greet {args} {
    usage parse greet $args
    if {$loud} {
        puts [string toupper "Hello, $name!"]
    } else {
        puts "Hello, $name!"
    }
}

greet Alice
greet -l Bob`

const showHelp = `# Generate help text
usage for compiler {
    arg <source> {
        help {Source file to compile}
    }
    arg ?output? {
        help {Output file}
        default {a.out}
    }
    flag -O --optimize ?level? {
        help {Optimization level}
        choices {0 1 2 3}
    }
    flag -g --debug {
        help {Include debug symbols}
    }
}

puts [usage help compiler]`

const withSubcommands = `# Define command with subcommands
usage for git {
    cmd clone {
        arg <repository>
        arg ?directory?
    } {
        help {Clone a repository}
    }
    cmd commit {
        flag -m --message <msg> {
            help {Commit message}
        }
    } {
        help {Record changes}
    }
}

proc git {args} {
    usage parse git $args
    puts "Subcommand: $subcommand"
    if {[lindex $subcommand 0] eq "clone"} {
        puts "Cloning $repository"
    }
}

git clone https://example.com/repo mydir`

const withChoices = `# Validate with choices
usage for format {
    arg <input>
    flag -f --format <fmt> {
        help {Output format}
        choices {json yaml toml}
    }
}

proc format {args} {
    usage parse format $args
    puts "Converting $input to $format format"
}

format data.txt -f json`
</script>

<FeatherPlayground :code="basicUsage" />

<FeatherPlayground :code="showHelp" />

<FeatherPlayground :code="withSubcommands" />

<FeatherPlayground :code="withChoices" />

## See Also

- [proc](./proc)
- [info](./info)
