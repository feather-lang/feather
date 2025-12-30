# Feather `switch` Builtin Documentation

## Summary of Our Implementation

The Feather `switch` builtin is implemented in `src/builtin_switch.c`. It provides a pattern-matching construct that evaluates a body script when a pattern matches the given string.

**Supported syntax forms:**

1. Inline pattern-body pairs: `switch ?options? string pattern body ?pattern body ...?`
2. List form: `switch ?options? string {pattern body ?pattern body ...?}`

**Key features implemented:**

- Three matching modes: `-exact` (default), `-glob`, and `-regexp`
- The `--` option to mark end of options
- The `default` pattern that matches anything (must be last)
- Fall-through using `-` as body (allows sharing body among multiple patterns)
- Proper error handling for missing bodies and misplaced `default`

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| `-exact` mode | Supported | Default matching mode |
| `-glob` mode | Supported | Uses glob-style matching via `feather_obj_glob_match()` |
| `-regexp` mode | Supported | Uses `ops->string.regex_match()` |
| `--` option | Supported | Marks end of options |
| `default` pattern | Supported | Must be last pattern |
| Fall-through with `-` | Supported | Body `-` uses next pattern's body |
| Inline syntax | Supported | Separate arguments for patterns and bodies |
| List syntax | Supported | Single braced list containing pattern-body pairs |
| Empty result on no match | Supported | Returns empty string when nothing matches |

## TCL Features We Do NOT Support

| Feature | Description |
|---------|-------------|
| `-nocase` option | Case-insensitive matching. TCL supports this option to perform comparisons in a case-insensitive manner. |
| `-matchvar varName` option | Only valid with `-regexp`. Captures the list of matches (overall match and capturing groups) into the specified variable. |
| `-indexvar varName` option | Only valid with `-regexp`. Captures the list of start/end indices for each match into the specified variable. |

### Details on Missing Features

#### `-nocase`

TCL's `-nocase` option allows case-insensitive pattern matching. For example:

```tcl
switch -nocase -glob $input {
    "Hello*" { puts "matched hello" }
}
```

This would match "HELLO", "hello", "HeLLo", etc. Our implementation does not support this option.

#### `-matchvar`

TCL's `-matchvar` option (only valid with `-regexp`) allows capturing the matched substrings:

```tcl
switch -regexp -matchvar foo $bar {
    {a(b*)c} {
        puts "Found [string length [lindex $foo 1]] 'b's"
    }
}
```

The variable receives a list where:
- Element 0: The overall matched substring
- Element 1..n: Substrings matched by capturing groups

Our implementation does not support capturing match results.

#### `-indexvar`

TCL's `-indexvar` option (only valid with `-regexp`) captures the indices of matches:

```tcl
switch -regexp -indexvar indices $bar {
    {pattern} {
        lassign [lindex $indices 0] start end
    }
}
```

The variable receives a list of two-element lists, each containing `{start end}` indices.

Our implementation does not support capturing match indices.

## Notes on Implementation Differences

1. **Error messages**: Our error messages are similar to TCL's but may not be identical in wording. For example, we report "bad option" for unknown options, matching TCL's style.

2. **Glob matching**: Our glob matching is implemented via `feather_obj_glob_match()`. It supports the core glob patterns (`*`, `?`, `[...]`) but may have subtle differences from TCL's `string match` command in edge cases.

3. **Regexp matching**: Regular expression matching uses the host-provided `ops->string.regex_match()` function. The regex syntax and behavior depend on the host implementation, which may differ from TCL's RE syntax described in `re_syntax`.

4. **Two-argument edge case**: TCL documentation mentions that when there are exactly two arguments (no options starting with `-`), the first is treated as the string and the second as the pattern/body list. Our implementation handles this through the general option parsing logic but may behave slightly differently at the boundary.

5. **Comments in switch**: TCL documentation warns about proper comment placement (only inside body scripts, not between patterns). Our implementation does not explicitly handle or warn about this, as comments are processed by the general script parser.
