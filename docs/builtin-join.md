# join Builtin Command

## Summary of Our Implementation

The `join` command in feather creates a string by concatenating list elements with a separator string between them.

**Syntax:** `join list ?joinString?`

Our implementation (`src/builtin_join.c`):

1. Accepts 1 or 2 arguments (list, optional separator)
2. Defaults to a single space as the separator when not specified
3. Returns an empty string for an empty list
4. Returns the single element (unchanged) for a one-element list
5. Concatenates all elements with the separator for lists with 2+ elements

## TCL Features We Support

| Feature | Status | Notes |
|---------|--------|-------|
| Basic list joining | Supported | Concatenates all list elements into a single string |
| Custom separator string | Supported | Second optional argument specifies the join string |
| Default space separator | Supported | Uses single space `" "` when no separator provided |
| Empty list handling | Supported | Returns empty string `""` |
| Single element list | Supported | Returns the element as-is |
| Multi-character separators | Supported | e.g., `join $list ", "` works correctly |

## TCL Features We Do NOT Support

Our implementation appears to be **fully compatible** with TCL's `join` command. The TCL manual defines a simple interface:

- Takes a list and an optional joinString
- Defaults joinString to space
- Joins elements with the separator

We implement all of these features.

## Notes on Implementation Differences

### List Parsing

Our implementation calls `ops->list.from(interp, listObj)` to convert the first argument to a list representation. This is consistent with TCL's requirement that "the list argument must be a valid Tcl list."

### Error Messages

Our error message format matches TCL convention:
```
wrong # args: should be "join list ?joinString?"
```

### String Representation

TCL's manual notes an interesting use case: using `join` to "flatten a list by a single level." For example:
```tcl
set data {1 {2 3} 4 {5 {6 7} 8}}
join $data
# Returns: 1 2 3 4 5 {6 7} 8
```

This works because nested list elements retain their bracing when converted to string representation. Our implementation should handle this correctly since we use `ops->list.at()` to retrieve each element and `ops->string.concat()` to build the result, which will use the string representation of each element.

### Memory Management

Our implementation builds the result incrementally using string concatenation. For very large lists, this may have different performance characteristics compared to TCL's implementation, but the functional behavior is equivalent.
