# Plan: Unify Variable and Namespace Operations

## Decision Summary

Based on user feedback:
- **Fire traces for qualified access**: Yes (align with standard TCL)
- **Remove `ns.{get,set,unset,var_exists}`**: Yes
- **Approach**: Change interface (add `ns` parameter to `var.*` operations)

## Current State (after commit 2f86926)

The trace refactoring moved all trace logic to C:
- `feather_fire_var_traces()` in `src/trace.c` handles trace firing
- C wrappers exist in `src/var.c`:
  - `feather_get_var(ops, interp, name)` - calls `ops->var.get` + fires traces
  - `feather_set_var(ops, interp, name, value)` - calls `ops->var.set` + fires traces
  - `feather_unset_var(ops, interp, name)` - fires traces + calls `ops->var.unset`
- Go host no longer handles traces (removed `fireVarTraces`, `TraceEntry`, etc.)

**Current inconsistencies:**
- `builtin_set.c`, `builtin_unset.c`, `parse.c`, `builtin_expr.c` - use wrappers, fire traces
- `builtin_append.c`, `builtin_incr.c`, `builtin_lappend.c` - use raw `ops->var.*`/`ops->ns.*`, NO traces

## Revised Approach

Since trace firing is now in C, extend the existing C wrappers instead of changing `FeatherVarOps`:

### Updated C Wrapper Signatures (`src/var.c`)

```c
// Add ns parameter (can be nil for frame-local)
FeatherObj feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj name, FeatherObj ns);
void feather_set_var(const FeatherHostOps *ops, FeatherInterp interp,
                     FeatherObj name, FeatherObj ns, FeatherObj value);
void feather_unset_var(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name, FeatherObj ns);
```

### Semantics

| `ns` parameter | Behavior |
|----------------|----------|
| `0` (nil) | Frame-local: `ops->var.*` + fire traces on `name` |
| namespace path | Namespace: `ops->ns.*_var` + fire traces on qualified name |

## Files to Modify

### 1. `src/var.c` - Extend wrappers
```c
FeatherObj feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj name, FeatherObj ns) {
  ops = feather_get_ops(ops);
  FeatherObj value;
  FeatherObj traceName;
  if (ops->list.is_nil(interp, ns)) {
    value = ops->var.get(interp, name);
    traceName = name;
  } else {
    value = ops->ns.get_var(interp, ns, name);
    // Build qualified name for trace: ns + "::" + name (or just use original varName)
    traceName = /* reconstruct qualified name */;
  }
  feather_fire_var_traces(ops, interp, traceName, "read");
  return value;
}
```

### 2. `src/internal.h` - Update declarations

### 3. `src/feather.h`
- Remove from `FeatherNamespaceOps`: `get_var`, `set_var`, `var_exists`, `unset_var`

### 4. `interp_callbacks.go`
- Remove: `goNsGetVar`, `goNsSetVar`, `goNsVarExists`, `goNsUnsetVar`

### 5. `callbacks.c`
- Remove initialization of removed ns var ops

### 6. C Builtins - Use wrappers with ns parameter

**`src/builtin_set.c`**:
```c
// Before (lines 25-33):
if (ops->list.is_nil(interp, ns)) {
    value = feather_get_var(ops, interp, localName);
} else {
    value = ops->ns.get_var(interp, ns, localName);
    feather_fire_var_traces(ops, interp, varName, "read");
}

// After:
value = feather_get_var(ops, interp, localName, ns);
```

**`src/builtin_append.c`** - Add trace firing (currently missing!):
```c
// Before (no traces):
current = ops->var.get(interp, localName);
// or
current = ops->ns.get_var(interp, ns, localName);

// After:
current = feather_get_var(ops, interp, localName, ns);
```

**`src/builtin_lappend.c`** - Same as append

**`src/builtin_incr.c`** - Same pattern

**`src/builtin_info.c`** - Use wrapper for exists check

**`src/builtin_variable.c`** - Use wrapper

**`src/parse.c`** - Already uses wrapper, just add ns param

**`src/builtin_expr.c`** - Already uses wrapper, just add ns param

## Implementation Steps

### Step 1: Extend C wrappers in `src/var.c`
1. Add `ns` parameter to `feather_get_var`, `feather_set_var`, `feather_unset_var`
2. When `ns != 0`: use `ops->ns.get_var` etc. + fire traces
3. When `ns == 0`: current behavior

### Step 2: Update `src/internal.h` declarations

### Step 3: Update all callers to pass ns parameter
- `builtin_set.c` - pass ns, remove manual branch
- `builtin_unset.c` - pass ns (if applicable)
- `parse.c` - pass ns
- `builtin_expr.c` - pass ns

### Step 4: Fix missing trace firing
- `builtin_append.c` - switch to wrappers
- `builtin_lappend.c` - switch to wrappers
- `builtin_incr.c` - switch to wrappers

### Step 5: Remove ns var ops from interface
- `src/feather.h` - remove `get_var`, `set_var`, `var_exists`, `unset_var` from `FeatherNamespaceOps`
- `interp_callbacks.go` - remove `goNsGetVar`, `goNsSetVar`, `goNsVarExists`, `goNsUnsetVar`
- `callbacks.c` - remove initialization

### Step 6: Test
- `mise test` - verify all 1497+ tests pass
- `mise test:js` - verify JS/WASM host still works

## Benefits

1. **Reduced duplication**: No more branch pattern in every builtin
2. **Fixed trace semantics**: Qualified access (`::x`) now fires traces consistently
3. **Fixed missing traces**: `append`, `lappend`, `incr` will fire traces
4. **Simpler API**: 4 fewer operations in `FeatherNamespaceOps`
5. **Cleaner mental model**: One way to access variables with traces
