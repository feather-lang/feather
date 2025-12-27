// Package feather provides an embeddable TCL interpreter for Go applications.
//
// # Overview
//
// feather is a minimal, pure implementation of the core TCL language designed
// for embedding. It provides:
//
//   - A clean, idiomatic Go API
//   - Automatic type conversion between Go and TCL
//   - Foreign object support for exposing Go types to TCL
//   - No external dependencies beyond the Go standard library
//
// # Quick Start
//
//	import "github.com/feather-lang/feather"
//
//	func main() {
//	    interp := feather.New()
//	    defer interp.Close()
//
//	    // Evaluate TCL scripts
//	    result, _ := interp.Eval("expr {2 + 2}")
//	    fmt.Println(result.String()) // "4"
//
//	    // Set and get variables
//	    interp.SetVar("name", "World")
//	    result, _ = interp.Eval(`set greeting "Hello, $name!"`)
//
//	    // Register Go functions
//	    interp.Register("double", func(x int) int { return x * 2 })
//	    result, _ = interp.Eval("double 21") // "42"
//	}
//
// # Registering Go Functions
//
// The Register method accepts any Go function and automatically converts
// arguments and return values:
//
//	// Simple function
//	interp.Register("greet", func(name string) string {
//	    return "Hello, " + name + "!"
//	})
//
//	// Function with error return
//	interp.Register("divide", func(a, b int) (int, error) {
//	    if b == 0 {
//	        return 0, errors.New("division by zero")
//	    }
//	    return a / b, nil
//	})
//
//	// Variadic function
//	interp.Register("sum", func(nums ...int) int {
//	    total := 0
//	    for _, n := range nums {
//	        total += n
//	    }
//	    return total
//	})
//
// # Foreign Objects
//
// Expose Go types as TCL commands using RegisterType:
//
//	type Counter struct {
//	    value int
//	}
//
//	feather.RegisterType[*Counter](interp, "Counter", feather.TypeDef[*Counter]{
//	    New: func() *Counter { return &Counter{} },
//	    Methods: map[string]any{
//	        "get":  func(c *Counter) int { return c.value },
//	        "set":  func(c *Counter, v int) { c.value = v },
//	        "incr": func(c *Counter) int { c.value++; return c.value },
//	    },
//	})
//
//	// In TCL:
//	// set c [Counter new]
//	// $c set 10
//	// $c incr  ;# returns 11
//	// $c destroy
//
// # Custom Shimmering Types
//
// Implement the ObjType interface to create custom types that participate
// in TCL's shimmering (lazy type conversion) system:
//
//	type MyType struct {
//	    data string
//	}
//
//	func (t *MyType) Name() string { return "mytype" }
//	func (t *MyType) UpdateString(o *feather.Obj) { o.SetString(t.data) }
//	func (t *MyType) Dup() feather.ObjType { return &MyType{data: t.data} }
//
//	// Create an object with custom internal representation
//	obj := feather.NewObj(&MyType{data: "hello"})
//
// Custom types can also implement conversion interfaces (IntoInt, IntoDouble,
// IntoList, IntoDict, IntoBool) to participate in automatic type conversion.
//
// # Value Access
//
// The *Obj type provides type-safe access to TCL values:
//
//	result, _ := interp.Eval("list 1 2 3")
//
//	// Get as different types
//	str := feather.AsString(result)  // "1 2 3"
//	list, _ := result.List()         // []*Obj with 3 elements
//	for _, v := range list {
//	    n, _ := v.Int()              // 1, 2, 3
//	}
//
// # Supported Type Conversions
//
// Go to TCL:
//   - string → string
//   - int, int64 → integer
//   - float64 → double
//   - bool → "1" or "0"
//   - []T → list
//   - map[string]T → dict
//
// TCL to Go:
//   - string → string
//   - integer → int, int64
//   - double → float64
//   - list → []T
//   - "1"/"true"/"yes"/"on" → true
//   - "0"/"false"/"no"/"off" → false
package feather
