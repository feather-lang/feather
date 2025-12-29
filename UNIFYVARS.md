# Plan: Unify Variable and Namespace Operations

## Decision Summary

- **Fire traces for qualified access**: Yes (align with standard TCL)
- **Remove `ns.{get,set,unset,var_exists}`**: Yes
- **Approach**: Extend C wrappers to handle qualified names (like `namespace_util.c` does for commands)

## Pattern Established by Recent Commits

The last four commits established a clear pattern for centralizing logic in C:

| Commit | What was centralized | Host ops removed |
|--------|---------------------|------------------|
| 2f86926 | Trace firing → `src/trace.c` | `FeatherTraceOps` |
| 730514d | Rename validation → `src/builtin_rename.c` | displayName helpers |
| 80eb014 | Command storage → namespace only | legacy builtins/procs maps |
| 6470985 | Command ops → `src/namespace_util.c` | `FeatherProcOps` |

**The pattern:**
1. Create C functions that handle qualified names internally
2. C functions call low-level `ops->ns.*` operations after splitting names
3. Remove the higher-level host ops that duplicated this logic

## Current State

**`src/namespace_util.c`** - handles qualified **command** names:
- `feather_register_command(qualifiedName, ...)` → splits, calls `ops->ns.set_command`
- `feather_lookup_command(name, ...)` → splits, calls `ops->ns.get_command`
- `feather_delete_command(name)` → splits, calls `ops->ns.delete_command`
- `feather_rename_command(old, new)` → splits, manages namespaces

**`src/var.c`** - handles **unqualified** variable names only:
- `feather_get_var(name)` → calls `ops->var.get` + fires traces
- `feather_set_var(name, value)` → calls `ops->var.set` + fires traces
- `feather_unset_var(name)` → fires traces + calls `ops->var.unset`

**Gap:** The `var.c` wrappers don't handle qualified names like `::varname`. Builtins must still branch between `ops->var.*` and `ops->ns.get_var/set_var`.

## Implementation Plan

Extend `var.c` wrappers to handle qualified variable names, mirroring `namespace_util.c`:

### Step 1: Extend `src/var.c`

```c
FeatherObj feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj name) {
  ops = feather_get_ops(ops);

  // Resolve qualified name
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, name, &ns, &localName);

  FeatherObj value;
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    value = ops->var.get(interp, localName);
  } else {
    // Qualified - namespace storage
    value = ops->ns.get_var(interp, ns, localName);
  }

  // Fire traces on original name
  feather_fire_var_traces(ops, interp, name, "read");
  return value;
}
```

Same pattern for `feather_set_var` and `feather_unset_var`.

Add `feather_var_exists` wrapper:
```c
int feather_var_exists(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name);
```

### Step 2: Update `src/internal.h`

Add declaration for `feather_var_exists`.

### Step 3: Update all builtins to use wrappers

Remove the branch pattern from:
- `src/builtin_set.c` - use `feather_get_var(name)`, `feather_set_var(name, value)`
- `src/builtin_append.c` - switch to wrappers (currently no traces!)
- `src/builtin_lappend.c` - switch to wrappers (currently no traces!)
- `src/builtin_incr.c` - switch to wrappers (currently no traces!)
- `src/builtin_info.c` - use `feather_var_exists(name)`
- `src/builtin_variable.c` - use `feather_set_var`
- `src/parse.c` - already uses wrapper, update signature
- `src/builtin_expr.c` - already uses wrapper, update signature

### Step 4: Remove `ns.*_var` from host interface

**`src/feather.h`:**
- Remove from `FeatherNamespaceOps`: `get_var`, `set_var`, `var_exists`, `unset_var`

**`interp_callbacks.go`:**
- Remove: `goNsGetVar`, `goNsSetVar`, `goNsVarExists`, `goNsUnsetVar`

**`callbacks.c`:**
- Remove function pointer assignments for removed ops

**`js/feather.js`:**
- Remove: `feather_host_ns_get_var`, `feather_host_ns_set_var`, etc.

### Step 5: Test

- `mise test` - verify all tests pass
- `mise test:js` - verify JS host still works

## Files to Modify

| File | Changes |
|------|---------|
| `src/var.c` | Extend wrappers to handle qualified names |
| `src/internal.h` | Add `feather_var_exists` declaration |
| `src/feather.h` | Remove 4 ops from `FeatherNamespaceOps` |
| `src/builtin_set.c` | Remove branch, use wrapper |
| `src/builtin_append.c` | Switch to wrappers |
| `src/builtin_lappend.c` | Switch to wrappers |
| `src/builtin_incr.c` | Switch to wrappers |
| `src/builtin_info.c` | Use `feather_var_exists` |
| `src/builtin_variable.c` | Use wrapper |
| `src/parse.c` | Update to new signature |
| `src/builtin_expr.c` | Update to new signature |
| `interp_callbacks.go` | Remove 4 goNs*Var functions |
| `callbacks.c` | Remove 4 function pointer assignments |
| `js/feather.js` | Remove 4 feather_host_ns_*_var functions |

## Benefits

1. **Consistent with established pattern**: Follows `namespace_util.c` approach
2. **Fixed trace semantics**: Qualified access (`::x`) fires traces
3. **Fixed missing traces**: `append`, `lappend`, `incr` will fire traces
4. **Simpler host interface**: 4 fewer operations in `FeatherNamespaceOps`
5. **Single implementation**: All variable access logic in C
