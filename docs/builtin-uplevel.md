# builtin: uplevel

## Summary of Our Implementation

The `uplevel` command in Feather evaluates a script in a different stack frame context. Our implementation is located in `src/builtin_uplevel.c` with level parsing logic in `src/level_parse.c`.

**Syntax:** `uplevel ?level? command ?arg ...?`

Our implementation:

1. Parses an optional level specifier (relative `N` or absolute `#N`)
2. Defaults to level `1` (caller's frame) when level is omitted
3. Concatenates multiple arguments with spaces to form the script
4. Temporarily switches the active frame to the target level
5. Evaluates the script in that frame's variable context
6. Restores the original frame after evaluation
7. Returns the result of the script evaluation

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Relative level (`N`) | Supported | Moves N levels up the call stack |
| Absolute level (`#N`) | Supported | Targets absolute stack frame number |
| Default level of `1` | Supported | Used when level is omitted |
| Multiple argument concatenation | Supported | Arguments are joined with spaces |
| Level defaulting prevention | Supported | If first arg looks like a level (starts with `#` or is numeric), it is parsed as a level |
| Variable context switching | Supported | Script executes with target frame's variables |
| Result propagation | Supported | Returns result of evaluated script |
| Error propagation | Supported | Errors from script evaluation are propagated |

## Additional Supported Features

| Feature | Status | Notes |
|---------|--------|-------|
| Namespace interaction | **Supported** | Namespace eval and procs add call frames correctly |
| `apply` command interaction | **Supported** | Apply adds call frames that count for uplevel |
| Concat-style argument joining | **Supported** | Whitespace trimming and list handling work correctly |

All major uplevel features are now implemented and match TCL behavior.

## Notes on Implementation Differences

1. **Frame Management:** Our implementation uses `ops->frame.set_active()` to temporarily switch the active frame, then restores it after evaluation. This is conceptually similar to TCL's behavior where the invoking procedure "disappears" from the stack during execution.

2. **Level Validation:** We validate that:
   - Relative levels do not exceed the current level
   - Absolute levels (`#N`) exist within the stack bounds
   - Level specifiers are well-formed integers

3. **Script Evaluation:** We evaluate with `TCL_EVAL_LOCAL` flag, ensuring variables are resolved in the target frame's context.

4. **Error Messages:** Our error messages match TCL's format: `wrong # args: should be "uplevel ?level? command ?arg ...?"` and `bad level "X"`.

5. **Level Detection Heuristic:** When determining if the first argument is a level specifier, we check:
   - If it starts with `#` (absolute level)
   - If it consists purely of digits (relative level)

   This matches TCL's behavior where the level cannot be defaulted if the first command argument is an integer or starts with `#`.
