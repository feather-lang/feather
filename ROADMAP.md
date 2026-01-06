# usage complete - Command Completion Implementation

## What We Want

Add a new subcommand to the `usage` builtin:

```tcl
usage complete script pos
```

Returns a list of completion candidates at position `pos` in `script` as a list of dicts.

### Completion Context

Completions are offered:
- At top-level command positions
- Inside arguments marked with `type script` in usage specs
- Recursively through nested script contexts

The parser determines command positions following standard TCL parsing rules (after whitespace, newlines, semicolons, inside braces when parent arg is `type script`).

### Return Format

Returns a list of dicts, each representing a completion candidate:

```tcl
{
  {text <string> type <category> help <description>}
  {text <string> type <category> help <description>}
  ...
}
```

For argument placeholders (where user must provide a value):
```tcl
{
  {text {} type arg-placeholder name <arg-name> help <description>}
  ...
}
```

### Completion Types

| Type | Description | When Offered |
|------|-------------|--------------|
| `command` | Top-level command | At command position, empty or partial prefix |
| `subcommand` | Subcommand from usage spec | After parent command/subcommand |
| `flag` | Flag option | When flags are valid (always, except when flag expects value) |
| `value` | Argument/flag value from `choices` | When completing arg/flag that has choices |
| `arg-placeholder` | Indicates expected argument | When positional arg expected |

### Completion Rules

1. **Command Completion**
   - Source: Only commands with registered usage specs
   - Matching: Case-sensitive prefix match
   - Ordering: Alphabetical (since top-level commands aren't in spec order)
   - At position 0 or empty prefix: return all registered commands

2. **Subcommand Completion**
   - Only offer subcommands valid in current context (after parent has been matched)
   - Ordering: Preserved from spec order
   - Matching: Prefix match against subcommand name

3. **Flag Completion**
   - Always offer flags when valid (even without `-` prefix)
   - Exception: Don't offer flags when a flag is expecting its value
   - Include both short and long forms as separate entries

4. **Value Completion**
   - When argument or flag has `choices` defined, offer those as completions
   - Inherit parent help text for value completions
   - Only return choices, not other flags/args

5. **Argument Placeholders**
   - Offer required arg placeholders when that positional arg is expected
   - Offer optional arg placeholders alongside flags
   - Variadic args: offer placeholder only once (after first occurrence, stop suggesting)

6. **Filtering by `hide`**
   - Exclude entries marked with `hide` from completions

7. **Type Hints**
   - Arguments with `type file`, `type dir`, etc. are not completed by Feather
   - Host should intercept these cases and provide filesystem-based completions
   - Document this behavior for embedders

### Examples

#### Basic Command Completion
```tcl
usage for puts {arg <text>}
usage for proc {arg <name> arg <args> arg <body> {type script}}

usage complete {pu} 2
# => {{text puts type command help {}}}

usage complete {} 0
# => {{text proc type command help {}}
#     {text puts type command help {}}
#     ...all registered commands...}
```

#### Subcommand Completion
```tcl
usage for git {
    cmd clone {arg <repo>}
    cmd commit {flag -m --message <msg>}
    cmd checkout {arg <branch>}
}

usage complete {git co} 6
# => {{text commit type subcommand help {}}
#     {text checkout type subcommand help {}}}

usage complete {git commit } 11
# => {{text -m type flag help {}}
#     {text --message type flag help {}}
#     {text {} type arg-placeholder name ... help {}}}
```

#### Flag Completion
```tcl
usage for compile {
    arg <source>
    flag --verbose {help {Enable verbose output}}
    flag -O --optimize ?level? {help {Optimization level}}
}

usage complete {compile file.c } 15
# => {{text --verbose type flag help {Enable verbose output}}
#     {text -O type flag help {Optimization level}}
#     {text --optimize type flag help {Optimization level}}}

usage complete {compile file.c -} 16
# => {{text --verbose type flag help {Enable verbose output}}
#     {text -O type flag help {Optimization level}}
#     {text --optimize type flag help {Optimization level}}}
```

#### Value Completion (choices)
```tcl
usage for compile {
    flag --format <fmt> {
        help {Output format}
        choices {json yaml toml}
    }
}

usage complete {compile --format } 18
# => {{text json type value help {Output format}}
#     {text yaml type value help {Output format}}
#     {text toml type value help {Output format}}}

usage complete {compile --format j} 19
# => {{text json type value help {Output format}}}
```

#### Script-Type Argument Completion
```tcl
usage for if {
    arg <condition>
    arg <body> {type script}
}

usage for puts {arg <text>}

usage complete {if {$x > 0} {pu}} 17
# => {{text puts type command help {}}}
```

#### Nested Script Completion
```tcl
usage for while {
    arg <condition>
    arg <body> {type script}
}

usage complete {while {1} {\n  if {$x} {\n    se}} 30
# => {{text set type command help {}}}
```

#### Argument Placeholder
```tcl
usage for cmd {
    arg <input> {help {Input file}}
    arg ?output? {help {Output file}}
    flag --verbose
}

usage complete {cmd } 4
# => {{text {} type arg-placeholder name input help {Input file}}
#     {text --verbose type flag help {}}}

usage complete {cmd file.txt } 13
# => {{text {} type arg-placeholder name output help {Output file}}
#     {text --verbose type flag help {}}}
```

#### Variadic Arguments
```tcl
usage for cmd {
    arg <files>... {help {Input files}}
    flag --verbose
}

usage complete {cmd file1.txt } 13
# => {{text --verbose type flag help {}}}
# Note: No more placeholder for files (already satisfied once)
```

---

## How We Get There

### Step 1: Add Completion Entry Structure

**File:** `src/builtin_usage.c`

Add internal functions to build completion entries:

```c
// Create completion entry dict
static FeatherObj make_completion(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    const char *text,
    const char *type,
    FeatherObj help
);

// Create arg placeholder entry
static FeatherObj make_arg_placeholder(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    const char *name,
    FeatherObj help
);
```

These build dicts with the format: `{text <str> type <type> help <help>}` or `{text {} type arg-placeholder name <name> help <help>}`.

### Step 2: Implement Command Position Detection

**File:** `src/builtin_usage.c`

Parse the script to determine what context the cursor is in:

```c
typedef struct {
    int at_command_position;        // Cursor is where a command can start
    FeatherObj partial_token;       // The partial text being completed
    size_t token_start;             // Start position of current token
    FeatherObj parent_command;      // The command being invoked (if in args)
    FeatherObj matched_subcommands; // List of matched subcommands (path)
    int in_flag_value;              // Cursor is completing a flag value
    FeatherObj current_flag;        // The flag expecting a value (if any)
} CompletionContext;

static CompletionContext analyze_completion_context(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    const char *script,
    size_t script_len,
    size_t pos
);
```

This function:
1. Tokenizes the script up to `pos` using the parser
2. Determines if we're at a command position
3. Tracks which command/subcommands have been matched
4. Detects if we're completing a flag value
5. Extracts the partial token being completed

### Step 3: Implement Command Completion

**File:** `src/builtin_usage.c`

When at command position, complete from registered commands:

```c
static FeatherObj complete_commands(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj prefix
);
```

This function:
1. Gets all registered usage specs from `::usage::specs`
2. Filters by prefix match (case-sensitive)
3. Returns list of `{text <cmd> type command help <...>}` dicts
4. Sorted alphabetically

### Step 4: Implement Subcommand Completion

**File:** `src/builtin_usage.c`

When inside a command with subcommands, complete from spec:

```c
static FeatherObj complete_subcommands(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj spec,
    FeatherObj prefix
);
```

This function:
1. Walks the spec's `cmd` entries
2. Filters by prefix match
3. Respects `hide` flag
4. Returns entries in spec order
5. Returns `{text <subcmd> type subcommand help <...>}` dicts

### Step 5: Implement Flag Completion

**File:** `src/builtin_usage.c`

Offer available flags from current spec:

```c
static FeatherObj complete_flags(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj spec,
    FeatherObj prefix,
    FeatherObj already_provided // dict of flag_name -> 1 for flags already in args
);
```

This function:
1. Walks the spec's `flag` entries
2. Skips flags already provided (unless they're variadic - future extension)
3. Returns both short and long forms as separate entries
4. Filters by prefix match
5. Respects `hide` flag
6. Returns `{text <flag> type flag help <...>}` dicts

### Step 6: Implement Value Completion (choices)

**File:** `src/builtin_usage.c`

When completing an argument or flag value with choices:

```c
static FeatherObj complete_choices(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj entry, // arg or flag entry with choices
    FeatherObj prefix
);
```

This function:
1. Extracts `choices` from entry
2. Filters by prefix match
3. Inherits `help` from parent entry
4. Returns `{text <choice> type value help <...>}` dicts

### Step 7: Implement Argument Placeholder Generation

**File:** `src/builtin_usage.c`

Determine which positional args are expected:

```c
static FeatherObj get_arg_placeholders(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj spec,
    size_t args_provided // number of positional args provided so far
);
```

This function:
1. Walks `arg` entries in spec
2. Determines which args still need values:
   - If required and not yet provided: include
   - If optional and next in sequence: include
   - If variadic and not yet provided: include once
3. Returns `{text {} type arg-placeholder name <name> help <...>}` dicts

### Step 8: Implement Script-Type Recursion

**File:** `src/builtin_usage.c`

When cursor is inside a script-type argument, recurse:

```c
static int is_inside_script_arg(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    CompletionContext *ctx,
    FeatherObj spec,
    size_t *script_start_pos // OUT: where the script arg starts
);
```

This function:
1. Checks if current argument position corresponds to a `type script` arg
2. If yes, calculates the position within that script argument
3. Recursively calls completion logic with the adjusted position

### Step 9: Implement Main Completion Logic

**File:** `src/builtin_usage.c`

Tie everything together:

```c
static FeatherObj usage_complete_impl(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    const char *script,
    size_t script_len,
    size_t pos
);
```

Algorithm:
1. Parse script and analyze context (Step 2)
2. If at command position:
   - If no parent command: complete top-level commands (Step 3)
   - If parent has subcommands: complete subcommands (Step 4)
3. If in flag value: complete choices for that flag (Step 6)
4. If in argument position:
   - If arg has choices: complete choices (Step 6)
   - Otherwise: combine flags (Step 5) + arg placeholders (Step 7)
5. If inside script-type argument: recurse (Step 8)
6. Filter all results by prefix match
7. Return combined list

### Step 10: Add Subcommand Handler

**File:** `src/builtin_usage.c`

Add `usage complete` to the subcommand dispatcher:

```c
// In feather_builtin_usage():
if (feather_obj_eq_literal(ops, interp, subcmd, "complete")) {
    // usage complete script pos
    if (argc != 2) {
        *err = ops->string.intern(interp,
            S("usage: usage complete script pos"));
        return FEATHER_ERR;
    }

    // Get script string
    size_t script_len = 0;
    const char *script = ops->string.get_bytes(interp, argv[0], &script_len);

    // Get position
    int64_t pos_i = 0;
    if (!ops->integer.get(interp, argv[1], &pos_i) || pos_i < 0) {
        *err = ops->string.intern(interp,
            S("usage complete: pos must be a non-negative integer"));
        return FEATHER_ERR;
    }
    size_t pos = (size_t)pos_i;

    if (pos > script_len) {
        pos = script_len; // Clamp to end
    }

    *result = usage_complete_impl(ops, interp, script, script_len, pos);
    return FEATHER_OK;
}
```

### Step 11: Add Documentation

**File:** `docs/builtin-usage.md`

Add section documenting `usage complete`:
- Syntax and return format
- All completion types
- Examples for each scenario
- Notes for embedders (handling placeholders, type hints)

### Step 12: Add Test Cases

**File:** `harness/testdata/usage-complete.html` (new file)

Create comprehensive test suite covering:
- Command completion at various positions
- Subcommand completion with nesting
- Flag completion (with and without prefix)
- Value completion from choices
- Argument placeholders
- Script-type recursion
- Variadic argument handling
- Hide flag filtering
- Edge cases (empty script, position at end, position 0)

Test format:
```html
<test id="complete-command-prefix">
  <script>
    usage for puts {arg <text>}
    usage for proc {arg <name>}
    usage complete {pu} 2
  </script>
  <expected>{{text puts type command help {}}}</expected>
</test>
```

### Step 13: Update Public C API

**File:** `src/feather.h`

Add declaration for programmatic completion (for embedders):

```c
/**
 * Get completion candidates at a position in a script.
 *
 * Returns a list of dicts with completion information.
 * See docs/builtin-usage.md for dict format.
 *
 * @param ops Host operations
 * @param interp Interpreter
 * @param script Script text
 * @param script_len Length of script
 * @param pos Cursor position (byte offset)
 * @return List of completion candidate dicts
 */
FeatherObj feather_usage_complete(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    const char *script,
    size_t script_len,
    size_t pos
);
```

### Step 14: Manual Testing and Refinement

Build and test with real usage:
1. `mise build` to compile
2. `mise test` to run test suite
3. Create test scripts with various usage specs
4. Manually verify completion behavior
5. Test in the raylib game example (add console with completion UI)
6. Iterate on edge cases and UX

### Step 15: Commit

Create commit with:
- Full implementation
- Documentation updates
- Test suite
- Commit message documenting design decisions and learnings
