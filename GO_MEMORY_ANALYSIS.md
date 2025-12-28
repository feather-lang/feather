# Go Memory Leak Analysis

## Root Cause

The Go implementation has a memory leak caused by unbounded growth of the `InternalInterp.objects` map.

### How Objects Are Managed

1. **Every object gets a unique handle** (`FeatherObj` = `uintptr`):
   ```go
   func (i *InternalInterp) internString(s string) FeatherObj {
       id := i.nextID
       i.nextID++
       i.objects[id] = NewStringObj(s)  // ← Stored forever
       return id
   }
   ```

2. **Objects are NEVER removed** from the `i.objects` map

3. **Every eval creates hundreds of temporary objects**:
   - Parsing creates string objects for each token
   - List operations create list objects
   - Expression evaluation creates intermediate values
   - Variable substitution creates objects
   - All these objects accumulate forever

4. **Persistent storage uses handles**, not copies:
   ```go
   // Variables store handles
   ns.vars[varName] = FeatherObj(value)

   // Procedures store handles
   type Procedure struct {
       name   FeatherObj
       params FeatherObj
       body   FeatherObj
   }
   ```

### Memory Growth Pattern

Over 10,000 iterations:
- **Start**: 249 KB
- **End**: 119,827 KB (~120 MB)
- **Growth**: 12,244 bytes/iteration
- **Total objects created**: Hundreds of thousands

## Comparison with WASM Implementation

The WASM implementation uses **arena-based memory management**:

1. **Scratch arena**: Temporary objects during eval, reset after completion
2. **Materialize/wrap pattern**: Persistent storage holds actual values, not handles
3. **Explicit cleanup**: `feather_arena_reset()` after each eval

The Go implementation lacks any cleanup mechanism.

## Proposed Fix: Post-Eval Object Collection

Add a mark-and-sweep cleanup after each top-level eval:

### Algorithm

```go
func (i *InternalInterp) collectUnusedObjects() {
    // Mark phase: build set of all referenced handles
    referenced := make(map[FeatherObj]bool)

    // Mark result and return options
    referenced[i.result] = true
    referenced[i.returnOptions] = true
    referenced[i.globalNS] = true
    referenced[i.scriptPath] = true

    // Mark all namespace variables
    for _, ns := range i.namespaces {
        for _, handle := range ns.vars {
            i.markHandle(referenced, handle)
        }
        // Mark procedure bodies/params
        for _, cmd := range ns.commands {
            if cmd.proc != nil {
                i.markHandle(referenced, cmd.proc.name)
                i.markHandle(referenced, cmd.proc.params)
                i.markHandle(referenced, cmd.proc.body)
            }
        }
    }

    // Mark all frames (usually empty after eval)
    for _, frame := range i.frames {
        i.markHandle(referenced, frame.cmd)
        i.markHandle(referenced, frame.args)
    }

    // Mark all trace scripts
    for _, traces := range i.varTraces {
        for _, t := range traces {
            i.markHandle(referenced, t.script)
        }
    }
    // Same for cmdTraces and execTraces

    // Sweep phase: delete unreferenced objects
    for handle := range i.objects {
        if !referenced[handle] {
            delete(i.objects, handle)
        }
    }
}

func (i *InternalInterp) markHandle(referenced map[FeatherObj]bool, handle FeatherObj) {
    if handle == 0 || referenced[handle] {
        return
    }
    referenced[handle] = true

    // Recursively mark compound objects
    if obj := i.objects[handle]; obj != nil {
        if items, err := AsList(obj); err == nil {
            for _, item := range items {
                // Items are *Obj, need to find their handles
                for h, o := range i.objects {
                    if o == item {
                        i.markHandle(referenced, h)
                    }
                }
            }
        }
        if dict, err := AsDict(obj); err == nil {
            for _, item := range dict.Items {
                for h, o := range i.objects {
                    if o == item {
                        i.markHandle(referenced, h)
                    }
                }
            }
        }
    }
}
```

### Integration

Call `collectUnusedObjects()` after each top-level eval completes:

```go
func (i *InternalInterp) Eval(script string) (FeatherResult, error) {
    scriptHandle := i.internString(script)
    defer i.collectUnusedObjects()  // ← Cleanup after eval

    res := callCEval(i.handle, scriptHandle, 0)
    // ... rest of eval logic
}
```

## Alternative: Reference Counting

Instead of mark-and-sweep, use reference counting:
- Increment refcount when storing in persistent storage
- Decrement when removing
- Delete when refcount reaches 0

**Pros**: Immediate cleanup
**Cons**: More complex, need to track all references, risk of cycles

## Alternative: Copy-on-Store

Store actual `*Obj` in persistent storage instead of handles:

```go
type Namespace struct {
    vars map[string]*Obj  // ← Store objects, not handles
}
```

**Pros**: Go GC handles cleanup automatically
**Cons**: Large refactor, need to change all persistent storage

## Recommendation

Implement **post-eval object collection** (mark-and-sweep):
- Minimal code changes
- Works with existing handle-based architecture
- Similar to WASM arena reset
- Predictable cleanup after each eval
