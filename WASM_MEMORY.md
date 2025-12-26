# Plan: Arena-Based Memory Management for WASM

## Goal

Eliminate memory leaks in the WASM build by implementing arena-based memory management with a clear separation between scratch (temporary) and persistent storage.

**Current state:**
- The WASM bump allocator (`js/wasm_alloc.c`) never frees memory — `heap_ptr` only grows
- The JS host's `FeatherInterp.objects` Map accumulates handles forever via `store()`
- Every eval leaks: parsing creates strings, list operations create handles, all persist indefinitely
- Long-running sessions eventually hit OOM

**Root causes:**
1. No mechanism to reclaim WASM heap memory
2. No mechanism to reclaim JS object handles
3. Persistent structures (procs, namespaces, traces) store raw handles that become invalid if we try to reset

**Desired end state:**
- Two arenas: **scratch** (reset after each top-level eval) and **persistent** (lives forever)
- WASM heap is scratch-only — reset via `feather_arena_reset()` after each eval
- JS object handles are scratch-only — `scratch.objects` Map cleared after each eval
- Persistent storage (namespace vars, proc bodies, traces) stores **materialized JS values**, not handles
- Retrieval from persistent storage wraps values in fresh scratch handles
- C code in `src/` manages the WASM arena; JS code manages the handle arena

**Memory model:**

```
┌─────────────────────────────────────────────────────────────────┐
│                        WASM Linear Memory                        │
├─────────────────────────────────────────────────────────────────┤
│  Static Data  │  Scratch Arena (reset after eval)               │
│               │  ← heap_base                    ← heap_ptr      │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     JS FeatherInterp                             │
├──────────────────────────┬──────────────────────────────────────┤
│   Persistent Storage     │   Scratch Arena                      │
│   (actual JS values)     │   (handles, reset after eval)        │
├──────────────────────────┼──────────────────────────────────────┤
│ namespaces.vars: Map     │ scratch.objects: Map<handle, obj>    │
│   "x" → {type,value}     │   42 → {type: 'string', value: ''}   │
│ procs: Map               │ scratch.nextHandle: number           │
│   "foo" → {params,body}  │                                      │
│ traces: Map              │                                      │
│ foreignInstances: Map    │                                      │
└──────────────────────────┴──────────────────────────────────────┘
```

**Benefits:**
- Zero memory leaks — scratch arenas are fully reclaimed
- No garbage collection overhead — just reset pointers
- No reference counting complexity
- Predictable memory usage bounded by single-eval peak
- Clean separation of concerns: C handles WASM memory, JS handles object lifetime

**Files involved:**
- `src/arena.h` (new) — arena API declarations
- `src/arena.c` (new) — arena implementation for WASM builds
- `js/wasm_alloc.c` — replaced by arena.c, deleted
- `js/feather.js` — scratch arena for handles, materialize/wrap pattern
- `js/mise.toml` — updated build to include arena.c, export reset function

---

## Key Edge Cases

### Nested Evals

Traces can trigger nested evals. For example, from `fireVarTraces()` (lines 270-286 in feather.js):

```javascript
// Current implementation - traces call feather_script_eval recursively
const fireVarTraces = (interp, varName, op) => {
  const traces = interp.traces.variable.get(varName);
  // ...
  wasmInstance.exports.feather_script_eval(0, interp.id, ptr, len, 0);
};
```

**Rule:** Arena reset happens ONLY at top-level eval completion. Nested evals do not reset.

**Implementation:** Track eval depth in FeatherInterp:
```javascript
this.evalDepth = 0;  // Increment on eval entry, decrement on exit
// Only reset when evalDepth returns to 0
```

### Handle Lifecycle

A handle is valid from creation until the next top-level eval completes. C code must not cache handles across evals.

**Stale handle access:** Returns `undefined` from `get()`. Most operations treat `undefined` as empty string or error.

**If a host function stores a handle in persistent storage without materializing:**
- The handle becomes invalid after reset
- Next access returns garbage or undefined
- This is a bug in the host function, not user error

### upvar/variable links

Variable links (via `feather_host_var_link`) store level + name, not handles:
```javascript
// Current implementation (line 489)
interp.currentFrame().vars.set(localName, { link: { level: targetLevel, name: targetName } });
```

**Safe:** Links use level numbers and string names, not handles. No change needed.

### Frame Storage

Frames store `cmd` and `args` as handles:
```javascript
// Current implementation (line 371)
interp.frames.push({ vars: new Map(), cmd, args, ns: parentNs });
```

**Problem:** If we reset mid-eval, frame info becomes invalid.

**Solution:** Since frames are always popped before eval completes, and we only reset at top-level completion, this is safe. The only concern is if `info level` is called — it should work within the same eval.

---

## Dependency Graph

```
M1 ──────────────────────────────────────┐
     (arena.h)                           │
         │                               │
         ▼                               │
M2 ──────┼───────────────────────────────┤
     (arena.c, delete wasm_alloc.c)      │
         │                               │
         │      ┌────────────────────────┘
         │      │
         ▼      ▼
M3 ─────────────────────────────────────
     (JS scratch arena, materialize/wrap)
         │
         ├──────┬──────┬──────┬──────┐
         │      │      │      │      │
         ▼      ▼      ▼      ▼      ▼
       M4     M5     M6     M7     M8     ← Parallelizable
      (var) (proc) (ns)  (frame)(return)
         │      │      │      │      │
         └──────┴──────┴──────┴──────┘
                       │
                       ▼
                     M9
                 (integrate reset)
                       │
                       ▼
                    M10
                (diagnostics/stress test)
                       │
                       ▼
                    M11
                (documentation)
                       │
                       ▼
                    M12
                 (verify all)
```

**Parallelizable milestones:** M4, M5, M6, M7, M8 can be done in any order after M3.

---

## M1: Define Arena API in C

Create `src/arena.h` with the arena management API.

**Tasks:**

1. Create `src/arena.h` with header guards
2. Define the arena API (see code below)
3. Document that `feather_arena_reset()` invalidates all pointers from previous allocations

**Target code:**
```c
#ifndef FEATHER_ARENA_H
#define FEATHER_ARENA_H

#include <stddef.h>

/**
 * Arena-based memory allocation for feather.
 *
 * In WASM builds, all allocations come from a single bump arena.
 * The arena is reset after each top-level eval, reclaiming all memory.
 *
 * WARNING: feather_arena_reset() invalidates ALL pointers from previous
 * allocations. Only call at top-level eval boundaries.
 *
 * Native builds may provide their own allocator via FeatherHostOps,
 * or use the default arena if available.
 */

/* Allocate `size` bytes from the current arena. Returns aligned pointer. */
void *feather_arena_alloc(size_t size);

/* Reset the arena, reclaiming all allocated memory. */
void feather_arena_reset(void);

/* Get current arena usage in bytes (for diagnostics). */
size_t feather_arena_used(void);

/* Get total arena capacity in bytes. */
size_t feather_arena_capacity(void);

#endif /* FEATHER_ARENA_H */
```

**Verification:** `zig cc -c -target wasm32-freestanding src/arena.h` compiles without errors.

**Test:** N/A (header only)

---

## M2: Implement Arena for WASM

Create `src/arena.c` implementing the arena for WASM builds.

**Current code to replace** (js/wasm_alloc.c):
```c
extern unsigned char __heap_base;
static unsigned char *heap_ptr = &__heap_base;

void *alloc(unsigned int size) {
    unsigned char *ptr = heap_ptr;
    heap_ptr += size;
    heap_ptr = (unsigned char *)(((unsigned long)heap_ptr + 7) & ~7);
    return ptr;
}

void free(void *ptr) {
    (void)ptr;  // Bump allocator doesn't free
}
```

**Target code** (src/arena.c):
```c
#include "arena.h"

#ifdef FEATHER_WASM_BUILD

extern unsigned char __heap_base;

static unsigned char *arena_base = &__heap_base;
static unsigned char *arena_ptr = &__heap_base;

void *feather_arena_alloc(size_t size) {
    unsigned char *ptr = arena_ptr;
    arena_ptr += size;
    /* Align to 8 bytes */
    arena_ptr = (unsigned char *)(((size_t)arena_ptr + 7) & ~7);
    return ptr;
}

void feather_arena_reset(void) {
    arena_ptr = arena_base;
}

size_t feather_arena_used(void) {
    return (size_t)(arena_ptr - arena_base);
}

size_t feather_arena_capacity(void) {
    return feather_arena_used();
}

/* Compatibility shims for existing code */
void *alloc(size_t size) {
    return feather_arena_alloc(size);
}

void free(void *ptr) {
    (void)ptr;
}

#endif /* FEATHER_WASM_BUILD */
```

**Build command update** (js/mise.toml):
```diff
 [tasks.build]
 run = """
 zig wasm-ld --no-entry \
   --allow-undefined \
   --export=feather_interp_init \
   ...
   --export=alloc \
   --export=free \
+  --export=feather_arena_reset \
+  --export=feather_arena_used \
   --export=wasm_call_compare \
   --import-memory \
   -o feather.wasm \
-  $(for f in ../src/*.c wasm_alloc.c; do
+  $(for f in ../src/*.c; do
     zig cc -target wasm32-freestanding -Os -c -DFEATHER_WASM_BUILD -I ../src -o /tmp/$(basename $f .c).o $f
     echo /tmp/$(basename $f .c).o
   done)
 """
```

**Verification:** 
- `cd js && mise build` succeeds
- `wasm-objdump -x feather.wasm | grep feather_arena` shows exports

**Test:** Existing tests still pass (`mise test:js`)

---

## M3: Add Scratch Arena to JS Host

Modify `FeatherInterp` to use a scratch arena for handles.

**Current code** (js/feather.js lines 26-53):
```javascript
class FeatherInterp {
  constructor(id) {
    this.id = id;
    this.objects = new Map();
    this.nextHandle = 1;
    this.result = 0;
    // ... rest
  }

  store(obj) {
    const handle = this.nextHandle++;
    this.objects.set(handle, obj);
    return handle;
  }

  get(handle) {
    return this.objects.get(handle);
  }
}
```

**Target code:**
```javascript
class FeatherInterp {
  constructor(id) {
    this.id = id;
    
    // Scratch arena - reset after each top-level eval
    this.scratch = {
      objects: new Map(),
      nextHandle: 1,
    };
    this.evalDepth = 0;  // Track nested eval depth
    
    this.result = 0;
    // ... rest unchanged
  }

  store(obj) {
    const handle = this.scratch.nextHandle++;
    this.scratch.objects.set(handle, obj);
    return handle;
  }

  get(handle) {
    return this.scratch.objects.get(handle);
  }

  resetScratch() {
    this.scratch = { objects: new Map(), nextHandle: 1 };
  }

  /**
   * Materialize a handle into a persistent value (deep copy).
   * Use when storing in procs, namespaces, traces, etc.
   */
  materialize(handle) {
    if (handle === 0) return null;
    const obj = this.get(handle);
    if (!obj) return null;
    
    if (obj.type === 'string') return { type: 'string', value: obj.value };
    if (obj.type === 'int') return { type: 'int', value: obj.value };
    if (obj.type === 'double') return { type: 'double', value: obj.value };
    if (obj.type === 'list') {
      return { type: 'list', items: obj.items.map(h => this.materialize(h)) };
    }
    if (obj.type === 'dict') {
      return { 
        type: 'dict', 
        entries: obj.entries.map(([k, v]) => [this.materialize(k), this.materialize(v)])
      };
    }
    if (obj.type === 'foreign') {
      // Foreign objects can't be fully materialized; store reference info
      return { type: 'foreign', typeName: obj.typeName, stringRep: obj.stringRep };
    }
    // Fallback
    return { type: 'string', value: this.getString(handle) };
  }

  /**
   * Wrap a materialized value into a fresh scratch handle.
   * Use when retrieving from persistent storage.
   */
  wrap(value) {
    if (value === null || value === undefined) return 0;
    
    if (value.type === 'list') {
      const items = value.items.map(item => this.wrap(item));
      return this.store({ type: 'list', items });
    }
    if (value.type === 'dict') {
      const entries = value.entries.map(([k, v]) => [this.wrap(k), this.wrap(v)]);
      return this.store({ type: 'dict', entries });
    }
    // Primitives: string, int, double, foreign
    return this.store({ ...value });
  }
}
```

**Verification:**
- `mise test:js` passes (no functional change yet — materialize/wrap not called)

**Test:** Add to js/tester.js temporarily:
```javascript
// After creating interp:
const h1 = interp.store({ type: 'string', value: 'test' });
const m = interp.materialize(h1);
interp.resetScratch();
const h2 = interp.wrap(m);
console.assert(interp.getString(h2) === 'test', 'materialize/wrap roundtrip');
```

---

## M4: Update Variable Storage

Modify variable operations to materialize on set, wrap on get.

**Current code** (feather_host_var_set, line 440):
```javascript
feather_host_var_set: (interpId, name, value) => {
  const interp = interpreters.get(interpId);
  const varName = interp.getString(name);
  const frame = interp.currentFrame();
  const entry = frame.vars.get(varName);
  if (entry?.link) {
    // ... handle link
    targetEntry.value = value;  // ← stores raw handle
  } else if (entry?.nsLink) {
    // ... handle nsLink
    ns.vars.set(entry.nsLink.name, { value });  // ← stores raw handle
  } else {
    frame.vars.set(varName, { value });  // ← stores raw handle
  }
  fireVarTraces(interp, varName, 'write');
},
```

**Target code:**
```javascript
feather_host_var_set: (interpId, name, value) => {
  const interp = interpreters.get(interpId);
  const varName = interp.getString(name);
  const frame = interp.currentFrame();
  const entry = frame.vars.get(varName);
  const materialized = interp.materialize(value);  // ← materialize here
  
  if (entry?.link) {
    const targetFrame = interp.frames[entry.link.level];
    if (targetFrame) {
      let targetEntry = targetFrame.vars.get(entry.link.name);
      if (!targetEntry) targetEntry = {};
      targetEntry.value = materialized;
      targetFrame.vars.set(entry.link.name, targetEntry);
    }
  } else if (entry?.nsLink) {
    const ns = interp.getNamespace(entry.nsLink.ns);
    if (ns) ns.vars.set(entry.nsLink.name, { value: materialized });
  } else {
    frame.vars.set(varName, { value: materialized });
  }
  fireVarTraces(interp, varName, 'write');
},
```

**Current code** (feather_host_var_get, line 416):
```javascript
feather_host_var_get: (interpId, name) => {
  // ...
  result = entry.value || 0;  // ← returns raw handle
  fireVarTraces(interp, varName, 'read');
  return result;
},
```

**Target code:**
```javascript
feather_host_var_get: (interpId, name) => {
  const interp = interpreters.get(interpId);
  const varName = interp.getString(name);
  const frame = interp.currentFrame();
  const entry = frame.vars.get(varName);
  if (!entry) return 0;
  
  let materialized;
  if (entry.link) {
    const targetFrame = interp.frames[entry.link.level];
    const targetEntry = targetFrame?.vars.get(entry.link.name);
    if (!targetEntry) return 0;
    materialized = typeof targetEntry === 'object' && 'value' in targetEntry 
      ? targetEntry.value : targetEntry;
  } else if (entry.nsLink) {
    const ns = interp.getNamespace(entry.nsLink.ns);
    const nsEntry = ns?.vars.get(entry.nsLink.name);
    if (!nsEntry) return 0;
    materialized = typeof nsEntry === 'object' && 'value' in nsEntry 
      ? nsEntry.value : nsEntry;
  } else {
    materialized = entry.value;
  }
  
  if (!materialized) return 0;
  
  fireVarTraces(interp, varName, 'read');
  return interp.wrap(materialized);  // ← wrap here
},
```

**Verification:** `mise test:js` passes

**Test case:**
```tcl
set x hello
set y $x
# After internal reset, y should still be "hello"
```

---

## M5: Update Proc Storage

Modify proc operations to materialize on define, wrap on retrieve.

**Current code** (feather_host_proc_define, line 515):
```javascript
feather_host_proc_define: (interpId, name, params, body) => {
  const interp = interpreters.get(interpId);
  const procName = interp.getString(name);
  interp.procs.set(procName, { params, body });  // ← raw handles
  // ... namespace storage also uses raw handles
  namespace.commands.set(simpleName, { kind: TCL_CMD_PROC, fn: 0, params, body });
},
```

**Target code:**
```javascript
feather_host_proc_define: (interpId, name, params, body) => {
  const interp = interpreters.get(interpId);
  const procName = interp.getString(name);
  
  // Materialize for persistent storage
  const materializedParams = interp.materialize(params);
  const materializedBody = interp.materialize(body);
  
  interp.procs.set(procName, { 
    params: materializedParams, 
    body: materializedBody 
  });

  // Also store in namespace commands map
  let nsPath = '';
  let simpleName = procName;
  if (procName.startsWith('::')) {
    const withoutLeading = procName.slice(2);
    const lastSep = withoutLeading.lastIndexOf('::');
    if (lastSep !== -1) {
      nsPath = withoutLeading.slice(0, lastSep);
      simpleName = withoutLeading.slice(lastSep + 2);
    } else {
      simpleName = withoutLeading;
    }
  }
  const namespace = interp.ensureNamespace('::' + nsPath);
  namespace.commands.set(simpleName, { 
    kind: TCL_CMD_PROC, 
    fn: 0, 
    params: materializedParams, 
    body: materializedBody 
  });
},
```

**Current code** (feather_host_proc_params, line 541):
```javascript
feather_host_proc_params: (interpId, name, resultPtr) => {
  // ...
  writeI32(resultPtr, proc.params);  // ← raw handle
  return TCL_OK;
},
```

**Target code:**
```javascript
feather_host_proc_params: (interpId, name, resultPtr) => {
  const interp = interpreters.get(interpId);
  const procName = interp.getString(name);
  const proc = interp.procs.get(procName) || interp.procs.get(`::${procName}`);
  if (proc) {
    writeI32(resultPtr, interp.wrap(proc.params));  // ← wrap here
    return TCL_OK;
  }
  return TCL_ERROR;
},
```

**Similarly update** `feather_host_proc_body` (line 552).

**Verification:** `mise test:js` passes

**Test case:**
```tcl
proc greet {name} { return "Hello, $name" }
greet World
# Should work across eval boundaries
```

---

## M6: Update Namespace Command Storage

Modify namespace command operations to materialize on set, wrap on get.

**Current code** (feather_host_ns_set_command, line 809):
```javascript
feather_host_ns_set_command: (interpId, ns, name, kind, fn, params, body) => {
  const interp = interpreters.get(interpId);
  const nsPath = interp.getString(ns);
  const cmdName = interp.getString(name);
  const namespace = interp.ensureNamespace(nsPath);
  namespace.commands.set(cmdName, { kind, fn, params, body });  // ← raw handles
},
```

**Target code:**
```javascript
feather_host_ns_set_command: (interpId, ns, name, kind, fn, params, body) => {
  const interp = interpreters.get(interpId);
  const nsPath = interp.getString(ns);
  const cmdName = interp.getString(name);
  const namespace = interp.ensureNamespace(nsPath);
  namespace.commands.set(cmdName, { 
    kind, 
    fn, 
    params: interp.materialize(params),
    body: interp.materialize(body)
  });
},
```

**Current code** (feather_host_ns_get_command, around line 795):
```javascript
feather_host_ns_get_command: (interpId, ns, name, paramsPtr, bodyPtr, fnPtr) => {
  // ...
  writeI32(paramsPtr, cmd.params);
  writeI32(bodyPtr, cmd.body);
  // ...
},
```

**Target code:**
```javascript
feather_host_ns_get_command: (interpId, ns, name, paramsPtr, bodyPtr, fnPtr) => {
  const interp = interpreters.get(interpId);
  // ... lookup logic unchanged ...
  writeI32(paramsPtr, interp.wrap(cmd.params));
  writeI32(bodyPtr, interp.wrap(cmd.body));
  writeI32(fnPtr, cmd.fn || 0);
  return cmd.kind;
},
```

**Verification:** `mise test:js` passes

---

## M7: Update Frame Storage

Frames are only accessed within a single eval, so they're safe without materialize/wrap. However, `feather_host_frame_info` should be updated for consistency if frames are ever inspected across boundaries.

**Current code** (feather_host_frame_push, line 361):
```javascript
feather_host_frame_push: (interpId, cmd, args) => {
  // ...
  interp.frames.push({ vars: new Map(), cmd, args, ns: parentNs });  // ← raw handles
  // ...
},
```

**Decision:** Since frames are always popped before eval completes, and reset only happens at top-level completion, raw handles in frames are safe. **No change needed for M7.**

However, document this assumption:
```javascript
// NOTE: Frame cmd/args store raw handles. This is safe because:
// 1. Frames are always popped before eval returns
// 2. Arena reset only happens at top-level eval completion
// 3. If we ever support frame introspection across evals, revisit this
```

**Verification:** `mise test:js` passes (no changes made)

---

## M8: Update Trace Storage

Modify trace operations to materialize script on add, wrap on info.

**Current code** (feather_host_trace_add, line 1424):
```javascript
feather_host_trace_add: (interpId, kind, name, ops, script) => {
  // ...
  traces.get(nameStr).push({ ops: opsStr, script });  // ← raw handle
  return TCL_OK;
},
```

**Target code:**
```javascript
feather_host_trace_add: (interpId, kind, name, ops, script) => {
  const interp = interpreters.get(interpId);
  const kindStr = interp.getString(kind);
  const nameStr = interp.getString(name);
  const opsStr = interp.getString(ops);
  // Materialize script for persistent storage
  const scriptStr = interp.getString(script);
  const traces = interp.traces[kindStr];
  if (!traces) return TCL_ERROR;
  if (!traces.has(nameStr)) traces.set(nameStr, []);
  traces.get(nameStr).push({ ops: opsStr, script: scriptStr });  // ← store string
  return TCL_OK;
},
```

**Update fireVarTraces** (line 270) to use stored string directly:
```javascript
const fireVarTraces = (interp, varName, op) => {
  const traces = interp.traces.variable.get(varName);
  if (!traces || traces.length === 0) return;

  for (const trace of traces) {
    const ops = trace.ops.split(/\s+/);
    if (!ops.includes(op)) continue;

    // trace.script is now a string, not a handle
    const cmd = `${trace.script} ${varName} {} ${op}`;
    const [ptr, len] = writeString(cmd);
    wasmInstance.exports.feather_script_eval(0, interp.id, ptr, len, 0);
    wasmInstance.exports.free(ptr);
  }
};
```

**Update feather_host_trace_info** (line 1450):
```javascript
feather_host_trace_info: (interpId, kind, name) => {
  const interp = interpreters.get(interpId);
  const kindStr = interp.getString(kind);
  const nameStr = interp.getString(name);
  const traces = interp.traces[kindStr]?.get(nameStr) || [];
  const items = traces.map(t => {
    const ops = t.ops.split(/\s+/).filter(o => o);
    const subItems = ops.map(op => interp.store({ type: 'string', value: op }));
    // t.script is now a string, wrap it in a fresh handle
    subItems.push(interp.store({ type: 'string', value: t.script }));
    return interp.store({ type: 'list', items: subItems });
  });
  return interp.store({ type: 'list', items });
},
```

**Update feather_host_trace_remove** similarly.

**Verification:** `mise test:js` passes

**Test case:**
```tcl
proc tracecb {name1 name2 op} { puts "traced: $name1" }
trace add variable x write tracecb
set x 1
set x 2
# Trace should fire on each set
```

---

## M9: Integrate Arena Reset in Eval

Call arena reset after each top-level eval completes.

**Current code** (eval method, line 1619):
```javascript
eval(interpId, script) {
  const [ptr, len] = writeString(script);
  const result = wasmInstance.exports.feather_script_eval(0, interpId, ptr, len, 0);
  wasmInstance.exports.free(ptr);

  const interp = interpreters.get(interpId);
  if (result === TCL_OK) {
    return interp.getString(interp.result);
  }
  // ... error handling
},
```

**Target code:**
```javascript
eval(interpId, script) {
  const interp = interpreters.get(interpId);
  interp.evalDepth++;
  
  try {
    const [ptr, len] = writeString(script);
    const result = wasmInstance.exports.feather_script_eval(0, interpId, ptr, len, 0);
    // Note: don't free ptr yet - it's in arena, will be reset
    
    // Capture result BEFORE reset (getString returns plain JS string)
    const resultValue = interp.getString(interp.result);
    
    if (result === TCL_OK) {
      return resultValue;
    }
    // ... rest of error handling, using resultValue instead of interp.result
  } finally {
    interp.evalDepth--;
    
    // Reset arenas only at top-level completion
    if (interp.evalDepth === 0) {
      interp.resetScratch();
      wasmInstance.exports.feather_arena_reset();
    }
  }
},
```

**Also update parse()** if called standalone:
```javascript
parse(interpId, script) {
  const interp = interpreters.get(interpId);
  // ... existing parse logic ...
  
  // Reset after parse completes (parse is always top-level)
  interp.resetScratch();
  wasmInstance.exports.feather_arena_reset();
  
  return { status, result: resultStr, errorMessage };
},
```

**Verification:** 
- `mise test:js` passes
- Manual test: Run 100 evals in a loop, check memory doesn't grow

**Test:** Add stress test (see M10)

---

## M10: Add Diagnostics and Testing

Add memory diagnostics and stress tests.

**Add to exported API:**
```javascript
return {
  // ... existing methods ...
  
  memoryStats(interpId) {
    const interp = interpreters.get(interpId);
    return {
      scratchHandles: interp.scratch.objects.size,
      wasmArenaUsed: wasmInstance.exports.feather_arena_used(),
      namespaceCount: interp.namespaces.size,
      procCount: interp.procs.size,
      evalDepth: interp.evalDepth,
    };
  },
  
  forceReset(interpId) {
    const interp = interpreters.get(interpId);
    if (interp.evalDepth > 0) {
      throw new Error('Cannot reset during eval');
    }
    interp.resetScratch();
    wasmInstance.exports.feather_arena_reset();
  },
};
```

**Create stress test** (js/stress-test.js):
```javascript
import { createFeather } from './feather.js';

async function stressTest() {
  const feather = await createFeather('./feather.wasm');
  const interp = feather.create();
  
  const iterations = 10000;
  const startMem = feather.memoryStats(interp);
  console.log('Start:', startMem);
  
  for (let i = 0; i < iterations; i++) {
    feather.eval(interp, `
      set x [list a b c d e f g h i j]
      lappend x [string repeat "x" 100]
      proc tmp {} { return [expr {1 + 2}] }
      tmp
      rename tmp {}
    `);
    
    if (i % 1000 === 0) {
      console.log(`Iteration ${i}:`, feather.memoryStats(interp));
    }
  }
  
  const endMem = feather.memoryStats(interp);
  console.log('End:', endMem);
  
  // Verify no significant growth
  if (endMem.scratchHandles > startMem.scratchHandles + 100) {
    console.error('FAIL: Handle leak detected');
    process.exit(1);
  }
  if (endMem.wasmArenaUsed > 10000) {
    console.error('FAIL: WASM arena not being reset');
    process.exit(1);
  }
  
  console.log('PASS: No memory leaks detected');
}

stressTest().catch(e => {
  console.error('Error:', e);
  process.exit(1);
});
```

**Verification:** 
- `node js/stress-test.js` passes
- Memory stats show bounded growth

---

## M11: Documentation and Cleanup

Update documentation and remove dead code.

**Tasks:**

1. Delete `js/wasm_alloc.c`

2. Update `WASM.md`:
   - Add section on arena-based memory management
   - Document the scratch/persistent split
   - Explain the materialize/wrap pattern
   - Document `feather_arena_reset()` and when it's called

3. Add comments to key functions in `feather.js`:
   ```javascript
   /**
    * materialize(handle) - Deep copy handle to persistent value.
    * 
    * Handles are only valid during a single eval. To store values
    * persistently (in procs, namespaces, traces), materialize them.
    */
   
   /**
    * wrap(value) - Create fresh scratch handle from persistent value.
    *
    * When retrieving from persistent storage, wrap values to get
    * handles that C code can use during this eval.
    */
   
   /**
    * resetScratch() - Reclaim all scratch memory.
    *
    * Only call at top-level eval boundaries (evalDepth === 0).
    * Invalidates all handles from previous allocations.
    */
   ```

4. Update `src/arena.h` with usage documentation (done in M1)

**Verification:** Documentation is accurate, no dead code remains.

---

## M12: Verify and Test

Final verification across all platforms.

**Checklist:**

1. `mise build` succeeds
2. `mise test` passes (Go host — unchanged, uses FeatherHostOps)
3. `mise test:js` passes (WASM host — now with arenas)
4. Browser demo works (`js/index.html`)
5. `node js/stress-test.js` passes — memory bounded over 10k iterations
6. REPL session stays responsive after many commands

**Verification:** All tests pass. Memory is properly managed in WASM builds.

---

## Summary of Changes

| File | Change |
|------|--------|
| `src/arena.h` | New — arena API |
| `src/arena.c` | New — WASM arena implementation |
| `js/wasm_alloc.c` | Deleted — replaced by arena.c |
| `js/feather.js` | Major — scratch arena, materialize/wrap, evalDepth tracking |
| `js/mise.toml` | Minor — build command updates |
| `js/stress-test.js` | New — memory stress test |
| `WASM.md` | Updated — document memory model |

**Functions requiring materialize-on-store:**
- `feather_host_var_set` (line 440)
- `feather_host_ns_set_var` (line 740)
- `feather_host_proc_define` (line 515)
- `feather_host_ns_set_command` (line 809)
- `feather_host_trace_add` (line 1424)

**Functions requiring wrap-on-retrieve:**
- `feather_host_var_get` (line 416)
- `feather_host_ns_get_var` (line 729)
- `feather_host_proc_params` (line 541)
- `feather_host_proc_body` (line 552)
- `feather_host_ns_get_command` (line 795)
- `feather_host_trace_info` (line 1450)

**No changes needed:**
- Frame operations (frames live within single eval)
- Return options (accessed same eval)
- Foreign objects (use string handles, not object handles)
