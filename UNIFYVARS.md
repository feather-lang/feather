# Plan: Unify Variable and Namespace Operations

## Decision Summary

Based on user feedback:
- **Fire traces for qualified access**: Yes (align with standard TCL)
- **Remove `ns.{get,set,unset,var_exists}`**: Yes
- **Approach**: Change interface (add `ns` parameter to `var.*` operations)

## New Interface

### Updated `FeatherVarOps` Signatures

```c
// Add ns parameter (can be nil for frame-local)
FeatherObj (*get)(FeatherInterp interp, FeatherObj name, FeatherObj ns);
void (*set)(FeatherInterp interp, FeatherObj name, FeatherObj ns, FeatherObj value);
void (*unset)(FeatherInterp interp, FeatherObj name, FeatherObj ns);
FeatherResult (*exists)(FeatherInterp interp, FeatherObj name, FeatherObj ns);

// Unchanged
void (*link)(FeatherInterp interp, FeatherObj local, size_t target_level, FeatherObj target);
void (*link_ns)(FeatherInterp interp, FeatherObj local, FeatherObj ns, FeatherObj name);
FeatherObj (*names)(FeatherInterp interp, FeatherObj ns);
```

### Semantics

| `ns` parameter | Behavior |
|----------------|----------|
| `nil` | Frame-local: follow upvar/variable links, fire traces |
| namespace path | Direct namespace access, fire traces, NO link following |

## Files to Modify

### 1. `src/feather.h`
- Update `FeatherVarOps` struct signatures
- Update docstrings
- Remove from `FeatherNamespaceOps`: `get_var`, `set_var`, `var_exists`, `unset_var`
- Update `link_ns` docstring (remove references to removed ops)

### 2. `interp_callbacks.go`
- Modify `goVarGet`, `goVarSet`, `goVarUnset`, `goVarExists`:
  - Add `ns C.FeatherObj` parameter
  - When `ns != 0`: access `i.namespaces[nsPath].vars[name]` directly + fire traces
  - When `ns == 0`: current behavior (link following + frame lookup + traces)
- Remove: `goNsGetVar`, `goNsSetVar`, `goNsVarExists`, `goNsUnsetVar`

### 3. `callbacks.c`
- Update function pointer setup for var ops
- Remove function pointer setup for ns var ops

### 4. C Builtins (remove branch pattern)

**`src/builtin_set.c`** (lines 25-31, 56-62):
```c
// Before:
if (ops->list.is_nil(interp, ns)) {
    value = ops->var.get(interp, localName);
} else {
    value = ops->ns.get_var(interp, ns, localName);
}

// After:
value = ops->var.get(interp, localName, ns);
```

**`src/builtin_append.c`** (lines 24-30, 47-53):
- Same pattern

**`src/builtin_lappend.c`** (lines 24-30, 47-53):
- Same pattern

**`src/builtin_incr.c`** (lines 24-30, 74-80):
- Same pattern

**`src/builtin_info.c`** (line 30):
```c
// Before:
exists = ops->ns.var_exists(interp, ns, localName);

// After:
exists = (ops->var.exists(interp, localName, ns) == TCL_OK);
```

**`src/builtin_variable.c`** (line 42):
```c
// Before:
ops->ns.set_var(interp, current_ns, name, value);

// After:
ops->var.set(interp, name, current_ns, value);
```

**`src/parse.c`** (lines 356, 400 - variable substitution):
```c
// Before:
value = ops->ns.get_var(interp, ns, localName);

// After:
value = ops->var.get(interp, localName, ns);
```

**`src/builtin_expr.c`** (line 371):
```c
// Before:
value = p->ops->ns.get_var(p->interp, ns, localName);

// After:
value = p->ops->var.get(p->interp, localName, ns);
```

## Implementation Steps

### Step 1: Update `src/feather.h`
1. Change `FeatherVarOps` function pointer signatures
2. Remove `get_var`, `set_var`, `var_exists`, `unset_var` from `FeatherNamespaceOps`
3. Update documentation

### Step 2: Update Go implementation
1. Modify `goVarGet(interp, name, ns)` in `interp_callbacks.go`:
   - If `ns != 0`: lookup in `i.namespaces[nsPath]`, fire traces
   - If `ns == 0`: current frame-local behavior
2. Same for `goVarSet`, `goVarUnset`, `goVarExists`
3. Delete `goNsGetVar`, `goNsSetVar`, `goNsVarExists`, `goNsUnsetVar`

### Step 3: Update `callbacks.c`
- Remove initialization of removed ns ops
- Update any function pointer assignments

### Step 4: Update C builtins
- `builtin_set.c` - replace branch with unified call
- `builtin_append.c` - replace branch with unified call
- `builtin_lappend.c` - replace branch with unified call
- `builtin_incr.c` - replace branch with unified call
- `builtin_info.c` - use `var.exists` with ns param
- `builtin_variable.c` - use `var.set` with ns param
- `parse.c` - use `var.get` with ns param
- `builtin_expr.c` - use `var.get` with ns param

### Step 5: Test
- `mise test` - verify all tests pass
- `mise test:js` - verify JS/WASM host still works

## Benefits

1. **Reduced duplication**: No more branch pattern in every builtin
2. **Fixed trace semantics**: Qualified access (`::x`) now fires traces
3. **Simpler API**: 4 fewer operations in `FeatherNamespaceOps`
4. **Cleaner mental model**: One way to access variables
