# Go Host Implementation

This directory contains the Go host implementation for the TCL core, using CGO to interface with the C library.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      bin/tclgo                           │
│                    (executable)                          │
└─────────────────────────────────────────────────────────┘
                          │
                          │ compiled from
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    hosts/go/*.go                         │
│                   (Go host code)                         │
│                                                          │
│  main.go ────► Entry point, REPL, script execution      │
│  host.go ────► TclHost callback wrappers                 │
│  object.go ──► TclObj with Go-native representations     │
│  vars.go ────► Variable storage using Go maps            │
│  arena.go ───► Arena using Go slices                     │
│  channel.go ─► I/O channels wrapping os.File             │
└─────────────────────────────────────────────────────────┘
                          │
                          │ CGO
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    host_c.c                              │
│            (CGO bridge / callback shims)                 │
│                                                          │
│  Exports C functions that call back into Go              │
│  Required because Go cannot export function pointers     │
└─────────────────────────────────────────────────────────┘
                          │
                          │ links
                          ▼
┌─────────────────────────────────────────────────────────┐
│                build/libtclc.a                           │
│              (static library from core/)                 │
│                                                          │
│  Public API: core/tclc.h                                 │
└─────────────────────────────────────────────────────────┘
```

## Files

| File | Purpose |
|------|---------|
| `main.go` | Entry point. Handles CLI flags, runs REPL or scripts. |
| `host.go` | Go-side `TclHost` callback implementations. |
| `host_c.c` | C shims that bridge between C function pointers and Go. |
| `object.go` | `TclObj` using Go strings, slices, and interfaces. |
| `vars.go` | Variable storage using `map[string]*TclObj`. |
| `arena.go` | Arena allocator using C memory for CGO compatibility. |
| `channel.go` | I/O channels wrapping `*os.File` and `bufio`. |

## Build

```bash
# Build the Go host (requires libtclc.a to exist)
make build-go       # Creates bin/tclgo

# Build everything
make build-all      # Creates bin/tclc, bin/tclgo, and libraries
```

## Testing

```bash
# Run differential tests with Go host
TCLC_INTERP=bin/tclgo make diff-all
```

---

## Critical Architecture Principles

These principles MUST be followed when modifying the Go host. Violations will cause memory leaks, performance issues, or incorrect behavior.

### 1. Handle Identity: Assign Once, Return Same

**Principle:** Every Go object passed to C must have a stable `cgo.Handle` assigned at creation time. The same handle must be returned on every access.

**Why:** The TclHost interface enables shimmering (lazy caching of type conversions). If `varGet` returns a different handle each time, C code cannot detect "same object" and caching is useless.

```go
// CORRECT: Handle assigned at creation, returned unchanged
type TclObj struct {
    handle    cgo.Handle  // Assigned in constructor
    stringRep string
    intRep    *int64      // Cached on first AsInt() call
}

func NewString(s string) *TclObj {
    obj := &TclObj{stringRep: s}
    obj.handle = cgo.NewHandle(obj)  // Assign once
    return obj
}

func (o *TclObj) Handle() uintptr {
    return uintptr(o.handle)  // Always same value
}

// In goVarGet:
return obj.Handle()  // NOT: allocObjHandle(obj)
```

```go
// WRONG: Creates new handle every access
func goVarGet(...) uintptr {
    obj := table.Get(name)
    return allocObjHandle(obj)  // New handle each time!
}
```

### 2. Use cgo.Handle, Not Manual Maps

**Principle:** Use `runtime/cgo.Handle` for all Go↔C handle management. Never use manual `map[uintptr]*T` with mutexes.

**Why:** `cgo.Handle` is the official Go mechanism, requires no locks, and has proper GC integration.

```go
// CORRECT: Using cgo.Handle
import "runtime/cgo"

func getObj(h uintptr) *TclObj {
    if h == 0 { return nil }
    return cgo.Handle(h).Value().(*TclObj)
}

func freeObj(obj *TclObj) {
    if obj.handle != 0 {
        obj.handle.Delete()
    }
}
```

```go
// WRONG: Manual handle map with locks
var (
    objMu      sync.RWMutex
    objHandles = make(map[uintptr]*TclObj)
    nextObjID  uintptr = 1
)
```

### 3. Never Duplicate Objects on Access

**Principle:** Functions that return existing objects (like `listIndex`, `varGet`, `arrayGet`) must return handles to the EXISTING object, not a copy.

**Why:** Duplication defeats shimmering. If you duplicate, cached type conversions are lost.

```go
// CORRECT: Return existing element
func goListIndex(h uintptr, idx C.size_t) uintptr {
    list, _ := obj.AsList()
    return list[idx].Handle()  // Existing object
}

// WRONG: Duplicates the element
func goListIndex(h uintptr, idx C.size_t) uintptr {
    list, _ := obj.AsList()
    elem := list[idx].Dup()        // Unnecessary copy
    return allocObjHandle(elem)    // New handle to copy
}
```

### 4. Arena Mark/Reset Must Track Chunks

**Principle:** Arena marks must capture both the chunk index AND offset. Reset must free chunks allocated after the mark.

**Why:** If only offset is tracked, chunks allocated after a mark are leaked on reset.

```go
// CORRECT: Pack chunk index into mark
func (a *Arena) MarkPacked() uint64 {
    chunkIdx := len(a.chunks) - 1
    if chunkIdx < 0 { chunkIdx = 0 }
    return uint64(chunkIdx)<<32 | uint64(a.offset)
}

func (a *Arena) ResetPacked(packed uint64) {
    chunkIdx := int(packed >> 32)
    offset := int(packed & 0xFFFFFFFF)
    // Free chunks allocated after mark
    for i := len(a.chunks) - 1; i > chunkIdx; i-- {
        C.free(a.chunks[i])
    }
    a.chunks = a.chunks[:chunkIdx+1]
    a.offset = offset
}
```

### 5. List Quoting Must Handle Brace Balance

**Principle:** When converting lists to strings, `listQuote` must check brace balance. Unbalanced braces require backslash escaping.

**Why:** Simple `{...}` wrapping breaks for strings like `a{b` which would become `{a{b}` (invalid TCL).

```go
// CORRECT: Check balance before brace quoting
func listQuote(s string) string {
    braceBalance := 0
    hasBackslash := false
    needsQuoting := false

    for _, c := range s {
        switch c {
        case '{': braceBalance++
        case '}':
            braceBalance--
            if braceBalance < 0 { needsQuoting = true }
        case '\\': hasBackslash = true; needsQuoting = true
        case ' ', '\t', '\n': needsQuoting = true
        }
    }

    if !needsQuoting && braceBalance == 0 { return s }
    if braceBalance == 0 && !hasBackslash { return "{" + s + "}" }
    return backslashQuote(s)  // Fall back to escaping
}
```

### 6. Standard Channels Use Fixed Handles

**Principle:** stdin, stdout, stderr must always return the SAME handle.

**Why:** Allocating new handles causes unbounded growth and violates identity expectations.

```go
// CORRECT: Fixed handles initialized once
var (
    stdinHandle, stdoutHandle, stderrHandle uintptr
    stdOnce sync.Once
)

func initStdChannels() {
    stdOnce.Do(func() {
        stdinChan = &Channel{...}
        stdinChan.handle = cgo.NewHandle(stdinChan)
        stdinHandle = uintptr(stdinChan.handle)
        // ... same for stdout, stderr
    })
}

func goChanStdin(ctxHandle uintptr) uintptr {
    initStdChannels()
    return stdinHandle  // Always same handle
}
```

### 7. Cache C Strings in Objects

**Principle:** When `getStringPtr` returns a C string, cache it in the object to avoid repeated allocation.

**Why:** `C.CString()` mallocs on every call. Without caching, repeated access leaks memory.

```go
type TclObj struct {
    // ...
    cachedCStr *C.char  // Cached C string
}

func goGetStringPtr(h uintptr, lenOut *C.size_t) *C.char {
    obj := getObj(h)
    if obj.cachedCStr == nil {
        obj.cachedCStr = C.CString(obj.stringRep)
    }
    return obj.cachedCStr
}

func freeObj(obj *TclObj) {
    if obj.cachedCStr != nil {
        C.free(unsafe.Pointer(obj.cachedCStr))
    }
    obj.handle.Delete()
}
```

---

## CGO Bridge

Go cannot directly export function pointers to C. The `host_c.c` file provides:

1. **Static C functions** that match `TclHost` callback signatures
2. **Go exports** called via `//export` comments in Go files
3. **Handle conversion** between C pointers and Go handles

Example flow for `varGet`:
```
C core calls host->varGet(ctx, name)
    → host_c.c: wrapVarGet() casts and calls goVarGet()
        → Go: //export goVarGet
            → vars.go: table.Get(name).Handle()
```

## Extension Commands

Go functions can be registered as TCL commands:

```go
type TclCmdFunc func(interp *TclInterp, args []*TclObj) (*TclObj, error)

// Registration (in HostContext)
ctx.RegisterCommand("mycommand", func(interp *TclInterp, args []*TclObj) (*TclObj, error) {
    return NewString("result"), nil
})
```

The `goCmdLookup` function checks extension commands first, then procs.

## Advantages Over C Host

1. **Garbage Collection** - Go GC handles object lifetimes via `cgo.Handle`
2. **Native strings** - Go strings handle Unicode properly
3. **Maps** - Variable tables use Go's built-in hash maps
4. **Error handling** - Can use Go's panic/recover for errors
5. **Type safety** - Compile-time type checking for callbacks
