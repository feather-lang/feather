# linsert Builtin Comparison

## Summary of Our Implementation

Our implementation of `linsert` is located in `src/builtin_linsert.c`. It:

1. Validates that at least 2 arguments are provided (list and index)
2. Parses the list from the first argument
3. Parses the index using `feather_parse_index()`, which supports:
   - Simple integer indices
   - `end` keyword
   - `end-N` syntax for end-relative indexing
4. Handles end-relative indices by adding 1 to position elements correctly
5. Clamps the index to valid bounds (0 to list length)
6. Uses `ops->list.splice()` to insert elements at the computed position
7. Returns the new list with elements inserted

## TCL Features We Support

- **Basic insertion**: Insert zero or more elements before a given index
- **Simple integer indices**: `linsert $list 0 a b c` inserts at the beginning
- **`end` keyword**: `linsert $list end a` appends elements
- **`end-N` syntax**: `linsert $list end-1 a` inserts before the last element
- **Negative index clamping**: Indices less than 0 are treated as 0 (insert at beginning)
- **Over-length index clamping**: Indices greater than list length append to the end
- **Multiple element insertion**: All element arguments are inserted in order
- **List parsing**: The first argument is parsed as a list if it is a string

## TCL Features We Do NOT Support

Based on the TCL manual, our implementation appears to be **fully compatible** with the documented behavior. The manual specifies:

1. Inserting elements before the index'th element - **Supported**
2. Negative/zero indices insert at beginning - **Supported**
3. Indices >= length treated as `end` - **Supported**
4. Simple index arithmetic - **Supported** (via `feather_parse_index`)
5. End-relative indexing - **Supported**
6. Start-relative indices: first element ends up at that index - **Supported**
7. End-relative indices: last element ends up at that index - **Supported**

There are no documented TCL features that we fail to support.

## Notes on Implementation Differences

### Index Handling for End-Relative Indices

Our implementation has special handling for end-relative indices (lines 26-40):

```c
int is_end_relative = (idxLen >= 3 &&
                       ops->string.byte_at(interp, indexObj, 0) == 'e' &&
                       ops->string.byte_at(interp, indexObj, 1) == 'n' &&
                       ops->string.byte_at(interp, indexObj, 2) == 'd');

// ... after parsing ...

if (is_end_relative) {
    index += 1;
}
```

This adjustment ensures that for end-relative indices, the last inserted element ends up at the specified position. For example, with `linsert {a b c} end-1 X`:
- The list has 3 elements (indices 0, 1, 2)
- `end-1` resolves to index 1 (second-to-last position)
- The adjustment makes it insert at position 2
- Result: `{a b X c}` - X is now at position `end-1` (index 1)

This matches TCL's documented behavior: "when index is end-relative, the last element will be at that index in the resulting list."

### Error Messages

Our error message format follows the TCL convention:
```
wrong # args: should be "linsert list index ?element ...?"
```

This matches the expected TCL error format.
