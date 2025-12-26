# Proposed Shimmering Design

## Core Concepts

Two types, mirroring TCL:

- `*Obj` — a Feather value
- `ObjType` — interface for internal representation behavior

**Design Constraints:**

- No thread safety implied (like a regular Go map)
- Shallow copy semantics for `Copy()` and `Dup()`
- Nil `*Obj` returns zero values (0, "", empty list, etc.)
- Equality is identity-based via handles

---

## The Object

```go
// Obj is a Feather value.
type Obj struct {
    bytes  string   // string representation ("" = empty string if intrep == nil)
    intrep ObjType  // internal representation (nil = pure string)
    cstr   *C.char  // cached C string for ops.string.get; freed on release
}
```

**String Representation Rules:**

- `bytes == ""` with `intrep == nil` → empty string value
- `bytes == ""` with `intrep != nil` → string rep needs regeneration via `UpdateString()`

---

## ObjType Interface (Minimal)

```go
// ObjType defines the core behavior for an internal representation.
type ObjType interface {
    // Name returns the type name (e.g., "int", "list").
    Name() string
    
    // UpdateString regenerates string representation from this internal rep.
    UpdateString() string
    
    // Dup creates a copy of this internal representation.
    Dup() ObjType
}
```

---

## Conversion Interfaces (Optional, Per-Type)

Types implement only the conversions they support:

```go
// IntoInt can convert directly to int64.
type IntoInt interface {
    IntoInt() (int64, bool)
}

// IntoDouble can convert directly to float64.
type IntoDouble interface {
    IntoDouble() (float64, bool)
}

// IntoList can convert directly to a list.
type IntoList interface {
    IntoList() ([]*Obj, bool)
}

// IntoDict can convert directly to a dictionary.
type IntoDict interface {
    IntoDict() (map[string]*Obj, []string, bool)
}

// IntoBool can convert directly to a boolean.
type IntoBool interface {
    IntoBool() (bool, bool)
}
```

---

## Concrete Types

### IntType

```go
type IntType int64

func (t IntType) Name() string         { return "int" }
func (t IntType) Dup() ObjType         { return t }
func (t IntType) UpdateString() string { return strconv.FormatInt(int64(t), 10) }

// IntType can become int, double, bool
func (t IntType) IntoInt() (int64, bool)      { return int64(t), true }
func (t IntType) IntoDouble() (float64, bool) { return float64(t), true }
func (t IntType) IntoBool() (bool, bool)      { return t != 0, true }
```

### DoubleType

```go
type DoubleType float64

func (t DoubleType) Name() string         { return "double" }
func (t DoubleType) Dup() ObjType         { return t }
func (t DoubleType) UpdateString() string { /* format float */ }

// DoubleType can become int, double
func (t DoubleType) IntoInt() (int64, bool)      { return int64(t), true }
func (t DoubleType) IntoDouble() (float64, bool) { return float64(t), true }
```

### ListType

```go
type ListType []*Obj

func (t ListType) Name() string         { return "list" }
func (t ListType) Dup() ObjType         { return ListType(slices.Clone(t)) }
func (t ListType) UpdateString() string { /* format as TCL list */ }

// ListType can become list, and dict (if even length)
func (t ListType) IntoList() ([]*Obj, bool) { return t, true }

func (t ListType) IntoDict() (map[string]*Obj, []string, bool) {
    if len(t)%2 != 0 {
        return nil, nil, false
    }
    items := make(map[string]*Obj)
    var order []string
    for i := 0; i < len(t); i += 2 {
        key := t[i].String()
        if _, exists := items[key]; !exists {
            order = append(order, key)
        }
        items[key] = t[i+1]
    }
    return items, order, true
}
```

### DictType

```go
type DictType struct {
    Items map[string]*Obj
    Order []string
}

func (t *DictType) Name() string         { return "dict" }
func (t *DictType) Dup() ObjType         { /* deep copy */ }
func (t *DictType) UpdateString() string { /* format as key-value pairs */ }

// DictType can become dict and list
func (t *DictType) IntoDict() (map[string]*Obj, []string, bool) {
    return t.Items, t.Order, true
}

func (t *DictType) IntoList() ([]*Obj, bool) {
    list := make([]*Obj, 0, len(t.Order)*2)
    for _, k := range t.Order {
        list = append(list, NewString(k), t.Items[k])
    }
    return list, true
}
```

### ForeignType

```go
type ForeignType struct {
    TypeName string
    Value    any
}

func (t *ForeignType) Name() string         { return t.TypeName }
func (t *ForeignType) Dup() ObjType         { return t }
func (t *ForeignType) UpdateString() string { return fmt.Sprintf("<%s:%p>", t.TypeName, t.Value) }

// ForeignType implements no conversion interfaces — it's opaque
```

---

## Free-Standing Conversion Functions

`Obj` doesn't know about types. Conversions are free functions:

```go
// AsInt converts o to int64, shimmering if needed.
func AsInt(o *Obj) (int64, error) {
    // Try direct conversion
    if c, ok := o.intrep.(IntoInt); ok {
        if v, ok := c.IntoInt(); ok {
            o.intrep = IntType(v)
            return v, nil
        }
    }
    // Fallback: parse string
    v, err := strconv.ParseInt(o.String(), 10, 64)
    if err != nil {
        return 0, fmt.Errorf("expected integer but got %q", o.String())
    }
    o.intrep = IntType(v)
    return v, nil
}

// AsDouble converts o to float64, shimmering if needed.
func AsDouble(o *Obj) (float64, error) {
    if c, ok := o.intrep.(IntoDouble); ok {
        if v, ok := c.IntoDouble(); ok {
            o.intrep = DoubleType(v)
            return v, nil
        }
    }
    v, err := strconv.ParseFloat(o.String(), 64)
    if err != nil {
        return 0, fmt.Errorf("expected number but got %q", o.String())
    }
    o.intrep = DoubleType(v)
    return v, nil
}

// AsList converts o to a list, shimmering if needed.
func AsList(o *Obj) ([]*Obj, error) {
    if c, ok := o.intrep.(IntoList); ok {
        if v, ok := c.IntoList(); ok {
            o.intrep = ListType(v)
            return v, nil
        }
    }
    items, err := parseList(o.String())
    if err != nil {
        return nil, err
    }
    o.intrep = ListType(items)
    return items, nil
}

// AsDict converts o to a dictionary, shimmering if needed.
func AsDict(o *Obj) (*DictType, error) {
    if c, ok := o.intrep.(IntoDict); ok {
        if items, order, ok := c.IntoDict(); ok {
            d := &DictType{Items: items, Order: order}
            o.intrep = d
            return d, nil
        }
    }
    // Fallback: parse as list, then convert
    list, err := AsList(o)
    if err != nil {
        return nil, err
    }
    if len(list)%2 != 0 {
        return nil, fmt.Errorf("missing value to go with key")
    }
    d := &DictType{Items: make(map[string]*Obj)}
    for i := 0; i < len(list); i += 2 {
        key := list[i].String()
        if _, exists := d.Items[key]; !exists {
            d.Order = append(d.Order, key)
        }
        d.Items[key] = list[i+1]
    }
    o.intrep = d
    return d, nil
}

// AsBool converts o to bool, shimmering if needed.
func AsBool(o *Obj) (bool, error) {
    if c, ok := o.intrep.(IntoBool); ok {
        if v, ok := c.IntoBool(); ok {
            return v, nil
        }
    }
    // Fallback: try as int, then check truthiness
    if v, err := AsInt(o); err == nil {
        return v != 0, nil
    }
    // String truthiness
    s := strings.ToLower(o.String())
    switch s {
    case "true", "yes", "on", "1":
        return true, nil
    case "false", "no", "off", "0":
        return false, nil
    }
    return false, fmt.Errorf("expected boolean but got %q", o.String())
}
```

---

## Methods on Obj (Minimal)

Only essential methods that don't involve type-specific logic:

```go
func (o *Obj) String() string {
    if o.bytes != "" {
        return o.bytes
    }
    if o.intrep != nil {
        o.bytes = o.intrep.UpdateString()
    }
    return o.bytes
}

func (o *Obj) Type() string {
    if o.intrep == nil {
        return "string"
    }
    return o.intrep.Name()
}

func (o *Obj) Invalidate() {
    o.bytes = ""
    if o.cstr != nil {
        C.free(unsafe.Pointer(o.cstr))
        o.cstr = nil
    }
}

func (o *Obj) Copy() *Obj {
    if o.intrep == nil {
        return &Obj{bytes: o.bytes}
    }
    return &Obj{bytes: o.bytes, intrep: o.intrep.Dup()}
}
```

---

## Constructors

```go
func NewString(s string) *Obj {
    return &Obj{bytes: s}
}

func NewInt(v int64) *Obj {
    return &Obj{intrep: IntType(v)}
}

func NewDouble(v float64) *Obj {
    return &Obj{intrep: DoubleType(v)}
}

func NewList(items ...*Obj) *Obj {
    return &Obj{intrep: ListType(items)}
}

func NewDict() *Obj {
    return &Obj{intrep: &DictType{Items: make(map[string]*Obj)}}
}

func NewForeign(typeName string, value any) *Obj {
    return &Obj{intrep: &ForeignType{TypeName: typeName, Value: value}}
}
```

---

## List and Dict Operations (Free Functions)

List operations are infallible — any value can become a list (wrapped as single element if needed).

```go
func ListLen(o *Obj) int {
    list, _ := AsList(o)
    return len(list)
}

func ListAt(o *Obj, i int) *Obj {
    list, _ := AsList(o)
    if i < 0 || i >= len(list) {
        return nil
    }
    return list[i]
}

func ListAppend(o *Obj, elem *Obj) {
    list, _ := AsList(o)
    o.intrep = ListType(append(list, elem))
    o.Invalidate()
}

func DictGet(o *Obj, key string) (*Obj, bool) {
    d, err := AsDict(o)
    if err != nil {
        return nil, false
    }
    v, ok := d.Items[key]
    return v, ok
}

func DictSet(o *Obj, key string, val *Obj) {
    d, _ := AsDict(o)
    if d == nil {
        d = &DictType{Items: make(map[string]*Obj)}
        o.intrep = d
    }
    if _, exists := d.Items[key]; !exists {
        d.Order = append(d.Order, key)
    }
    d.Items[key] = val
    o.Invalidate()
}
```

---

## The Interpreter

Holds state, manages C interop. Doesn't own value operations.

```go
type Interp struct {
    // C interop (internal)
    handles    map[Handle]*Obj
    reverse    map[*Obj]Handle
    nextHandle Handle
    
    // State
    globals  *Namespace
    frames   []*CallFrame
    result   *Obj
    commands map[string]*Command
}

type Handle uintptr

func (i *Interp) register(o *Obj) Handle { /* ... */ }
func (i *Interp) lookup(h Handle) *Obj   { /* ... */ }
```

---

## Adding New Types

1. Define your rep type
2. Implement `ObjType` (required)
3. Implement conversion interfaces (optional, only what makes sense)

```go
// RegexType caches a compiled regex
type RegexType struct {
    pattern string
    re      *regexp.Regexp
}

func (t *RegexType) Name() string         { return "regex" }
func (t *RegexType) Dup() ObjType         { return t }
func (t *RegexType) UpdateString() string { return t.pattern }

// No conversion interfaces — regex doesn't convert to int/list/etc.

// Conversion function
func AsRegex(o *Obj) (*regexp.Regexp, error) {
    if t, ok := o.intrep.(*RegexType); ok {
        return t.re, nil
    }
    re, err := regexp.Compile(o.String())
    if err != nil {
        return nil, err
    }
    o.intrep = &RegexType{pattern: o.String(), re: re}
    return re, nil
}
```

---

## Summary

| Concept | Role |
|---------|------|
| `*Obj` | The value — holds string + internal rep |
| `ObjType` | Core interface: Name, UpdateString, Dup |
| `IntoInt`, `IntoDouble`, ... | Optional interfaces for direct conversion |
| `AsInt()`, `AsList()`, ... | Free functions for shimmering |
| `Interp` | State + C handle map |

**Obj knows nothing about specific types. Types declare their own capabilities.**

---

## Usage

```go
// Create
s := feather.NewString("{a b c}")
n := feather.NewInt(42)

// Convert (shimmer)
list, _ := feather.AsList(s)
fmt.Println(len(list))          // 3

// Direct conversion (no string parsing)
d := feather.NewDouble(3.14)
i, _ := feather.AsInt(d)        // uses IntoInt, no string involved
fmt.Println(i)                  // 3

// Type that can't convert
foreign := feather.NewForeign("Conn", myConn)
_, err := feather.AsInt(foreign) // fails: no IntoInt, string "<Conn:0x...>" won't parse
```

---

## Integration

This design lives entirely within the Go host (`interp/` package). The C interpreter
continues to operate on opaque handles via `FeatherHostOps`.

**Changes required:**

- Replace internal value storage in `Interp` with `*Obj`
- Update `FeatherHostOps` callback implementations to use `AsInt()`, `AsList()`, etc.
- No changes to C code or builtins

**Invalidation behavior** (matches TCL): mutating operations like `lset` regenerate
the string representation. After `lset x 1 "modified"`, the string rep is recomputed
from the list elements.
