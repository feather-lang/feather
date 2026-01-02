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
- **`break` command**: Exits the loop immediately when invoked in `body` or `next`
- **`continue` command**: Skips remaining commands in `body`, but still executes `next` before the next iteration (note: `continue` in `next` is treated as an error per TCL behavior)
- **Error propagation**: Errors in any of the scripts are properly propagated
- **Empty string return**: Returns an empty string on normal completion (per TCL spec)
- **Argument validation**: Reports an error if the wrong number of arguments is provided

## TCL Features We Do NOT Support

Our implementation covers all core functionality specified in the TCL manual for the `for` command.

## Notes on Implementation Differences

1. **`break`/`continue` in `next` script**: Per TCL behavior:
   - `break` in `body`: Works correctly (exits loop)
   - `break` in `next`: Works correctly (exits loop)
   - `continue` in `next`: Produces error "invoked 'continue' outside of a loop" (matches TCL)

2. **`continue` handling**: Our implementation correctly handles `continue` - it skips the rest of `body` but still executes `next` before the next iteration. This matches TCL behavior.

3. **Condition evaluation**: We use the `expr` builtin to evaluate the test expression, which is consistent with TCL. The helper function `eval_for_condition` handles the expression evaluation and boolean conversion.

4. **Local scope evaluation**: Our implementation uses `TCL_EVAL_LOCAL` for evaluating scripts, which should maintain proper variable scoping within the loop.
