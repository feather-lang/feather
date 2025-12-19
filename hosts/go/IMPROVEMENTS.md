# Go Host Implementation: Problems and Improvements

This document catalogs the architectural problems with the current Go host implementation and proposes solutions.

## Executive Summary

The Go host implementation has fundamental design issues that defeat the purpose of the `TclHost` interface. The two most critical problems are:

1. **Manual handle tables** instead of leveraging Go's GC
2. **Handle proliferation** that defeats shimmering/caching optimizations

These issues result in memory leaks, poor performance, and an implementation that doesn't fulfill the architecture's design goals.

---

## Problem 1: Manual Handle Tables Instead of Go's GC

### Description

The code creates manual "handle tables" for every type, reimplementing reference management that Go's GC should handle:

| File | Handle Table | Lines |
|------|--------------|-------|
| `object.go` | `objHandles map[uintptr]*TclObj` | 27-29 |
| `vars.go` | `varsHandles map[uintptr]*VarTable` | 28-31 |
| `arena.go` | `arenaHandles map[uintptr]*Arena` | 23-26 |
| `channel.go` | `chanHandles map[uintptr]*Channel` | 31-33 |
| `host.go` | `ctxHandles map[uintptr]*HostContext` | 31-34 |
| `host.go` | `procHandles map[uintptr]*ProcDef` | 59-62 |

This violates the architecture which states: "Host provides GC, so no explicit reference counting in C."

### Impact

- Lock contention on every object access
- Memory leaks (handles allocated but never freed)
- Reimplements what `cgo.Handle` already provides

### Solution

Use `cgo.Handle` (Go 1.17+), the official mechanism for this exact problem:

```go
import "runtime/cgo"

func allocObjHandle(obj *TclObj) uintptr {
    if obj == nil {
        return 0
    }
    return uintptr(cgo.NewHandle(obj))
}

func getObj(h uintptr) *TclObj {
    if h == 0 {
        return nil
    }
    return cgo.Handle(h).Value().(*TclObj)
}

func freeObjHandle(h uintptr) {
    if h != 0 {
        cgo.Handle(h).Delete()
    }
}
```

Benefits:
- No locks needed
- No manual maps
- Explicit lifetime via `Delete()`
- Official, supported API

---

## Problem 2: Handle Proliferation Defeats Shimmering

### Description

The `TclHost` interface has many function pointers to enable shimmering - the lazy caching of multiple representations (string, int, list, etc.) on a single object. The current implementation completely defeats this by creating new handles on every access.

### Evidence

**Every variable read creates a new handle:**

`vars.go:219`:
```go
func goVarGet(...) uintptr {
    obj := table.Get(goName)
    return allocObjHandle(obj)  // New handle every time!
}
```

**List access duplicates objects:**

`object.go:546-547`:
```go
func goListIndex(h uintptr, idx C.size_t) uintptr {
    elem := list[idx].Dup()           // Copy the object
    return allocObjHandle(elem)        // New handle to the copy
}
```

**`asList` duplicates the entire list:**

`object.go:507-508`:
```go
for i, elem := range list {
    arr[i] = C.uintptr_t(allocObjHandle(elem.Dup()))
}
```

**`getStringPtr` allocates C memory every call:**

`object.go:400`:
```go
return C.CString(obj.stringRep)  // malloc + copy every time
```

### Impact

| Operation | Intended Cost | Actual Cost |
|-----------|---------------|-------------|
| `varGet` twice | 1 handle | 2 handles |
| `asInt` twice on same obj | 1 parse | 1 parse (cache useless due to copies) |
| `getStringPtr` twice | 0 allocs | 2 mallocs + 2 string copies |
| `listIndex` on 3 elements | 3 handles | 3 handles + 3 object copies |
| `asList` on 10-element list | 1 array + 10 handles | 1 malloc + 11 handles + 10 object copies |

### Solution

Handles should be stable identities assigned once at object creation:

```go
type TclObj struct {
    handle    cgo.Handle  // Assigned at creation, never changes
    stringRep string
    intRep    *int64
    doubleRep *float64
    listRep   []*TclObj
}

func NewString(s string) *TclObj {
    obj := &TclObj{stringRep: s}
    obj.handle = cgo.NewHandle(obj)
    return obj
}

func (o *TclObj) Handle() uintptr {
    return uintptr(o.handle)
}

// varGet returns the existing handle, not a new one
func goVarGet(...) uintptr {
    obj := table.Get(goName)
    if obj == nil {
        return 0
    }
    return obj.Handle()  // Same handle every time
}

// listIndex returns existing handle, no duplication
func goListIndex(h uintptr, idx C.size_t) uintptr {
    obj := getObj(h)
    list, _ := obj.AsList()
    if int(idx) >= len(list) {
        return 0
    }
    return list[idx].Handle()  // Existing handle, no copy
}
```

---

## Problem 3: C Memory Allocation

### Description

The architecture states "C core is allocation-free" but the Go host uses `C.malloc`/`C.calloc`:

| Location | Issue |
|----------|-------|
| `host.go:99` | `goFrameAlloc` uses `C.calloc` for frames |
| `arena.go:79,102` | `Arena.Alloc` uses `C.malloc` |
| `object.go:501` | `goAsList` uses `C.malloc` for return array |
| `arena.go:130` | `Arena.Strdup` leaks via `C.CString` |

### Solution

Use Go memory where possible:

```go
// Frames should be Go-allocated
func goFrameAlloc(ctxHandle C.uintptr_t) *C.TclFrame {
    frame := new(TclFrame)  // Go allocation
    // Use cgo.Handle to pass to C
    return frameToC(frame)
}
```

For arena, the C allocation is intentional (arena memory is accessed by C code), but the implementation should avoid `C.CString` leaks.

---

## Problem 4: Memory Leaks

### Description

Handles are allocated but rarely freed:

| Function | Leak |
|----------|------|
| `goVarGet` | New handle every read, never freed |
| `goArrayGet` | New handle every read, never freed |
| `goProcGetDef` | New handles for argList/body every call |
| `goCmdLookup` | New proc handle every lookup |
| `goListIndex` | New handle + dup every access |
| `goChanStdin/Stdout/Stderr` | New handle every call |
| `goGetStringPtr` | `C.CString` never freed |

### Solution

Define clear ownership model:

| Object Type | Lifetime | Who Frees |
|-------------|----------|-----------|
| Interpreter result | Until next command | C core before setting new result |
| Frame variables | Until frame pops | `goFrameFree` |
| Proc definitions | Until proc deleted | `cmdDelete` callback |
| Temporary strings | Until arena pops | Arena cleanup |

---

## Problem 5: Broken Arena Mark/Reset

### Description

- `Arena.Mark()` only returns offset in current chunk
- `Arena.Reset()` only resets offset, doesn't free chunks allocated after the mark

`arena.go:135-144`:
```go
func (a *Arena) Mark() int {
    return a.offset  // Only tracks offset, not chunk count
}

func (a *Arena) Reset(mark int) {
    if mark <= a.offset {
        a.offset = mark  // Doesn't free chunks!
    }
}
```

### Solution

Track chunk index in mark:

```go
type ArenaMark struct {
    chunkIndex int
    offset     int
}

func (a *Arena) Mark() ArenaMark {
    return ArenaMark{
        chunkIndex: len(a.chunks) - 1,
        offset:     a.offset,
    }
}

func (a *Arena) Reset(mark ArenaMark) {
    // Free chunks allocated after mark
    for i := len(a.chunks) - 1; i > mark.chunkIndex; i-- {
        C.free(a.chunks[i])
    }
    a.chunks = a.chunks[:mark.chunkIndex+1]
    a.sizes = a.sizes[:mark.chunkIndex+1]
    a.offset = mark.offset
}
```

---

## Problem 6: Incorrect List Quoting

### Description

`listQuote` wraps strings in `{}` but doesn't handle nested braces:

`object.go:234-253`:
```go
func listQuote(s string) string {
    // ...
    return "{" + s + "}"  // Broken for strings containing braces
}
```

A string like `a{b` becomes `{a{b}` which is invalid TCL.

### Solution

Proper TCL list quoting requires backslash escaping or checking brace balance:

```go
func listQuote(s string) string {
    if s == "" {
        return "{}"
    }

    // Check if simple (no special chars)
    needsQuoting := false
    braceBalance := 0
    for _, c := range s {
        switch c {
        case ' ', '\t', '\n', '\r', '"', '\\':
            needsQuoting = true
        case '{':
            braceBalance++
        case '}':
            braceBalance--
            if braceBalance < 0 {
                needsQuoting = true
            }
        }
    }

    if !needsQuoting && braceBalance == 0 {
        return s
    }

    if braceBalance == 0 && !strings.ContainsAny(s, "\\") {
        return "{" + s + "}"
    }

    // Fall back to backslash quoting
    return backslashQuote(s)
}
```

---

## Problem 7: Race Condition in Standard Channels

### Description

`initStdChannels` has a race condition with handle allocation:

`channel.go:62-96`:
```go
func initStdChannels() {
    chanMu.Lock()
    defer chanMu.Unlock()

    if stdinChan == nil {
        stdinChan = &Channel{...}
        chanHandles[allocChanHandleInternal(stdinChan)] = stdinChan
        //          ^^^^^^^^^^^^^^^^^^^^^^^^^ doesn't acquire lock (already held)
    }
}

// But then:
func goChanStdin(ctxHandle uintptr) uintptr {
    initStdChannels()
    return allocChanHandle(stdinChan)  // Allocates NEW handle each time!
}
```

### Solution

Standard channels should have fixed handles:

```go
const (
    stdinHandle  uintptr = 1
    stdoutHandle uintptr = 2
    stderrHandle uintptr = 3
)

func init() {
    // Reserve handles 1-3 for standard streams
    nextChanID = 4
}

func goChanStdin(ctxHandle uintptr) uintptr {
    initStdChannels()
    return stdinHandle  // Always the same handle
}
```

---

## Problem 8: Missing Extension Command Support

### Description

The interface supports extension commands (`TCL_CMD_EXTENSION`, `extInvoke`) but the implementation is stubbed:

`host_c.c:274-277`:
```c
static TclResult wrapExtInvoke(TclInterp* interp, void* handle,
                               int objc, TclObj** objv) {
    return TCL_ERROR;  // Does nothing
}
```

### Solution

Implement extension command registration:

```go
type TclCmdFunc func(interp *Interp, args []*TclObj) (*TclObj, error)

type HostContext struct {
    globalVars *VarTable
    procs      map[string]*ProcDef
    commands   map[string]*ExtCmd  // Add this
}

type ExtCmd struct {
    name string
    fn   TclCmdFunc
}

// Public API
func (ctx *HostContext) RegisterCommand(name string, fn TclCmdFunc) {
    ctx.commands[name] = &ExtCmd{name: name, fn: fn}
}
```

Update `goCmdLookup` to check extensions, implement `goExtInvoke` to call Go functions.

---

## Problem 9: Massive Stub Count

### Description

`host_c.c` has ~60+ stub functions. Major missing functionality:

- All dict operations
- Most string operations (index, range, replace, match, etc.)
- All filesystem operations
- All process/socket operations
- Event loop
- Regex
- Clock

### Impact

Many TCL commands will silently fail or return empty results.

---

## Problem 10: Unsafe Patterns

### Description

Several unsafe patterns that could cause issues:

**Arbitrary array cast:**
`object.go:506`:
```go
arr := (*[1 << 20]C.uintptr_t)(arrPtr)[:len(list):len(list)]
```

**Pointer arithmetic:**
`arena.go:113`:
```go
ptr := unsafe.Pointer(uintptr(chunkPtr) + uintptr(aligned))
```

---

## Recommended Fix Order

1. **Fix handle identity** (Problem 2) - Assign handles at object creation, return same handle on access
2. **Switch to cgo.Handle** (Problem 1) - Eliminate manual handle maps
3. **Define ownership model** (Problem 4) - Document and implement handle freeing
4. **Fix arena mark/reset** (Problem 5) - Track chunks properly
5. **Fix list quoting** (Problem 6) - Implement proper TCL quoting
6. **Add extension commands** (Problem 8) - Enable Go command registration
7. **Implement stubs** (Problem 9) - Priority based on test requirements

---

## Architecture Violations Summary

| Architecture Says | Code Does |
|-------------------|-----------|
| "GC-managed objects for TCL values" | Manual handle maps |
| "Host allocates from GC-managed memory" | Uses C.calloc for frames |
| "Arena for temporaries" | Arena uses C.malloc |
| "No malloc in C core" | Host uses C.malloc |
| "Shimmering for lazy conversion" | Copies defeat caching |
