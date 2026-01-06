# Completion System - Clean Design (v2)

## Problems with Current Implementation

1. **Conflated concepts**: The variable `prefix` serves two purposes:
   - Determining argument count (is user typing an argument?)
   - Filtering completions (what should results match?)

2. **Unclear position semantics**: Position 13 in `"cmd file1.txt "` (length 14) is ambiguous
   - Is user completing "file1.txt"? (no trailing space in slice)
   - Or starting a new token? (trailing space exists in original)

3. **No cursor context**: Don't distinguish between:
   - `"cmd fi|le"` - cursor IN a token (complete the token)
   - `"cmd file|"` - cursor AT END of token (complete the token)
   - `"cmd file |"` - cursor AFTER token (complete next token)

4. **Simplistic tokenization**: Splits on whitespace, ignores braces/quotes/escaping

5. **Duplicated logic**: Same flag_prefix workaround in Case 2 and Case 3

## Core Insight: Two Different "Prefixes"

The fundamental problem is conflating two concepts:

```c
// CURRENT (WRONG):
FeatherObj prefix;  // Used for BOTH filtering and context

// CORRECT:
FeatherObj filter_prefix;   // What to filter completions by
FeatherObj complete_tokens; // What tokens exist for context
```

## Clean Architecture

### Phase 1: Parse and Analyze Context

```c
typedef struct {
    /* Complete tokens (have trailing whitespace) */
    FeatherObj complete_tokens;    // List: ["cmd", "file1.txt"]

    /* Partial token being typed (empty if cursor after space) */
    FeatherObj partial_token;      // String: "fi" or ""

    /* Position metadata */
    size_t partial_token_start;    // Where partial token starts
    int at_token_boundary;         // 1 if cursor right after space/separator
} CompletionContext;
```

**Example:**
```
Input: "cmd file1.txt fi|le" at position 17
Result:
  complete_tokens = ["cmd", "file1.txt"]
  partial_token = "fi"
  at_token_boundary = 0

Input: "cmd file1.txt |" at position 14
Result:
  complete_tokens = ["cmd", "file1.txt"]
  partial_token = ""
  at_token_boundary = 1
```

### Phase 2: Determine Completion State

```c
typedef enum {
    COMPLETE_COMMAND,      // No tokens, completing command name
    COMPLETE_SUBCOMMAND,   // After command, spec has subcommands
    COMPLETE_FLAG_VALUE,   // Previous token is flag expecting value
    COMPLETE_ARGUMENT,     // Completing positional argument or flag
} CompletionState;

typedef struct {
    CompletionState state;

    /* The spec we're completing against */
    FeatherObj active_spec;

    /* Tokens relevant to this spec (excludes parent commands) */
    FeatherObj spec_tokens;

    /* If completing flag value, which flag? */
    FeatherObj expecting_value_for_flag;  // Empty if not completing flag value
} CompletionStateInfo;
```

### Phase 3: Generate Candidates

```c
/* Generate completions WITHOUT filtering */
static FeatherObj generate_completions(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    CompletionStateInfo *state_info
) {
    switch (state_info->state) {
        case COMPLETE_COMMAND:
            return generate_all_commands(ops, interp);

        case COMPLETE_SUBCOMMAND:
            return generate_all_subcommands(ops, interp, state_info->active_spec);

        case COMPLETE_FLAG_VALUE:
            return generate_flag_choices(ops, interp, state_info->expecting_value_for_flag);

        case COMPLETE_ARGUMENT:
            // Generate BOTH placeholders and flags
            FeatherObj result = generate_arg_placeholders(ops, interp,
                state_info->active_spec, state_info->spec_tokens);
            FeatherObj flags = generate_all_flags(ops, interp, state_info->active_spec);
            return combine_lists(ops, interp, result, flags);
    }
}
```

### Phase 4: Filter by Prefix

```c
/* Filter completions by prefix match */
static FeatherObj filter_completions(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj candidates,
    FeatherObj filter_prefix
) {
    /* Empty prefix matches everything */
    if (ops->string.byte_length(interp, filter_prefix) == 0) {
        return candidates;
    }

    FeatherObj result = ops->list.create(interp);
    size_t num = ops->list.length(interp, candidates);

    for (size_t i = 0; i < num; i++) {
        FeatherObj candidate = ops->list.at(interp, candidates, i);
        FeatherObj text = dict_get_str(ops, interp, candidate, K_TEXT);

        /* Check prefix match */
        if (obj_has_prefix(ops, interp, text, filter_prefix)) {
            result = ops->list.push(interp, result, candidate);
        }
    }

    return result;
}
```

## Main Flow

```c
static FeatherObj usage_complete_impl_v2(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj scriptObj,
    size_t pos
) {
    /* Phase 1: Parse script and determine cursor context */
    CompletionContext ctx = parse_completion_context(ops, interp, scriptObj, pos);

    /* Phase 2: Determine what we're completing */
    CompletionStateInfo state = determine_completion_state(ops, interp, &ctx);

    /* Phase 3: Generate all candidates (unfiltered) */
    FeatherObj candidates = generate_completions(ops, interp, &state);

    /* Phase 4: Filter by partial token */
    FeatherObj filtered = filter_completions(ops, interp, candidates, ctx.partial_token);

    return filtered;
}
```

## Key Improvements

### 1. Clear Separation of Concerns

Each phase has a single responsibility:
- **Parse**: Extract tokens and cursor position
- **State**: Determine what kind of completion
- **Generate**: Create all possible candidates
- **Filter**: Match against user input

### 2. No Special Cases

The old code had:
```c
// BEFORE: Hack to fix flags
FeatherObj flag_prefix = prefix;
if (!token_is_flag(ops, interp, prefix)) {
    flag_prefix = ops->string.intern(interp, "", 0);
}
FeatherObj flags = complete_flags(ops, interp, parsedSpec, flag_prefix);
```

The new code doesn't need this because:
- Generation phase creates ALL flags
- Filter phase matches them against partial_token
- If partial_token is "file1.txt", no flags match → empty result
- If partial_token is "", all flags match → return all flags
- If partial_token is "-v", matching flags returned

### 3. Cursor Position Semantics

```c
static CompletionContext parse_completion_context(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj scriptObj,
    size_t pos
) {
    CompletionContext ctx;
    ctx.complete_tokens = ops->list.create(interp);

    /* Find last command separator before pos */
    size_t cmd_start = find_command_start(ops, interp, scriptObj, pos);

    /* Tokenize from cmd_start to pos */
    size_t i = cmd_start;
    size_t last_token_end = cmd_start;

    while (i < pos) {
        /* Skip whitespace */
        while (i < pos && is_whitespace(scriptObj, i)) {
            i++;
        }

        if (i >= pos) {
            /* Cursor is at whitespace - no partial token */
            ctx.partial_token = ops->string.intern(interp, "", 0);
            ctx.partial_token_start = pos;
            ctx.at_token_boundary = 1;
            break;
        }

        /* Extract token */
        size_t token_start = i;
        while (i < pos && !is_whitespace(scriptObj, i)) {
            i++;
        }

        FeatherObj token = ops->string.slice(interp, scriptObj, token_start, i);

        if (i < pos) {
            /* Token is complete (has trailing whitespace before pos) */
            ctx.complete_tokens = ops->list.push(interp, ctx.complete_tokens, token);
            last_token_end = i;
        } else {
            /* Cursor is in/at this token - it's partial */
            ctx.partial_token = token;
            ctx.partial_token_start = token_start;
            ctx.at_token_boundary = 0;
        }
    }

    return ctx;
}
```

**Example behavior:**
```
"cmd file.txt |" at pos 14
  -> complete_tokens = ["cmd", "file.txt"]
  -> partial_token = ""
  -> Generates: all flags and next placeholder
  -> Filters by "": returns all flags and placeholder

"cmd file.txt -|v" at pos 15
  -> complete_tokens = ["cmd", "file.txt"]
  -> partial_token = "-"
  -> Generates: all flags and placeholders
  -> Filters by "-": returns only flags

"cmd file.txt -v|" at pos 16
  -> complete_tokens = ["cmd", "file.txt"]
  -> partial_token = "-v"
  -> Generates: all flags and placeholders
  -> Filters by "-v": returns flags starting with "-v"
```

### 4. Argument Counting Logic

```c
static FeatherObj generate_arg_placeholders(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj spec,
    FeatherObj tokens  // Only tokens relevant to this spec
) {
    /* Count positional arguments provided */
    size_t pos_arg_count = 0;
    size_t num_tokens = ops->list.length(interp, tokens);

    for (size_t i = 0; i < num_tokens; i++) {
        FeatherObj token = ops->list.at(interp, tokens, i);

        if (token_is_flag(ops, interp, token)) {
            /* Check if flag consumes next token */
            FeatherObj flag_entry = find_flag_entry(ops, interp, spec, token);
            int has_value = dict_get_int(ops, interp, flag_entry, K_HAS_VALUE);
            if (has_value && i + 1 < num_tokens) {
                i++;  // Skip flag value
            }
        } else {
            /* Positional argument */
            pos_arg_count++;
        }
    }

    /* NOTE: partial_token is NOT included in tokens, so we don't count it here.
     * This is correct because:
     * - If user typed "cmd file.txt|", partial="file.txt" shouldn't count (cursor IN it)
     * - If user typed "cmd file.txt |", tokens=["cmd","file.txt"], partial=""
     *   and file.txt already counted
     */

    /* Generate placeholders based on pos_arg_count */
    return generate_placeholders_for_position(ops, interp, spec, pos_arg_count);
}
```

### 5. No Duplicated Logic

Both "Case 2" and "Case 3" from the old implementation now use the same code path:
```c
CompletionStateInfo state = determine_completion_state(ops, interp, &ctx);
FeatherObj candidates = generate_completions(ops, interp, &state);
```

The state machine handles whether we have subcommands, flags, values, etc.

## Advanced: Proper TCL Tokenization

For v3, we could use the actual parser:

```c
static CompletionContext parse_completion_context_v3(
    const FeatherHostOps *ops,
    FeatherInterp interp,
    FeatherObj scriptObj,
    size_t pos
) {
    /* Use the real TCL parser to tokenize */
    FeatherParseState state;
    feather_parse_init(&state, ops, interp, scriptObj);

    CompletionContext ctx;
    ctx.complete_tokens = ops->list.create(interp);

    while (state.pos < pos) {
        FeatherObj token;
        FeatherResult result = feather_parse_word(&state, &token);

        if (result == TCL_ERROR) {
            /* Incomplete token at cursor - this is partial_token */
            ctx.partial_token = token;
            ctx.partial_token_start = state.word_start;
            break;
        }

        /* Check if there's whitespace after this token before pos */
        if (state.pos < pos) {
            /* Complete token */
            ctx.complete_tokens = ops->list.push(interp, ctx.complete_tokens, token);
        } else {
            /* Cursor is in this token */
            ctx.partial_token = token;
            ctx.partial_token_start = state.word_start;
        }
    }

    return ctx;
}
```

This would correctly handle:
- `cmd "file with spaces.txt" |` - quoted strings
- `cmd {nested {braces}} |` - brace grouping
- `cmd file\ with\ spaces.txt |` - backslash escaping

## Migration Path

1. **Phase 1**: Implement new architecture alongside old one
   - Keep `usage_complete_impl()` as-is
   - Add `usage_complete_impl_v2()` with new design
   - Add test flag to switch between implementations

2. **Phase 2**: Validate with existing tests
   - Run test suite with v2 implementation
   - Fix any regressions
   - Add tests for edge cases (quotes, braces, escaping)

3. **Phase 3**: Replace old implementation
   - Remove `usage_complete_impl()`
   - Rename `usage_complete_impl_v2()` → `usage_complete_impl()`
   - Clean up old helper functions

## Summary

The key insight is: **separate filtering from context**.

- **Context** (complete_tokens): What command/arguments have been provided?
- **Filter** (partial_token): What string are we matching against?

This eliminates all the special cases and makes the code:
- Easier to understand (each phase has one job)
- Easier to test (test each phase independently)
- Easier to extend (add new completion states without touching existing code)
- More correct (proper handling of cursor position semantics)
