# usage Command Implementation

## Summary of Our Implementation

The `usage` command in feather provides CLI argument parsing inspired by [usage.jdx.dev](https://usage.jdx.dev). It is implemented in `src/builtin_usage.c` and provides:

- Declarative specification of command-line arguments, flags, and subcommands
- Automatic parsing of argument lists into local variables
- Help text generation
- Validation of required arguments and flag values
- Support for choices, defaults, variadic arguments, and nested subcommands

The implementation also exposes a public C API for hosts to programmatically construct usage specs without parsing TCL strings.

## TCL Interface

### Subcommands

| Subcommand | Syntax | Description |
|------------|--------|-------------|
| `for` | `usage for cmdName ?spec?` | Define or retrieve a usage spec for a command |
| `parse` | `usage parse cmdName argsList` | Parse arguments and create local variables |
| `help` | `usage help cmdName` | Generate help text for a command |

### `usage for` - Define or Retrieve Specs

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

When called with a spec, stores the specification for later use with `parse` and `help`. When called without a spec, returns the spec in the same list format that `usage for` accepts (round-trippable).

### `usage parse` - Parse Arguments

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

The `parse` subcommand:
1. Looks up the spec for the given command name
2. Parses the argument list according to the spec
3. Creates local variables in the caller's scope
4. Returns an error if required arguments are missing or invalid

Special variables created:
- `$subcommand` - A list containing the path of matched subcommands (e.g., `{remote add}` for nested commands)

### `usage help` - Generate Help Text

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

### Arguments (`arg`)

```tcl
arg <name>              # Required argument
arg ?name?              # Optional argument
arg <name>...           # Required variadic (1 or more)
arg ?name?...           # Optional variadic (0 or more)
```

**Note:** We use `?name?` instead of `[name]` for optional arguments because square brackets trigger command substitution in TCL.

### Flags (`flag`)

```tcl
flag -s                 # Short flag only (boolean)
flag --long             # Long flag only (boolean)
flag -s --long          # Both short and long (boolean)
flag -f --file <path>   # Flag with required value
flag -o --output ?path? # Flag with optional value
```

Variable names are derived from flags:
- Long flags: `--verbose` → `$verbose`
- Short flags only: `-v` → `$v`
- Hyphens converted to underscores: `--ignore-case` → `$ignore_case`

Boolean flags are set to `1` when present, `0` when absent.

### Subcommands (`cmd`)

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

### Examples (`example`)

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

Examples are shown in the `usage help` output in a dedicated "Examples:" section. Each example consists of a code snippet and optional options:

| Option | Description |
|--------|-------------|
| `header` | A title for the example |
| `help` | A description of what the example does |

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

This allows clean formatting in specs while producing proper output:

```tcl
arg <file> {
    help {
        This is a multi-line description
        that will be dedented properly
    }
}
```

### Type Validation

The `type` option enables validation:

```tcl
arg <code> {type script}
```

Currently supported types:
- `script` - Validates that the value is a syntactically complete TCL script (balanced braces, quotes, brackets)

## Complete Example

```tcl
# Define the spec
usage for compiler {
    arg <source> {
        help {Source file to compile}
        choices {*.c *.cpp *.rs}
    }
    arg ?output? {
        help {Output file}
        default {a.out}
    }
    flag -O --optimize ?level? {
        help {Optimization level}
        choices {0 1 2 3 s z}
    }
    flag -g --debug {
        help {Include debug symbols}
    }
    flag -W --warn <type>... {
        help {Enable warnings}
    }
    cmd check {
        arg <file>
        flag --strict
    } {
        help {Check syntax without compiling}
    }
}

# Implement the command
proc compiler {args} {
    usage parse compiler $args

    if {[llength $subcommand] > 0 && [lindex $subcommand 0] eq "check"} {
        puts "Checking $file (strict=$strict)"
        return
    }

    puts "Compiling $source -> $output"
    if {$debug} {
        puts "  Debug symbols enabled"
    }
    if {$optimize ne ""} {
        puts "  Optimization level: $optimize"
    }
}

# Use the command
compiler input.c -O2 -g
compiler check main.c --strict
puts [usage help compiler]
```

## Public C API

For hosts embedding feather, a public C API allows programmatic construction of usage specs without parsing TCL strings.

### Header Declarations

All functions are declared in `src/feather.h`:

```c
#include "feather.h"

// Entry creation
FeatherObj feather_usage_arg(const FeatherHostOps *ops, FeatherInterp interp,
                             const char *name);
FeatherObj feather_usage_flag(const FeatherHostOps *ops, FeatherInterp interp,
                              const char *short_flag,
                              const char *long_flag,
                              const char *value);
FeatherObj feather_usage_cmd(const FeatherHostOps *ops, FeatherInterp interp,
                             const char *name,
                             FeatherObj subspec);
FeatherObj feather_usage_example(const FeatherHostOps *ops, FeatherInterp interp,
                                 const char *code,
                                 const char *header,
                                 const char *help);

// Entry modification (all return modified entry)
FeatherObj feather_usage_help(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry, const char *text);
FeatherObj feather_usage_long_help(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj entry, const char *text);
FeatherObj feather_usage_default(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry, const char *value);
FeatherObj feather_usage_choices(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry, FeatherObj choices);
FeatherObj feather_usage_type(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry, const char *type);
FeatherObj feather_usage_hide(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry);

// Spec assembly
FeatherObj feather_usage_spec(const FeatherHostOps *ops, FeatherInterp interp);
FeatherObj feather_usage_add(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj spec, FeatherObj entry);
FeatherObj feather_usage_about(const FeatherHostOps *ops, FeatherInterp interp,
                               const char *about, const char *description);
void feather_usage_register(const FeatherHostOps *ops, FeatherInterp interp,
                            const char *cmdname, FeatherObj spec);
```

### Entry Creation Functions

#### `feather_usage_arg`

```c
FeatherObj feather_usage_arg(ops, interp, "<input>");   // required
FeatherObj feather_usage_arg(ops, interp, "?output?");  // optional
FeatherObj feather_usage_arg(ops, interp, "<files>..."); // variadic
```

The name format encodes required/optional/variadic status.

#### `feather_usage_flag`

```c
// Boolean flag
feather_usage_flag(ops, interp, "-v", "--verbose", NULL);

// Flag with required value
feather_usage_flag(ops, interp, "-f", "--file", "<path>");

// Flag with optional value
feather_usage_flag(ops, interp, "-o", "--output", "?path?");

// Short flag only
feather_usage_flag(ops, interp, "-h", NULL, NULL);

// Long flag only
feather_usage_flag(ops, interp, NULL, "--help", NULL);
```

#### `feather_usage_cmd`

```c
FeatherObj subspec = feather_usage_spec(ops, interp);
subspec = feather_usage_add(ops, interp, subspec,
    feather_usage_arg(ops, interp, "<file>"));

FeatherObj cmd = feather_usage_cmd(ops, interp, "add", subspec);
```

#### `feather_usage_example`

```c
// Simple example (code only)
feather_usage_example(ops, interp, "mytool file.txt", NULL, NULL);

// Example with header
feather_usage_example(ops, interp, "mytool -v file.txt", "Verbose mode", NULL);

// Example with header and description
feather_usage_example(ops, interp,
    "mytool --format json file.txt",
    "JSON output",
    "Process the file and output results in JSON format.");
```

#### `feather_usage_about`

Creates a meta entry for the command's NAME and DESCRIPTION sections in manpage output:

```c
// Short description only (shown in NAME section)
feather_usage_about(ops, interp, "Process files", NULL);

// Short description and detailed description
feather_usage_about(ops, interp,
    "Process files",
    "Reads input files and processes them according to the specified options. "
    "The output can be written to stdout or to a file specified with --output.");
```

The `about` parameter provides a one-line description shown in the NAME section. The `description` parameter provides detailed text shown in the DESCRIPTION section (with automatic word wrapping).

### Modification Functions

All modification functions return the modified entry, allowing chaining:

```c
FeatherObj e = feather_usage_arg(ops, interp, "<input>");
e = feather_usage_help(ops, interp, e, "Input file to process");
e = feather_usage_default(ops, interp, e, "stdin");
```

### Complete C Example

```c
void register_mycommand(const FeatherHostOps *ops, FeatherInterp interp) {
    FeatherObj spec = feather_usage_spec(ops, interp);

    // Add command description (for NAME and DESCRIPTION sections)
    FeatherObj e = feather_usage_about(ops, interp,
        "Process input files",
        "Reads the input file and processes it according to the specified "
        "options. Output is written to the specified destination or stdout.");
    spec = feather_usage_add(ops, interp, spec, e);

    // Add required input argument with help
    e = feather_usage_arg(ops, interp, "<input>");
    e = feather_usage_help(ops, interp, e, "Input file to process");
    spec = feather_usage_add(ops, interp, spec, e);

    // Add optional output argument with default
    e = feather_usage_arg(ops, interp, "?output?");
    e = feather_usage_help(ops, interp, e, "Output destination");
    e = feather_usage_default(ops, interp, e, "stdout");
    spec = feather_usage_add(ops, interp, spec, e);

    // Add verbose flag
    e = feather_usage_flag(ops, interp, "-v", "--verbose", NULL);
    e = feather_usage_help(ops, interp, e, "Enable verbose output");
    spec = feather_usage_add(ops, interp, spec, e);

    // Add subcommand with its own spec
    FeatherObj subspec = feather_usage_spec(ops, interp);
    subspec = feather_usage_add(ops, interp, subspec,
        feather_usage_arg(ops, interp, "<file>"));
    e = feather_usage_cmd(ops, interp, "check", subspec);
    e = feather_usage_help(ops, interp, e, "Check syntax only");
    spec = feather_usage_add(ops, interp, spec, e);

    // Add example
    e = feather_usage_example(ops, interp,
        "mycommand input.txt output.txt",
        "Basic usage",
        "Process input.txt and write to output.txt.");
    spec = feather_usage_add(ops, interp, spec, e);

    // Register the spec
    feather_usage_register(ops, interp, "mycommand", spec);
}
```

## Internal Storage Format

Usage specs are stored in the `::usage::specs` namespace variable as a dict mapping command names to parsed spec lists.

Each entry in the parsed spec is a dict with a `type` key indicating the entry type. Only set keys that are needed (sparse representation).

### Arg Entry Keys

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | Always `"arg"` |
| `name` | string | Argument name (without delimiters) |
| `required` | int | `1` if required, `0` if optional |
| `variadic` | int | `1` if accepts multiple values |
| `help` | string | Short help text |
| `default` | string | Default value |
| `long_help` | string | Extended help text |
| `choices` | string | Space-separated valid values |
| `hide` | int | `1` to hide from help |
| `value_type` | string | Type hint (e.g., `"script"`) |

### Flag Entry Keys

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | Always `"flag"` |
| `short` | string | Short flag without dash (e.g., `"v"`) |
| `long` | string | Long flag without dashes (e.g., `"verbose"`) |
| `has_value` | int | `1` if flag takes a value |
| `value_required` | int | `1` if value is required when flag present |
| `var_name` | string | Variable name (auto-derived) |
| `help` | string | Short help text |
| `long_help` | string | Extended help text |
| `choices` | string | Space-separated valid values |
| `hide` | int | `1` to hide from help |
| `value_type` | string | Type hint |

### Cmd Entry Keys

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | Always `"cmd"` |
| `name` | string | Subcommand name |
| `spec` | list | List of entries for this subcommand |
| `help` | string | Short help text |
| `long_help` | string | Extended help text |
| `hide` | int | `1` to hide from help |
| `before_help` | string | Text shown before help content |
| `after_help` | string | Text shown after help content |
| `before_long_help` | string | Text shown before extended help |
| `after_long_help` | string | Text shown after extended help |

### Example Entry Keys

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | Always `"example"` |
| `code` | string | The example command to show |
| `header` | string | Title for the example |
| `help` | string | Description of what the example does |

### Meta Entry Keys

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | Always `"meta"` |
| `about` | string | One-line description for the NAME section |
| `long_help` | string | Detailed description for the DESCRIPTION section |

## Notes on Implementation

### Variable Name Derivation

For flags, the variable name is automatically derived:
1. Use long flag name if available, otherwise short flag name
2. Strip leading dashes
3. Convert internal hyphens to underscores

Example: `--ignore-case` → `$ignore_case`

### Argument Parsing Order

1. Flags are parsed first (can appear anywhere in the argument list)
2. Positional arguments are matched in order
3. The special `--` separator stops flag parsing

### Error Messages

```tcl
usage parse mycommand {}
# Error: missing required argument "input"

usage parse mycommand {file.txt --format invalid}
# Error: invalid value "invalid" for flag "--format": must be one of json, yaml, toml
```

### Choices Validation

Both arguments and flags with `choices` are validated. The error message lists all valid options.

### Script Type Validation

Arguments or flags with `type script` are validated for syntactic completeness:

```tcl
usage for eval {
    arg <code> {type script}
}

proc eval {args} {
    usage parse eval $args
    uplevel 1 $code
}

eval {puts "hello"}      ;# OK
eval {puts "hello}       ;# Error: argument "code" must be a complete script
eval {if {$x}            ;# Error: argument "code" must be a complete script
```
