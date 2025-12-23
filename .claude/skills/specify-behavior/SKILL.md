---
name: specify-behavior
description: |
  Use this skill to create a detailed specification for a Tcl command.
  The specification describes how the command should be implemented in tclc,
  including required TclHostOps and supported subcommands.
---

# Specify Behavior Skill

This skill creates a detailed specification for implementing a Tcl command in tclc.

## Workflow

1. **View the manual page** for the command:
   ```bash
   man -P cat n <command-name>
   ```

2. **Read the tclc header** to understand the host interface:
   - Review `src/tclc.h` for the `TclHostOps` structure
   - Identify which operations are already available
   - Determine if new operations are needed

3. **Read the ROADMAP.md** to understand tclc's philosophy:
   - No I/O in the language
   - No OO
   - Focus on metaprogramming, introspection, and control flow
   - Host provides memory management and data structures

4. **Create the specification** at `specs/commands/<command-name>.md`

## Specification Template

Use this structure for the specification file:

```markdown
# <command-name>

## Synopsis

<Copy from man page>

## Description

<Brief description of what the command does>

## tclc Scope

<Explain which features are in scope for tclc and which are omitted, with rationale>

## Subcommands

### <subcommand-name>

**Syntax:** `<command> <subcommand> ?args?`

**Description:** <what it does>

**Host Operations Used:**
- `ops->type.operation` - <why needed>

**Return:** <what it returns>

**Errors:** <error conditions>

---

(Repeat for each subcommand)

## Host Interface Requirements

### Existing Operations Used

- `TclListOps.length` - <purpose>
- `TclStringOps.get` - <purpose>
- (etc.)

### New Operations Required

If no new operations are needed:

> No additions to TclHostOps are required.

Otherwise, describe each new operation:

#### `TclTypeOps.new_operation`

```c
/**
 * new_operation does X.
 *
 * @param interp The interpreter instance
 * @param arg Description of argument
 * @return Description of return value
 */
TclResult (*new_operation)(TclInterp interp, TclObj arg);
```

**Rationale:** <Why this operation is needed and cannot be composed from existing ops>

## Implementation Notes

<Any special considerations, edge cases, or interactions with other commands>

## Test Cases

Outline key test cases that should be written:

1. <test case description>
2. <test case description>
3. (etc.)
```

## Philosophy Guidelines

When deciding what to include:

1. **Keep it minimal** - Only include subcommands essential for tclc's goals
2. **Defer I/O to host** - If a subcommand does I/O, omit it
3. **Prefer host operations** - Complex data structure work should use TclHostOps
4. **Support introspection** - Include subcommands that reveal interpreter state
5. **Enable metaprogramming** - Include subcommands that support code-as-data

## Example: Deciding Subcommand Scope

For `string`, you might include:
- `string length` - basic string operation
- `string index` - basic string operation
- `string match` - enables glob-based metaprogramming

But omit:
- `string cat` - can be done with concat
- String encoding operations - I/O concern, host responsibility
