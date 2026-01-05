---
name: define-usage
description: Defines or updates usage help for built-in commands. Use when adding help documentation to commands that don't have it, or fixing inaccurate help text.
---

# Define Usage Skill

Step-by-step process for adding or updating usage help documentation for built-in commands in feather.

## When to Use This Skill

- Adding usage help to a command that doesn't have it
- Fixing inaccurate or outdated help text
- Ensuring help text matches Feather's actual capabilities (not TCL's)

## Working Process

### 1. Check Current Help Status

Test if the command already has usage help:

```bash
echo "usage help <command>" | ./bin/feather-tester
```

If no help exists, you'll see:
```
no usage defined for "<command>"
```

### 2. Review the Command Implementation

Read the command's implementation to understand what it actually does:

```bash
# Read the builtin implementation
cat src/builtin_<command>.c
```

Key things to identify:
- What arguments does it accept?
- What does it return?
- What are the error cases?
- Are there any Feather-specific limitations vs TCL?

### 3. Check TCL Documentation (If Applicable)

If implementing a TCL command, check the reference manual:

```bash
man -P cat n <command>
```

**IMPORTANT**: Do not copy TCL documentation verbatim. Feather has important differences:
- **No array support** - Array syntax like `myArray(key)` is not supported
- Limited standard library - Many TCL commands may not exist
- Cross-references must point to commands that exist in Feather

### 4. Identify Feather-Specific Constraints

Key differences from TCL to document:

**Arrays**: Feather does not support TCL-style arrays per CLAUDE.md. If TCL docs mention array functionality, explicitly state arrays are not supported.

**Cross-references**: Only reference commands that have (or will have) usage help in Feather:
- Keep references if the command will be documented later
- Remove references to standard TCL commands not in Feather
- Use judgment: namespace, global, variable, upvar are core and will be added

### 5. Write the Usage Registration Function

The usage registration function follows this pattern:

```c
void feather_register_<command>_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Brief one-line description",
    "Detailed description paragraph 1.\n\n"
    "Detailed description paragraph 2.\n\n"
    "Note about Feather-specific behavior if needed.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required arguments
  e = feather_usage_arg(ops, interp, "<argName>");
  e = feather_usage_help(ops, interp, e, "Description of argument");
  spec = feather_usage_add(ops, interp, spec, e);

  // Optional arguments
  e = feather_usage_arg(ops, interp, "?optionalArg?");
  e = feather_usage_help(ops, interp, e, "Description of optional argument");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "command arg1 arg2",
    "Description of what this example does",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "<command>", spec);
}
```

### 6. Add Declaration and Registration

**Add to src/internal.h:**

```c
void feather_register_<command>_usage(const FeatherHostOps *ops, FeatherInterp interp);
```

**Add call in src/interp.c** in the `feather_register_usage()` function:

```c
feather_register_<command>_usage(ops, interp);
```

### 7. Build and Test

```bash
# Build the project
mise build

# View the generated help
echo "usage help <command>" | ./bin/feather-tester
```

Review the output carefully:
- Is the SYNOPSIS correct?
- Does the DESCRIPTION match Feather's behavior?
- Are Feather-specific notes clear?
- Are examples helpful and accurate?
- Do cross-references point to real/future commands?

### 8. Verify Formatting

The help output should follow Unix manpage format:

```
command(1)                General Commands Manual               command(1)

NAME
       command - Brief description

SYNOPSIS
       command <required> ?optional?

DESCRIPTION
       Detailed description with proper paragraph breaks.

       Second paragraph if needed.

ARGUMENTS
       <required>
              Description of required arg
       ?optional?
              Description of optional arg

EXAMPLES
       Description of example:

           command example code
```

### 9. Commit Changes

Use the commit skill or create a descriptive commit:

```bash
git add -A
git commit -m "Add usage help for <command> command

Implemented comprehensive usage documentation including:
- NAME and SYNOPSIS sections
- Detailed DESCRIPTION of behavior
- ARGUMENTS documentation
- EXAMPLES section

[Note any Feather-specific deviations from TCL]"
```

## Usage API Reference

### Core Functions

| Function | Purpose |
|----------|---------|
| `feather_usage_spec()` | Create new spec |
| `feather_usage_about()` | Set command name and description |
| `feather_usage_arg()` | Add argument (use `<name>` for required, `?name?` for optional) |
| `feather_usage_help()` | Add help text to previous element |
| `feather_usage_example()` | Add code example with description |
| `feather_usage_add()` | Add element to spec |
| `feather_usage_register()` | Register complete spec for command |

### Argument Syntax

| Syntax | Meaning |
|--------|---------|
| `<name>` | Required positional argument |
| `?name?` | Optional positional argument |
| `<name>...` | Variadic required (1 or more) |
| `?name?...` | Variadic optional (0 or more) |

**Note**: Use `?arg?` not `[arg]` because `[]` triggers command substitution in TCL.

### Multi-paragraph Descriptions

Use `\n\n` to separate paragraphs:

```c
"First paragraph about basic functionality.\n\n"
"Second paragraph about special cases.\n\n"
"Third paragraph about edge cases."
```

## Common Patterns

### Simple Command with One Required Argument

```c
void feather_register_return_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Return from a procedure",
    "Causes current procedure to return immediately with specified value.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?value?");
  e = feather_usage_help(ops, interp, e, "The value to return (default: empty string)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "return 42",
    "Return value 42 from procedure",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "return", spec);
}
```

### Command with Feather-Specific Note

```c
void feather_register_set_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Read and write variables",
    "Returns the value of variable varName. If value is specified, then set "
    "the value of varName to value.\n\n"
    "Note: Feather does not support TCL-style arrays. The varName must refer "
    "to a scalar variable. Array syntax like \"myArray(key)\" is not supported.");
  spec = feather_usage_add(ops, interp, spec, e);

  // ... rest of implementation
}
```

### Command with Multiple Arguments

```c
// For a command like: lrange list first last
e = feather_usage_arg(ops, interp, "<list>");
e = feather_usage_help(ops, interp, e, "The list to extract elements from");
spec = feather_usage_add(ops, interp, spec, e);

e = feather_usage_arg(ops, interp, "<first>");
e = feather_usage_help(ops, interp, e, "Index of first element");
spec = feather_usage_add(ops, interp, spec, e);

e = feather_usage_arg(ops, interp, "<last>");
e = feather_usage_help(ops, interp, e, "Index of last element");
spec = feather_usage_add(ops, interp, spec, e);
```

## Key Files

| File | Purpose |
|------|---------|
| `src/builtin_<command>.c` | Add `feather_register_<command>_usage()` function here |
| `src/internal.h` | Add function declaration |
| `src/interp.c` | Call registration function in `feather_register_usage()` |
| `src/builtin_usage.c` | Core usage system implementation |

## Important Reminders

1. **Do not copy TCL docs verbatim** - Feather has significant differences
2. **Explicitly note array limitations** - Arrays are not supported
3. **Verify cross-references** - Only reference commands that exist/will exist
4. **Use paragraph breaks** - `\n\n` for readable multi-paragraph help
5. **Test the output** - Always view the rendered help before committing
6. **Keep it accurate** - Help must match actual Feather behavior

## Example Session

```bash
# 1. Check current state
$ echo "usage help set" | ./bin/feather-tester
no usage defined for "set"

# 2. Add usage function to src/builtin_set.c
# 3. Add declaration to src/internal.h
# 4. Add registration call in src/interp.c

# 5. Build and test
$ mise build
$ echo "usage help set" | ./bin/feather-tester
set(1)                    General Commands Manual                   set(1)

NAME
       set - Read and write variables
...

# 6. Commit
$ git add -A
$ git commit -m "Add usage help for set command"
```
