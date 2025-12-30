# lreverse Builtin Comparison

## Summary of Our Implementation

Our implementation of `lreverse` in `src/builtin_lreverse.c` provides the core functionality of the TCL `lreverse` command. It takes a single list argument and returns a new list with the elements in reverse order.

The implementation:
1. Validates that exactly one argument is provided
2. Converts the argument to a list representation
3. Creates a new list by iterating from the end to the beginning of the input list
4. Pushes each element onto the result list
5. Returns the reversed list as the result

## TCL Features We Support

- **Basic list reversal**: Given a list, returns a new list with elements in reverse order
- **Argument validation**: Returns an error with the message `wrong # args: should be "lreverse list"` if the wrong number of arguments is provided
- **Nested lists**: Since we preserve list elements as-is, nested lists are handled correctly (e.g., `lreverse {a b {c d} e}` returns `{e {c d} b a}`)
- **Empty lists**: An empty list input returns an empty list

## TCL Features We Do NOT Support

The TCL `lreverse` command is quite simple and our implementation appears to be **feature-complete** with respect to the official TCL specification. The TCL manual describes only the basic functionality of reversing list element order, which we fully implement.

## Notes on Implementation Differences

1. **List parsing**: Our implementation uses `ops->list.from()` to convert the argument to a list, which relies on the interpreter's list parsing. This should be equivalent to TCL's list parsing behavior.

2. **String representation**: TCL's `lreverse` preserves the internal representation where possible. Our implementation creates a fresh list by pushing elements one at a time, which may result in a different internal representation but should produce an equivalent string representation.

3. **Error handling**: Our implementation returns `TCL_ERROR` if list conversion fails (when `ops->list.from()` returns 0), matching TCL's behavior of failing on malformed lists.

4. **Memory management**: Our implementation relies on the host's memory management through `FeatherHostOps`, whereas TCL manages memory internally. This is an architectural difference, not a behavioral one.
