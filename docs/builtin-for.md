# `for` Builtin Comparison

This document compares our implementation of the `for` command with the official TCL specification.

## Summary of Our Implementation

Our `for` builtin is implemented in `src/builtin_for.c`. It provides a C-style `for` loop with the following structure:

```tcl
for start test next body
```

The implementation:
1. Executes the `start` script once at the beginning
2. Evaluates the `test` expression before each iteration
3. If `test` is true (non-zero), executes `body`
4. After `body`, executes the `next` script
5. Repeats from step 2 until `test` evaluates to false (zero)
6. Returns an empty string upon normal completion

## TCL Features We Support

- **Basic loop structure**: All four arguments (`start`, `test`, `next`, `body`) are correctly handled
- **Expression evaluation**: The `test` argument is evaluated as an expression via the `expr` builtin
- **Boolean evaluation**: Supports integer (0 = false, non-zero = true) and boolean literals
- **`break` command**: Exits the loop immediately when invoked in `body`
- **`continue` command**: Skips remaining commands in `body`, but still executes `next` before the next iteration
- **Error propagation**: Errors in any of the scripts are properly propagated
- **Empty string return**: Returns an empty string on normal completion (per TCL spec)
- **Argument validation**: Reports an error if the wrong number of arguments is provided

## TCL Features We Do NOT Support

Based on our analysis, our implementation appears to cover the core functionality specified in the TCL manual. However, there is one subtle behavioral difference:

- **`break` in `next` script**: According to the TCL manual: "If a `break` command is invoked within `body` **or `next`**, then the `for` command will return immediately." Our implementation only handles `break` in the `body` script, not in the `next` script. If `break` is called in `next`, it would be treated as an error rather than a normal loop exit.

## Notes on Implementation Differences

1. **`break`/`continue` in `next` script**: The TCL manual explicitly states that `break` can be invoked in both `body` and `next` to terminate the loop. Our implementation only checks for `TCL_BREAK` after executing `body`, not after executing `next`. This means:
   - `break` in `body`: Works correctly (exits loop)
   - `break` in `next`: Currently propagates as an error
   - `continue` in `next`: Not mentioned in the TCL manual, so undefined behavior is acceptable

2. **`continue` handling**: Our implementation correctly handles `continue` - it skips the rest of `body` but still executes `next` before the next iteration. This matches TCL behavior.

3. **Condition evaluation**: We use the `expr` builtin to evaluate the test expression, which is consistent with TCL. The helper function `eval_for_condition` handles the expression evaluation and boolean conversion.

4. **Local scope evaluation**: Our implementation uses `TCL_EVAL_LOCAL` for evaluating scripts, which should maintain proper variable scoping within the loop.

## Recommended Fix

To fully match TCL behavior, the `break` check should also be added after executing the `next` script:

```c
// Execute the 'next' script (increment/update)
rc = feather_script_eval_obj(ops, interp, next, TCL_EVAL_LOCAL);
if (rc == TCL_BREAK) {
    // break was invoked in next - exit loop normally
    break;
} else if (rc != TCL_OK) {
    return rc;
}
```

This would make `break` work correctly when invoked in the `next` script.
