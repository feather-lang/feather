# lrepeat Builtin Implementation

## Summary of Our Implementation

Our implementation of `lrepeat` is located in `src/builtin_lrepeat.c`. The command creates a list by repeating a sequence of elements a specified number of times.

### Signature

```
lrepeat count ?value ...?
```

### Behavior

1. Validates that at least one argument (count) is provided
2. Parses the count as an integer, returning an error if it is not a valid integer
3. Validates that count is non-negative (>= 0)
4. Returns an empty list if count is 0 or no elements are provided
5. Builds the result list by repeating the provided elements `count` times

## TCL Features We Support

Our implementation supports all features documented in the TCL manual:

| Feature | Supported | Notes |
|---------|-----------|-------|
| Basic repetition (`lrepeat 3 a` -> `a a a`) | Yes | Core functionality works correctly |
| Multiple elements (`lrepeat 3 a b c` -> `a b c a b c a b c`) | Yes | Elements are repeated in sequence |
| Nested lists (`lrepeat 3 [lrepeat 2 a]` -> `{a a} {a a} {a a}`) | Yes | Works via TCL's normal evaluation |
| Zero count (`lrepeat 0 a` -> empty list) | Yes | Returns empty list |
| No elements (`lrepeat 3` -> empty list) | Yes | Returns empty list when no elements given |
| Non-negative integer validation | Yes | Returns error for negative counts |
| Integer parsing validation | Yes | Returns error for non-integer counts |

## TCL Features We Do NOT Support

Based on comparison with the TCL 8.5+ manual, our implementation appears to be **feature-complete**. The TCL manual specifies:

- `count` must be a non-negative integer - **we validate this**
- `element` can be any TCL value - **we support this**
- The result size is `count * number of elements` - **we produce this correctly**

There are no documented features in the TCL manual that our implementation lacks.

## Notes on Implementation Differences

### Error Messages

Our error messages differ slightly from standard TCL:

| Error Condition | Our Message | Standard TCL Message |
|-----------------|-------------|---------------------|
| Wrong # args | `wrong # args: should be "lrepeat count ?value ...?"` | Similar format |
| Non-integer count | `expected integer but got "X"` | `expected integer but got "X"` |
| Negative count | `bad count "X": must be integer >= 0` | `bad count "X": must be integer >= 0` |

### Internal Implementation

1. We use `ops->list.shift` to extract the count argument, which consumes it from the args list
2. We iterate using nested loops: outer loop for count repetitions, inner loop for elements
3. We build the result list incrementally using `ops->list.push`

### Edge Cases Handled

- `lrepeat 0 a b c` -> returns empty list
- `lrepeat 5` -> returns empty list (no elements to repeat)
- `lrepeat -1 a` -> returns error about negative count
- `lrepeat abc a` -> returns error about non-integer count
