# subst Builtin Comparison

This document compares feather's `subst` implementation with the TCL 9 reference.

## Summary of Our Implementation

Our `subst` implementation in `src/builtin_subst.c` performs variable, command, and backslash substitutions on a string argument. It supports all three optional flags (`-nobackslashes`, `-nocommands`, `-novariables`) to selectively disable specific substitution types.

Key implementation details:
- Backslash substitution handles: `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\\`, `\newline`, `\xNN` (hex), `\NNN` (octal), `\uNNNN` (16-bit Unicode), and `\UNNNNNNNN` (32-bit Unicode)
- Variable substitution handles: `$name`, `${name}`, `$name(index)` (array syntax with index substitution)
- Command substitution handles: `[command]` with proper bracket nesting, brace escaping, and quote handling
- Exception handling: `break`, `continue`, `return`, and custom return codes are caught and handled

## TCL Features We Support

1. **Core Substitution Types**
   - Backslash substitution (with common escape sequences)
   - Variable substitution (simple variables and arrays)
   - Command substitution (nested bracket evaluation)

2. **Option Flags**
   - `-nobackslashes` - disables backslash substitution
   - `-nocommands` - disables command substitution
   - `-novariables` - disables variable substitution

3. **Variable Syntax**
   - Simple variables: `$name`
   - Braced variables: `${name}`
   - Namespace-qualified variables: `$namespace::name`
   - Array variables with index: `$name(index)`
   - Index substitution within array syntax

4. **Backslash Escape Sequences**
   - Standard escapes: `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\\`
   - Backslash-newline: `\<newline>` collapses to single space (including trailing whitespace)
   - Hexadecimal: `\xNN` (up to 2 hex digits)
   - Octal: `\NNN` (up to 3 octal digits)
   - **Unicode 16-bit**: `\uNNNN` (exactly 4 hex digits, e.g., `\u00A9` → ©)
   - **Unicode 32-bit**: `\UNNNNNNNN` (exactly 8 hex digits, e.g., `\U0001F44B` → 👋)

5. **Exception Handling in Command Substitution**
   - `break` - stops substitution, returns result up to that point
   - `continue` - substitutes empty string for that command, continues processing
   - `return` - substitutes the returned value
   - Custom return codes (>= 5) - substitutes the returned value

6. **Proper Nesting**
   - Nested brackets in command substitution
   - Braces within command substitution
   - Quoted strings within command substitution

## TCL Features We Do NOT Support

1. **Variable Substitution Exception Handling**
   - TCL manual states that `continue` and `break` during variable substitution should be handled similarly to command substitution
   - Our implementation does not support variable read traces that could throw exceptions
   - Variable traces are not implemented, so this is moot for now

2. **Hexadecimal Digit Limit**
   - TCL allows unlimited hex digits after `\x`, using only the last two
   - Our implementation limits to 2 hex digits after `\x`

## Notes on Implementation Differences

1. **Double Substitution Warning**
   - The TCL manual notes that the string is "substituted twice" - once by the parser and once by subst
   - In our implementation, when `subst` is called from within a script, the parser substitutes first, then `subst` substitutes the result
   - This matches TCL behavior

2. **No Special Treatment of Braces/Quotes**
   - TCL manual explicitly states: "subst does not give any special treatment to double quotes or curly braces (except within command substitutions)"
   - Our implementation correctly follows this behavior - braces and quotes in the input string are treated as literal characters outside of command substitution context

3. **Error Handling**
   - TCL returns errors that occur during substitution
   - Our implementation does this correctly, propagating errors from variable lookups and command evaluation

4. **Index Substitution in Arrays**
   - When processing `$name(index)`, our implementation recursively substitutes the index portion
   - This matches TCL behavior where the index can contain variables and commands

5. **Backslash-Newline Handling**
   - Our implementation replaces `\<newline>` with a single space and consumes following spaces/tabs
   - This matches standard TCL behavior for backslash-newline sequences
