# lmap Builtin Comparison

This document compares feather's `lmap` implementation against the official TCL manual.

## Summary of Our Implementation

Our `lmap` implementation in `src/builtin_lmap.c` provides a mapping construct that iterates over one or more lists, executing a body script for each iteration and collecting results into an accumulator list.

The implementation:
- Accepts the syntax `lmap varList list ?varList list ...? command`
- Requires a minimum of 3 arguments (varlist, list, body) with an odd total count
- Validates that all varlists are non-empty
- Calculates the maximum number of iterations based on list lengths divided by variable counts
- Assigns empty strings to variables when lists are exhausted
- Appends body results to an accumulator list on normal completion
- Returns the accumulated list at the end

## TCL Features We Support

1. **Basic single-variable iteration**: `lmap varname list body`
   - Iterates through a list, assigning each element to a variable

2. **Multiple variables per list**: `lmap {a b} list body`
   - Assigns consecutive list elements to multiple variables per iteration

3. **Multiple varlist/list pairs**: `lmap var1 list1 var2 list2 body`
   - Supports parallel iteration over multiple lists

4. **Empty value padding**: When a list runs out of elements, remaining variables receive empty strings

5. **break statement**: Exits the loop early, returning accumulated results so far

6. **continue statement**: Skips appending the current body result to the accumulator and proceeds to the next iteration

7. **Error propagation**: Errors in the body script are propagated appropriately

8. **return propagation**: Return statements in the body are propagated

## TCL Features We Do NOT Support

After careful comparison with the TCL manual, our implementation appears to cover all documented features of `lmap`. No missing features were identified.

## Notes on Implementation Differences

1. **Element extraction semantics**: The TCL manual states that element assignment uses semantics "as if the `lindex` command had been used to extract the element." Our implementation uses `ops->list.at()` directly. This should be functionally equivalent for well-formed lists, but may differ in edge cases involving nested list parsing or special characters.

2. **Variable scope**: Our implementation uses `ops->var.set()` for variable assignment. The TCL manual does not explicitly specify scoping rules for `lmap`, but standard TCL behavior would have variables set in the current scope. Our `TCL_EVAL_LOCAL` flag in body evaluation suggests we follow similar semantics.

3. **Iteration count calculation**: Our implementation calculates iterations as `ceiling(listLen / numVars)` and takes the maximum across all pairs. This matches the TCL specification that "the total number of loop iterations is large enough to use up all the values from all the value lists."

4. **Result accumulation on continue**: The TCL manual explicitly states that when `break` or `continue` is invoked, "the body does not complete normally and the result is not appended to the accumulator list." Our implementation correctly handles this for both `break` (exits loop) and `continue` (skips append, continues iteration).
