# llength Builtin Implementation

## Summary of Our Implementation

The `llength` command in feather is implemented in `src/builtin_llength.c`. It:

1. Validates that exactly one argument is provided
2. Converts the argument to a list representation using `ops->list.from()`
3. Returns the number of elements in the list as an integer

The implementation is straightforward and uses the host operations API to:
- Parse the argument list
- Convert the input to a list
- Get the list length
- Return the result as an integer

## TCL Features We Support

- **Basic list length**: Returns the number of elements in a list
- **Argument validation**: Returns an error with the message `wrong # args: should be "llength list"` when the wrong number of arguments is provided
- **List parsing**: Properly handles list syntax including:
  - Simple word lists: `{a b c}` returns 3
  - Empty lists: `{}` returns 0
  - Nested lists: `{a b {c d} e}` returns 4 (the nested `{c d}` counts as one element)
  - Quoted elements with spaces: `{a b { } c d e}` returns 6 (the `{ }` is a single empty-looking element)

## TCL Features We Do NOT Support

Based on comparison with the TCL 9 manual, our implementation appears to support all documented features of `llength`. The TCL manual describes a simple command with the following behavior:

- Takes exactly one argument (a list)
- Returns a decimal string giving the number of elements

Our implementation correctly handles:
- Argument count validation
- List conversion from string representation
- Returning the count as an integer

There are no documented TCL features that our implementation lacks.

## Notes on Implementation Differences

1. **Return type**: TCL documentation specifies the result is "a decimal string". Our implementation uses `ops->integer.create()` which creates an integer object. In TCL, all values are ultimately strings, so this is semantically equivalent due to shimmering - the integer will be converted to a string representation when needed.

2. **List conversion**: Our implementation explicitly converts the input to a list using `ops->list.from()`. This ensures proper parsing of the string as a TCL list, handling all the edge cases like nested braces, quoted elements, and backslash escapes.

3. **Error handling**: We follow the standard TCL error message format for wrong argument count. The message matches what users would expect from TCL.

4. **Memory management**: The implementation creates a copy of the list (`listCopy`) for getting the length. This is handled through the host operations API, with memory management being the responsibility of the embedder as per feather's design.
