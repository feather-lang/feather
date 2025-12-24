# Plan: Transparent Foreign Object Support in tclc

## Goals

1. **Elegant TCL experience** - foreign objects feel native, idiomatic syntax
2. **Minimal embedder burden** - exposing host types is declarative, not imperative
3. **Cross-runtime consistency** - same patterns work for Go, Node.js, Java, Swift

## Design Philosophy

**Complexity budget**: tclc stays minimal, host libraries absorb complexity.

The libraries we ship (Go, Node.js, Java, Swift) handle:
- Type registration and method dispatch
- Argument conversion (TclObj ↔ native types)
- Object-as-command registration
- Lifecycle management

tclc just needs hooks for the libraries to build on.

## User Experience Goals

### For TCL Users - Elegant Syntax

```tcl
# Object-as-command pattern (Tk style)
set mux [Mux new]
$mux handle "/" serveIndex
$mux listen 8080

# Introspection works
info type $mux           ;# => Mux
info methods $mux        ;# => handle listen close destroy

# Objects are values - pass them around
proc setupRoutes {m} {
    $m handle "/" indexHandler
    $m handle "/api" apiHandler
}
setupRoutes $mux

# Destruction is explicit
$mux destroy
```

### For Embedders - Declarative API

```go
// Go: Define a type in ~10 lines
tclc.DefineType[*http.ServeMux]("Mux", tclc.TypeDef{
    New: func() *http.ServeMux {
        return http.NewServeMux()
    },
    Methods: tclc.Methods{
        "handle": func(m *http.ServeMux, pattern string, handler tclc.Proc) {
            m.HandleFunc(pattern, handler.AsHTTPHandler())
        },
        "listen": func(m *http.ServeMux, port int) error {
            return http.ListenAndServe(fmt.Sprintf(":%d", port), m)
        },
    },
})
// That's it! Mux is now available in TCL
```

```javascript
// Node.js: Same pattern
tclc.defineType('Server', {
    new: () => http.createServer(),
    methods: {
        listen: (server, port) => server.listen(port),
        close: (server) => server.close(),
    }
});
```

```java
// Java: Annotation or builder
@TclType("Connection")
public class ConnectionWrapper {
    @TclConstructor
    public static Connection create(String url) {
        return DriverManager.getConnection(url);
    }

    @TclMethod
    public ResultSet query(Connection conn, String sql) {
        return conn.createStatement().executeQuery(sql);
    }
}
```

```swift
// Swift: Protocol-based
extension URLSession: TclExposable {
    static var tclTypeName = "URLSession"

    static func tclNew() -> URLSession { .shared }

    var tclMethods: [String: TclMethod] {
        ["fetch": { url in self.data(from: URL(string: url)!) }]
    }
}
```

## Architecture

### Layer 1: tclc Core (C) - Minimal Hooks

tclc provides the primitives. The host libraries build ergonomics on top.

```c
typedef struct TclForeignOps {
    // Check if an object is a foreign object
    int (*is_foreign)(TclInterp interp, TclObj obj);

    // Get the type name (e.g., "Mux", "Connection")
    TclObj (*type_name)(TclInterp interp, TclObj obj);

    // Get string representation (for shimmering/display)
    TclObj (*string_rep)(TclInterp interp, TclObj obj);

    // List method names (for introspection)
    TclObj (*methods)(TclInterp interp, TclObj obj);

    // Invoke a method: obj.method(args)
    TclResult (*invoke)(TclInterp interp, TclObj obj,
                        TclObj method, TclObj args);

    // Destructor callback (called when object is destroyed)
    void (*destroy)(TclInterp interp, TclObj obj);
} TclForeignOps;
```

**tclc changes:**
1. Add `TclForeignOps` to `TclHostOps`
2. `info type $obj` - returns type name for any value
3. `info methods $obj` - returns method list (empty for non-foreign)
4. String shimmering calls `foreign.string_rep()` for foreign objects

### Layer 2: Host Library (Go/JS/Java/Swift) - The Magic

The library provides:

**A. Type Registry**
```go
type TypeDef struct {
    Name       string
    New        any                    // Constructor function
    Methods    map[string]any         // Method implementations
    StringRep  func(any) string       // Custom string representation
    Destroy    func(any)              // Cleanup callback
}

var typeRegistry = map[string]*TypeDef{}
```

**B. Automatic Command Registration**

When `DefineType("Mux", ...)` is called:
1. Register `Mux` as a command (the constructor)
2. `Mux new` creates instance, registers `mux1` as command
3. `mux1 handle ...` dispatches to Methods["handle"]
4. `mux1 destroy` calls Destroy callback, unregisters command

**C. Argument Conversion**

The library automatically converts TclObj ↔ native types:
- `string` ↔ TclObj (string rep)
- `int`, `int64` ↔ TclObj (integer rep)
- `float64` ↔ TclObj (double rep)
- `[]T` ↔ TclObj (list rep)
- `map[string]T` ↔ TclObj (dict rep)
- `*ForeignType` ↔ TclObj (foreign rep)
- `tclc.Proc` ↔ TclObj (callable script/proc name)
- `error` → TCL_ERROR with message

**D. Object-as-Command Pattern**

When a foreign object is created:
```go
func (lib *Library) CreateForeign(typeName string, value any) TclObj {
    // 1. Create the foreign object
    obj := lib.interp.NewForeign(typeName, value)

    // 2. Generate unique handle name
    handle := lib.nextHandle(typeName)  // "mux1", "mux2", ...

    // 3. Register handle as command that dispatches to object
    lib.interp.RegisterCommand(handle, func(cmd, args TclObj) TclResult {
        method := lib.interp.ListShift(args)
        return lib.foreign.invoke(obj, method, args)
    })

    // 4. Set up destructor via command trace
    lib.interp.TraceCommand(handle, "delete", func() {
        lib.foreign.destroy(obj)
    })

    // 5. Return handle (which is also the object's string rep)
    return lib.interp.Intern(handle)
}
```

## Implementation Roadmap

### Phase 1: tclc Core Changes

**Files:**
- `src/tclc.h` - Add `TclForeignOps` to `TclHostOps`
- `src/builtins.c` - Add `info type`, `info methods` subcommands

**Scope:**
1. Define `TclForeignOps` struct with 6 callbacks
2. Add `foreign` field to `TclHostOps`
3. Extend `info` command:
   - `info type $obj` → type name (or "string"/"list"/"dict"/"int" for builtins)
   - `info methods $obj` → method list (calls `foreign.methods`)
4. Modify string shimmering path to check `foreign.is_foreign()` and call `foreign.string_rep()`

### Phase 2: Go Host - Low-Level Support

**Files:**
- `interp/tclc.go` - Add foreign fields to Object, implement callbacks
- `interp/callbacks.go` - Export foreign ops to C
- `interp/callbacks.c` - C wrapper functions

**Scope:**
1. Add to `Object` struct:
   ```go
   isForeign    bool
   foreignType  string
   foreignValue any
   ```
2. Implement `TclForeignOps` callbacks
3. Add `NewForeign(typeName string, value any) TclObj` method
4. Add type registry: `map[string]*ForeignTypeDef`

### Phase 3: Go Library - High-Level API

**Files:**
- New package: `tclc/` (or extend `interp/`)

**Scope:**
1. `DefineType[T]()` - generic type registration with reflection
2. Automatic argument conversion via reflection
3. Object-as-command registration
4. Handle generation (`mux1`, `mux2`, ...)
5. Lifecycle management via command traces

### Phase 4: Documentation & Examples

1. Example: HTTP server type
2. Example: Database connection type
3. Example: Custom data structure
4. Guide for each runtime (Node.js, Java, Swift stubs)

## Files to Modify

| File | Changes |
|------|---------|
| `src/tclc.h` | Add `TclForeignOps` struct, add to `TclHostOps` |
| `src/builtins.c` | `info type`, `info methods` subcommands |
| `interp/tclc.go` | Foreign fields in Object, type registry, NewForeign |
| `interp/callbacks.go` | Implement 6 foreign ops callbacks |
| `interp/callbacks.c` | C wrappers for foreign ops |
| `interp/library.go` (new) | High-level DefineType API |

## Object Storage

```go
type Object struct {
    // Existing representations (shimmering)
    stringVal string
    intVal    int64
    isInt     bool
    dblVal    float64
    isDouble  bool
    listItems []TclObj
    isList    bool
    dictItems map[string]TclObj
    isDict    bool

    // New: Foreign object support
    isForeign    bool
    foreignType  string   // "Mux", "Connection", etc.
    foreignValue any      // The actual Go/host value
}
```

**Shimmering behavior:**
- Foreign objects can shimmer to string (via `string_rep`)
- String rep is cached in `stringVal`
- `isForeign` remains true - object retains foreign identity
- Cannot shimmer to int/list/dict (returns error)

## Testing Strategy

1. **Unit tests** (in tclc test harness):
   - `info type` returns correct type for all value types
   - `info methods` returns method list for foreign objects
   - Foreign objects have string representation
   - Foreign objects pass through lists/dicts unchanged

2. **Integration tests** (Go):
   - Define type, create instance, call methods
   - Object-as-command dispatch works
   - Argument conversion for various types
   - Error handling (wrong type, missing method)
   - Lifecycle: destroy cleans up

3. **Example applications**:
   - HTTP server with routes
   - Database with queries
   - File handle wrapper

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Method syntax | `$obj method args` | Tk-style, most TCL-idiomatic |
| Object lifecycle | Explicit `$obj destroy` | Predictable, no GC surprises |
| Handle format | `typename1`, `typename2` | Readable, debuggable |
| Type checking | Runtime, not compile-time | TCL is dynamic |
| Comparison | By handle string | Consistent with TCL semantics |
