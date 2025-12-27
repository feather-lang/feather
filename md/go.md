---
outline: deep
---

# Go

Feather is written in Go and designed for easy embedding in Go applications.

## Installation

```bash
go get github.com/feather-lang/feather
```

## Quick Start

```go
package main

import (
    "fmt"
    "github.com/feather-lang/feather"
)

func main() {
    interp := feather.New()
    defer interp.Close()

    // Register a host command with automatic type conversion
    interp.Register("greet", func(name string) string {
        return "Hello, " + name + "!"
    })

    // Evaluate TCL code
    result, err := interp.Eval(`greet "Feather"`)
    if err != nil {
        panic(err)
    }
    fmt.Println(result) // Hello, Feather!
}
```

## Registering Commands

Use `Register` to add commands with automatic type conversion:

```go
// Simple function
interp.Register("greet", func(name string) string {
    return "Hello, " + name + "!"
})

// Multiple arguments with different types
interp.Register("add", func(a, b int) int {
    return a + b
})

// With error return
interp.Register("divide", func(a, b int) (int, error) {
    if b == 0 {
        return 0, errors.New("division by zero")
    }
    return a / b, nil
})

// Variadic arguments
interp.Register("sum", func(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
})
```

Supported parameter types: `string`, `int`, `int64`, `float64`, `bool`, `[]string`, and variadic versions.

### Low-Level Interface

For full control over argument handling, use `RegisterCommand`:

```go
interp.RegisterCommand("mysum", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
    if len(args) < 2 {
        return feather.Errorf("wrong # args: should be \"%s a b\"", cmd.String())
    }
    a, err := args[0].Int()
    if err != nil {
        return feather.Error(err.Error())
    }
    b, err := args[1].Int()
    if err != nil {
        return feather.Error(err.Error())
    }
    return feather.OK(a + b)
})
```

## Working with Values

Feather values (`*feather.Obj`) can be converted to Go types:

```go
// String (always available)
s := obj.String()

// Integer
i, err := obj.Int()

// Float
f, err := obj.Double()

// Boolean (TCL boolean rules)
b, err := obj.Bool()

// List (if already has list representation)
items, err := obj.List()

// Dict (shimmers to dict if needed)
dictObj, err := obj.Dict()
```

Create new values with constructor functions:

```go
feather.NewStringObj("hello")
feather.NewIntObj(42)
feather.NewDoubleObj(3.14)
feather.NewListObj(item1, item2, item3)
feather.NewDictObj()
```

Or use interpreter convenience methods:

```go
interp.String("hello")
interp.Int(42)
interp.Float(3.14)
interp.Dict()
interp.DictKV("name", "Alice", "age", 30)
interp.DictFrom(map[string]any{"name": "Alice", "age": 30})
```

## Foreign Types

Expose Go structs as TCL objects with methods:

```go
type Counter struct {
    value int64
}

interp.RegisterType("Counter", feather.TypeDef[*Counter]{
    New: func() *Counter {
        return &Counter{value: 0}
    },
    Methods: feather.Methods{
        "get": func(c *Counter) int64 {
            return c.value
        },
        "set": func(c *Counter, val int64) {
            c.value = val
        },
        "incr": func(c *Counter) int64 {
            c.value++
            return c.value
        },
    },
    String: func(c *Counter) string {
        return fmt.Sprintf("<Counter:%d>", c.value)
    },
})
```

Use from TCL:

```tcl
set c [Counter new]
$c set 10
$c incr
puts [$c get]  ;# 11
```

## Parsing Input

Check if input is complete before evaluating (useful for REPLs):

```go
result := interp.Parse(script)
switch result.Status {
case feather.ParseOK:
    // Script is complete, safe to eval
case feather.ParseIncomplete:
    // Unclosed braces/quotes, wait for more input
case feather.ParseError:
    // Syntax error
    fmt.Println(result.Message)
}
```

## API Reference

Full documentation is available on [pkg.go.dev](https://pkg.go.dev/github.com/feather-lang/feather).

## Example Project

See [feather-httpd](https://github.com/feather-lang/feather-httpd) for a complete example: an HTTP server scriptable via Feather with a telnet REPL for live inspection and modification.
