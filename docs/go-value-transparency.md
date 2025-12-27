# Go Value Transparency Design

## Goal

Make the Go host work primarily with Go values, with automatic conversion to/from Feather `*Obj` representations.

## Architecture

### 1. Core `*Obj` type remains for C interop

```go
type Obj struct {
    bytes  string
    intrep ObjType
    cstr   *C.char
}
```

The shimmering system is preserved but becomes transparent to Go code.

### 2. Generic conversion functions

```go
// Extract typed Go value from *Obj
func Get[T any](o *Obj) (T, error)

// Create *Obj from any Go value
func New[T any](v T) *Obj

// Convenience: panic on error
func Must[T any](v T, err error) T
```

**Conversion rules:**

- `Get[string](o)` → uses `o.String()` (always succeeds)
- `Get[int](o)` → uses `AsInt(o)` with shimmering
- `Get[[]string](o)` → uses `AsList(o)`, converts each element
- `Get[map[string]any](o)` → uses `AsDict(o)`, converts values
- `Get[T](o)` where T is custom → extracts from `ForeignType`

### 3. Automatic command marshaling

```go
// Commands can use any of these signatures:
func(i *Interp, arg1 T1, arg2 T2, ...) R
func(i *Interp, arg1 T1, arg2 T2, ...) (R, error)
func(i *Interp, arg1 T1, arg2 T2, ...) error
func(i *Interp, args ...T) R

// Register with automatic marshaling
interp.RegisterGoCommand("mycommand", func(i *Interp, name string, count int) (string, error) {
    return fmt.Sprintf("Processed %d items for %s", count, name), nil
})
```

**Implementation:**

```go
func (i *Interp) RegisterGoCommand(name string, fn any) {
    wrapped := wrapGoFunc(fn)
    i.RegisterCommand(name, wrapped)
}

func wrapGoFunc(fn any) CommandFunc {
    fnType := reflect.TypeOf(fn)
    fnValue := reflect.ValueOf(fn)

    return func(i *Interp, cmd *Obj, args []*Obj) Result {
        // Build arguments using reflection + Get[T]
        callArgs := []reflect.Value{reflect.ValueOf(i)}

        for idx := 1; idx < fnType.NumIn(); idx++ {
            paramType := fnType.In(idx)

            // Handle variadic
            if idx >= len(args) {
                break
            }

            // Convert *Obj to expected type
            goVal := convertToType(args[idx], paramType)
            callArgs = append(callArgs, goVal)
        }

        // Call function
        results := fnValue.Call(callArgs)

        // Convert return values to Result
        return resultsToResult(results)
    }
}
```

### 4. Transparent Go value embedding

Any Go value can be embedded in `*Obj` and participate in shimmering:

```go
// Store native Go slice
list := New([]string{"a", "b", "c"})

// Converts to Feather list automatically when needed
items, _ := AsList(list)  // []*Obj{NewString("a"), ...}

// But can also get back as Go value
goSlice := Must(Get[[]string](list))  // []string{"a", "b", "c"}
```

## Usage Examples

### Example 1: Pure Go command

```go
interp.RegisterGoCommand("process", func(i *Interp, input string, count int, opts []string) (string, error) {
    // Entire function works with Go values
    // No *Obj in sight!

    result := strings.Repeat(input, count)
    for _, opt := range opts {
        result = applyOption(result, opt)
    }
    return result, nil
})
```

From TCL:
```tcl
set result [process "hello" 3 {opt1 opt2}]
```

### Example 2: Mixed Go/Feather

```go
func MyCommand(i *Interp, cmd *Obj, args []*Obj) Result {
    // Get Go values when you need them
    filename := Must(Get[string](args[0]))

    // Work with Go values
    data, err := os.ReadFile(filename)
    if err != nil {
        return Error(err.Error())
    }

    // Return Go values - automatically converted
    return OK(New(string(data)))
}
```

### Example 3: Custom types

```go
type Connection struct {
    Host string
    Port int
}

func (c *Connection) Send(msg string) error { /* ... */ }

interp.RegisterGoCommand("dial", func(i *Interp, host string, port int) (*Connection, error) {
    conn := &Connection{Host: host, Port: port}
    // Returns Go value - stored as ForeignType in *Obj
    return conn, nil
})

interp.RegisterGoCommand("send", func(i *Interp, conn *Connection, msg string) error {
    // Get the Go value back automatically
    return conn.Send(msg)
})
```

From TCL:
```tcl
set conn [dial "localhost" 8080]
send $conn "Hello, world!"
```

## Implementation Strategy

### Phase 1: Generic conversion (highest value)

1. Implement `Get[T any](o *Obj) (T, error)` with:
   - Built-in type support: string, int, bool, float64, []T, map[string]T
   - ForeignType extraction for custom types
   - Recursive conversion for nested structures

2. Implement `New[T any](v T) *Obj` with:
   - Built-in type detection
   - Automatic ForeignType wrapping for custom types
   - Shimmering-compatible representations

### Phase 2: Command marshaling (high convenience)

3. Implement `RegisterGoCommand` with reflection-based wrapping
4. Support various function signatures
5. Automatic error handling and result conversion

### Phase 3: Enhanced shimmering (completeness)

6. Make ForeignType values participate in shimmering:
   - `[]string` stored as ForeignType can shimmer to ListType
   - `map[string]any` can shimmer to DictType
   - Preserve Go value when possible

## Benefits

1. **Natural Go idioms**: Commands written in idiomatic Go
2. **Type safety**: Compile-time checking where possible
3. **No duplication**: Don't need parallel type hierarchies
4. **Gradual adoption**: Can mix both styles
5. **C interop preserved**: `*Obj` and shimmering still work for C layer
6. **Performance**: Lazy conversion via shimmering
7. **Extensibility**: Users can add custom type conversions

## Trade-offs

- Reflection cost for command calls (acceptable for script interpretation)
- Type conversion errors are runtime, not compile-time (but caught at command registration)
- Added complexity in conversion layer (but isolated to `convert.go`)

## Files to modify

- `convert.go` - add generic `Get`/`New` functions
- `interp.go` - add `RegisterGoCommand`
- `objtype_foreign.go` - enhance ForeignType to support more conversions
- New: `marshal.go` - reflection-based command wrapper

## Open questions

1. Should we support struct tags for custom conversion? (`tcl:"name"`)
2. How to handle optional arguments? (variadic? options struct?)
3. Should `Get` support concurrent access patterns?
4. Performance: cache reflection info per command?
