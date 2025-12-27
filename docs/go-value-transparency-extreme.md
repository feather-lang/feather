# Go Value Transparency: Extreme Edition

## The Most Extreme Version

**No conversion functions. No `*Obj` in user code. Just pure Go values.**

## Core Insight

The registry doesn't store `*Obj` - it stores `any`. The Go host is just a registry of Go values with automatic marshaling.

```go
type Registry struct {
    objects map[Handle]any  // Store raw Go values
    nextHandle Handle
}
```

## User API

```go
// Register a command with pure Go signature
// No *Interp, no *Obj, no error handling - just the logic
interp.Register("add", func(a, b int) int {
    return a + b
})

interp.Register("greet", func(name string, times int) string {
    return strings.Repeat("Hello "+name+"! ", times)
})

interp.Register("connect", func(host string, port int) *sql.DB {
    db, _ := sql.Open("postgres", fmt.Sprintf("host=%s port=%d", host, port))
    return db
})

interp.Register("query", func(db *sql.DB, sql string) []map[string]any {
    // Pure Go code
    rows, _ := db.Query(sql)
    defer rows.Close()
    return scanRows(rows)
})
```

From TCL:
```tcl
set sum [add 5 3]                           ;# 8
set msg [greet "Alice" 3]                   ;# "Hello Alice! Hello Alice! Hello Alice! "
set db [connect "localhost" 5432]           ;# Returns opaque handle
set results [query $db "SELECT * FROM users"] ;# Returns list of dicts
```

## How It Works

### 1. Everything is `any`

```go
//export feather_string_intern
func feather_string_intern(interp Handle, s *C.char, len C.size_t) Handle {
    str := C.GoStringN(s, C.int(len))
    return registry.Add(str)  // Store string directly as any
}

//export feather_int_create
func feather_int_create(interp Handle, value C.int64_t) Handle {
    return registry.Add(int64(value))  // Store int64 directly
}

//export feather_list_create
func feather_list_create(interp Handle) Handle {
    return registry.Add([]any{})  // Store slice of any
}
```

### 2. Automatic Marshaling via Reflection

```go
func (i *Interp) Register(name string, fn any) {
    fnType := reflect.TypeOf(fn)
    fnValue := reflect.ValueOf(fn)

    // Build the command wrapper
    wrapper := func(interpHandle Handle, cmdHandle Handle, argsHandles []Handle) Handle {
        // Convert handles to Go values matching expected types
        args := make([]reflect.Value, fnType.NumIn())

        for i := 0; i < fnType.NumIn(); i++ {
            expectedType := fnType.In(i)
            argValue := registry.Get(argsHandles[i])  // Get as any

            // Convert to expected type
            args[i] = convertToType(argValue, expectedType)
        }

        // Call the function
        results := fnValue.Call(args)

        // Store result and return handle
        if len(results) == 0 {
            return 0  // Void
        }

        resultValue := results[0].Interface()
        return registry.Add(resultValue)
    }

    i.commands[name] = wrapper
}
```

### 3. On-Demand Type Conversion

```go
func convertToType(value any, targetType reflect.Type) reflect.Value {
    // If types match, done
    if reflect.TypeOf(value) == targetType {
        return reflect.ValueOf(value)
    }

    // Convert based on target type
    switch targetType.Kind() {
    case reflect.String:
        return reflect.ValueOf(fmt.Sprint(value))

    case reflect.Int, reflect.Int64:
        return reflect.ValueOf(toInt(value))

    case reflect.Slice:
        return convertToSlice(value, targetType)

    case reflect.Map:
        return convertToMap(value, targetType)

    case reflect.Ptr, reflect.Interface:
        // Direct type assertion for custom types
        return reflect.ValueOf(value)

    default:
        panic(fmt.Sprintf("cannot convert %T to %v", value, targetType))
    }
}

func toInt(value any) int64 {
    switch v := value.(type) {
    case int64:
        return v
    case int:
        return int64(v)
    case string:
        i, _ := strconv.ParseInt(v, 10, 64)
        return i
    case float64:
        return int64(v)
    default:
        return 0
    }
}
```

### 4. Shimmering is Implicit

When a value is requested as a different type, conversion happens automatically:

```go
// Store as string
h := registry.Add("42")

// Request as int - converts on the fly
val := registry.Get(h)  // "42" (any)
intVal := toInt(val)    // 42 (int64)

// No explicit shimmer, just conversion
```

## No `*Obj` Type at All

The `*Obj` type disappears entirely. There's just:

1. **Handles** (`uint32`) - opaque references used by C code
2. **`any`** - Go values stored in the registry
3. **Reflection** - automatic marshaling

```go
// The entire "value system"
type Handle uint32

var registry = struct {
    objects map[Handle]any
    nextHandle Handle
}{
    objects: make(map[Handle]any),
    nextHandle: 1,
}

func (r *Registry) Add(value any) Handle {
    h := r.nextHandle
    r.nextHandle++
    r.objects[h] = value
    return h
}

func (r *Registry) Get(h Handle) any {
    return r.objects[h]
}
```

## What About String Representation?

Every Go value can be converted to a string (TCL's fundamental type):

```go
func toString(value any) string {
    switch v := value.(type) {
    case string:
        return v
    case int, int64, int32:
        return fmt.Sprint(v)
    case float64:
        return fmt.Sprint(v)
    case []any:
        return formatList(v)
    case map[string]any:
        return formatDict(v)
    default:
        // Opaque types return handle representation
        return fmt.Sprintf("<%T:%p>", v, v)
    }
}
```

## What About Lists?

Lists are just `[]any`:

```go
//export feather_list_create
func feather_list_create(interp Handle) Handle {
    return registry.Add([]any{})
}

//export feather_list_append
func feather_list_append(interp Handle, list Handle, item Handle) {
    listVal := registry.Get(list).([]any)
    itemVal := registry.Get(item)
    listVal = append(listVal, itemVal)
    registry.objects[list] = listVal  // Update
}

//export feather_list_at
func feather_list_at(interp Handle, list Handle, index C.int) Handle {
    listVal := registry.Get(list).([]any)
    return registry.Add(listVal[int(index)])
}
```

## What About Dicts?

Dicts are just `map[string]any`:

```go
//export feather_dict_create
func feather_dict_create(interp Handle) Handle {
    return registry.Add(make(map[string]any))
}

//export feather_dict_set
func feather_dict_set(interp Handle, dict Handle, key Handle, val Handle) {
    dictVal := registry.Get(dict).(map[string]any)
    keyStr := toString(registry.Get(key))
    itemVal := registry.Get(val)
    dictVal[keyStr] = itemVal
}
```

## The Complete Example

```go
package main

import "github.com/feather-lang/feather"

type User struct {
    Name string
    Age  int
}

func main() {
    interp := feather.New()

    // Register commands with pure Go signatures
    interp.Register("create-user", func(name string, age int) *User {
        return &User{Name: name, Age: age}
    })

    interp.Register("get-name", func(u *User) string {
        return u.Name
    })

    interp.Register("get-age", func(u *User) int {
        return u.Age
    })

    interp.Register("birthday", func(u *User) {
        u.Age++
    })

    // Eval TCL code
    interp.Eval(`
        set user [create-user "Alice" 30]
        puts [get-name $user]    ;# Alice
        puts [get-age $user]     ;# 30
        birthday $user
        puts [get-age $user]     ;# 31
    `)
}
```

**Zero conversion functions. Zero `*Obj` manipulation. Just Go.**

## Benefits

1. **Completely invisible barrier**: Go code is just Go code
2. **No learning curve**: If you know Go, you know the API
3. **Type safety**: Function signatures are checked at compile time
4. **No boilerplate**: No conversion, no error handling
5. **Automatic**: Everything is inferred via reflection

## Trade-offs

1. **Reflection overhead**: Every command call uses reflection
2. **Runtime errors**: Type mismatches caught at runtime, not compile time
3. **No explicit control**: Can't optimize conversions
4. **Magic**: Harder to debug when conversions fail
5. **No shimmering optimization**: Every conversion happens fresh

## The Key Question

**Is the simplicity worth losing explicit control?**

For embedding TCL in a Go application, probably yes.
For a high-performance interpreter, probably no.

## Implementation Complexity

Surprisingly low:

- `registry.go` - Handle registry storing `any` (50 lines)
- `marshal.go` - Reflection-based command wrapper (100 lines)
- `convert.go` - Type conversion helpers (150 lines)
- `callbacks.go` - C export functions (200 lines)

**Total: ~500 lines to make the barrier invisible.**

## Alternative: Hybrid Approach

Keep `*Obj` internal but provide both APIs:

```go
// Simple API - pure Go, uses reflection
interp.Register("add", func(a, b int) int {
    return a + b
})

// Advanced API - manual marshaling, no reflection
interp.RegisterCommand("add", func(i *Interp, cmd *Obj, args []*Obj) Result {
    a, _ := AsInt(args[0])
    b, _ := AsInt(args[1])
    return OK(NewInt(a + b))
})
```

Use the simple API by default, drop to advanced when needed for performance.
