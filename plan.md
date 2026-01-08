# Completion API Plan for libfeather

## Context

The `usage complete` command returns completion candidates as a list of dicts:
```tcl
{{text puts type command help {}} {text proc type command help {}} ...}
```

C embedders (like game consoles, raylib apps) need to:
1. Get completions at a cursor position
2. Iterate over candidates efficiently
3. Access candidate fields (text, type, help, name) without parsing TCL dicts

## Design Principles

1. **Handle-based** - Consistent with existing libfeather API
2. **Zero allocations for caller** - C code shouldn't need to malloc for iteration
3. **Lazy field access** - Only extract fields the caller needs
4. **Simple iteration** - Count + index pattern, not callbacks

## Proposed API

### Option A: Direct Field Access (Recommended)

```c
// Get completions for script at position
// Returns a list handle containing completion dicts
FeatherObj FeatherComplete(FeatherInterp interp,
                           const char *script, size_t script_len,
                           size_t pos);

// Get number of completion candidates
size_t FeatherCompleteCount(FeatherInterp interp, FeatherObj completions);

// Get a specific completion candidate (returns dict handle)
FeatherObj FeatherCompleteAt(FeatherInterp interp, FeatherObj completions, size_t index);

// Extract fields from a completion dict
// Returns 0 if field doesn't exist, copies to buffer
size_t FeatherCompleteText(FeatherInterp interp, FeatherObj completion,
                           char *buf, size_t buflen);
size_t FeatherCompleteType(FeatherInterp interp, FeatherObj completion,
                           char *buf, size_t buflen);
size_t FeatherCompleteHelp(FeatherInterp interp, FeatherObj completion,
                           char *buf, size_t buflen);
size_t FeatherCompleteName(FeatherInterp interp, FeatherObj completion,
                           char *buf, size_t buflen);
```

**Usage Example:**
```c
FeatherObj completions = FeatherComplete(interp, script, strlen(script), cursor_pos);
size_t count = FeatherCompleteCount(interp, completions);

for (size_t i = 0; i < count; i++) {
    FeatherObj c = FeatherCompleteAt(interp, completions, i);

    char text[256], type[32];
    FeatherCompleteText(interp, c, text, sizeof(text));
    FeatherCompleteType(interp, c, type, sizeof(type));

    if (strcmp(type, "command") == 0) {
        draw_completion(text, COLOR_BLUE);
    } else if (strcmp(type, "flag") == 0) {
        draw_completion(text, COLOR_GREEN);
    }
    // ...
}
```

### Option B: Minimal API (Alternative)

Just expose `FeatherComplete` and let C code use existing dict operations:

```c
FeatherObj FeatherComplete(FeatherInterp interp,
                           const char *script, size_t script_len,
                           size_t pos);
```

Then use existing:
- `FeatherListLen()` / `FeatherListAt()` for iteration
- `FeatherDictGet()` for field access

**Usage:**
```c
FeatherObj completions = FeatherComplete(interp, script, strlen(script), pos);
size_t count = FeatherListLen(interp, completions);

for (size_t i = 0; i < count; i++) {
    FeatherObj c = FeatherListAt(interp, completions, i);

    FeatherObj textKey = FeatherString(interp, "text", 4);
    FeatherObj textVal = FeatherDictGet(interp, c, textKey);

    char text[256];
    FeatherCopy(interp, textVal, text, sizeof(text));
    // ...
}
```

### Comparison

| Aspect | Option A | Option B |
|--------|----------|----------|
| API surface | 6 new functions | 1 new function |
| C code simplicity | Very simple | More verbose |
| Handle allocations | Same (arena managed) | More (creates key strings) |
| Type safety | Named accessors | Generic dict access |
| Future extensibility | Add more accessors | Already complete |

## Recommendation

**Start with Option B** (minimal API) because:
1. Only adds one function
2. Leverages existing list/dict APIs
3. Can always add convenience functions later
4. Matches the "feather is minimal" philosophy

Then if C embedders find the dict access cumbersome, add Option A's convenience functions.

## Implementation Steps

1. Add `FeatherComplete` function:
   - Takes script, length, position
   - Calls `interp.Call("usage", "complete", script, pos)`
   - Returns list handle

2. Update header comments in exports.go to document completion workflow

3. Add example to raylib game showing console completion UI

## Alternative: Struct-based Return

For maximum efficiency, could return a struct:

```c
typedef struct {
    const char *text;
    size_t text_len;
    const char *type;
    size_t type_len;
    const char *help;
    size_t help_len;
    const char *name;  // for arg-placeholder only
    size_t name_len;
} FeatherCompletion;

// Returns count, fills array (caller provides buffer)
size_t FeatherComplete(FeatherInterp interp,
                       const char *script, size_t script_len, size_t pos,
                       FeatherCompletion *out, size_t max_completions);
```

This is more C-idiomatic but requires:
- Caller to pre-allocate array
- Careful lifetime management (strings point into Go memory)
- More complex implementation

**Not recommended** for initial implementation due to complexity.
