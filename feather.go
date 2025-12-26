// Package feather provides an embeddable TCL interpreter.
//
// feather is a pure, minimal implementation of TCL designed for embedding
// into Go applications. It provides a clean, idiomatic Go API while preserving
// TCL's powerful metaprogramming capabilities.
//
// # Quick Start
//
//	interp := feather.New()
//	defer interp.Close()
//
//	result, err := interp.Eval("set x 42; expr {$x * 2}")
//	if err != nil {
//	    log.Fatal(err)
//	}
//	fmt.Println(result.String()) // "84"
//
// # Registering Go Functions
//
// The [Interp.Register] method allows exposing Go functions to TCL with
// automatic argument conversion:
//
//	interp.Register("greet", func(name string) string {
//	    return "Hello, " + name + "!"
//	})
//	result, _ := interp.Eval(`greet World`)
//	// result.String() == "Hello, World!"
//
// Supported parameter types: string, int, int64, float64, bool, []string.
// Supported return types: string, int, int64, float64, bool, error, or (T, error).
//
// # Low-Level Command Registration
//
// For full control over argument handling, use [Interp.RegisterCommand]:
//
//	interp.RegisterCommand("sum", func(i *feather.Interp, cmd feather.Object, args []feather.Object) feather.Result {
//	    if len(args) < 2 {
//	        return feather.Errorf("wrong # args: should be \"%s a b\"", cmd.String())
//	    }
//	    a, _ := args[0].Int()
//	    b, _ := args[1].Int()
//	    return feather.OK(a + b)
//	})
//
// # Working with Values
//
// The [Object] type represents TCL values and supports shimmering (lazy type conversion):
//
//	// Create values
//	s := interp.String("hello")
//	n := interp.Int(42)
//	f := interp.Float(3.14)
//	b := interp.Bool(true)
//	list := interp.List(interp.String("a"), interp.Int(1))
//	dict := interp.DictKV("name", "Alice", "age", 30)
//
//	// Read values back
//	s.String()      // always succeeds
//	n.Int()         // (int64, error) - parses if needed
//	f.Float()       // (float64, error)
//	b.Bool()        // (bool, error) - TCL boolean rules
//	list.List()     // ([]Object, error)
//	dict.Dict()     // (map[string]Object, error)
//
// # Exposing Go Types
//
// Use [RegisterType] to expose Go structs as TCL objects:
//
//	feather.RegisterType[*MyService](interp, "Service", feather.TypeDef[*MyService]{
//	    New: func() *MyService { return NewMyService() },
//	    Methods: map[string]any{
//	        "doWork": (*MyService).DoWork,
//	    },
//	})
//	interp.Eval(`set svc [Service new]; $svc doWork`)
package feather

import (
	"fmt"
	"strings"

	"github.com/feather-lang/feather/interp"
)

// Object represents a TCL value.
//
// Object provides lazy access to value representations through shimmering.
// When you call [Object.Int] on a string object, it parses the string and
// caches the integer representation. Subsequent calls return the cached value.
//
// Objects are only valid while the owning interpreter is alive. Do not store
// Objects beyond the lifetime of their interpreter.
//
// The zero value is a nil object; most methods return zero/empty values for nil objects.
type Object struct {
	i *interp.Interp
	h interp.FeatherObj
}

// String returns the string representation of the object.
//
// This method always succeeds. For typed objects (int, list, dict), it generates
// the canonical TCL string representation.
//
//	interp.Int(42).String()           // "42"
//	interp.List(...).String()         // "a b c"
//	interp.DictKV("k", "v").String()  // "k v"
func (o Object) String() string {
	if o.i == nil {
		return ""
	}
	return o.i.GetString(o.h)
}

// Int returns the integer representation of the object.
//
// If the object is not already an integer, it attempts to parse the string
// representation. The parsed value is cached (shimmering).
//
// Returns an error if the value cannot be converted to an integer.
//
//	interp.String("42").Int()     // 42, nil
//	interp.String("hello").Int()  // 0, error
func (o Object) Int() (int64, error) {
	if o.i == nil {
		return 0, fmt.Errorf("nil object")
	}
	return o.i.GetInt(o.h)
}

// Float returns the floating-point representation of the object.
//
// If the object is not already a float, it attempts to parse the string
// representation. Integer objects are converted to float without parsing.
//
// Returns an error if the value cannot be converted to a float.
//
//	interp.String("3.14").Float()  // 3.14, nil
//	interp.Int(42).Float()         // 42.0, nil
func (o Object) Float() (float64, error) {
	if o.i == nil {
		return 0, fmt.Errorf("nil object")
	}
	return o.i.GetDouble(o.h)
}

// Bool returns the boolean representation of the object.
//
// TCL has specific rules for boolean values:
//   - Truthy: "1", "true", "yes", "on" (case-insensitive)
//   - Falsy: "0", "false", "no", "off" (case-insensitive)
//
// Returns an error for any other value.
//
//	interp.String("yes").Bool()   // true, nil
//	interp.String("0").Bool()     // false, nil
//	interp.String("maybe").Bool() // false, error
func (o Object) Bool() (bool, error) {
	if o.i == nil {
		return false, fmt.Errorf("nil object")
	}
	s := o.i.GetString(o.h)
	switch strings.ToLower(s) {
	case "1", "true", "yes", "on":
		return true, nil
	case "0", "false", "no", "off":
		return false, nil
	default:
		return false, fmt.Errorf("expected boolean but got %q", s)
	}
}

// List returns the list representation of the object.
//
// If the object is not already a list, it parses the string representation
// as a TCL list. The parsed value is cached (shimmering).
//
// Returns an error if the value cannot be parsed as a TCL list.
//
//	interp.String("a b c").List()  // [a, b, c], nil
//	interp.String("{a b} c").List() // [{a b}, c], nil
func (o Object) List() ([]Object, error) {
	if o.i == nil {
		return nil, fmt.Errorf("nil object")
	}
	items, err := o.i.GetList(o.h)
	if err != nil {
		return nil, err
	}
	result := make([]Object, len(items))
	for i, h := range items {
		result[i] = Object{o.i, h}
	}
	return result, nil
}

// Dict returns the dict representation of the object.
//
// If the object is not already a dict, it parses the string/list representation
// as key-value pairs. The parsed value is cached (shimmering).
//
// Returns an error if the value has an odd number of elements or cannot be
// parsed as a list.
//
//	interp.String("a 1 b 2").Dict()  // {"a": 1, "b": 2}, nil
//	interp.String("a b c").Dict()    // nil, error (odd elements)
func (o Object) Dict() (map[string]Object, error) {
	if o.i == nil {
		return nil, fmt.Errorf("nil object")
	}
	items, order, err := o.i.GetDict(o.h)
	if err != nil {
		return nil, err
	}
	result := make(map[string]Object, len(order))
	for _, k := range order {
		result[k] = Object{o.i, items[k]}
	}
	return result, nil
}

// Type returns the native type name of the object.
//
// Built-in types: "string", "int", "double", "list", "dict".
// Foreign types return their registered type name (e.g., "Counter").
//
//	interp.Int(42).Type()           // "int"
//	interp.String("hi").Type()      // "string"
//	counterObj.Type()               // "Counter"
func (o Object) Type() string {
	if o.i == nil {
		return "string"
	}
	return o.i.Type(o.h)
}

// IsNil returns true if this is a nil/invalid object.
//
// Nil objects occur when:
//   - The zero value Object{} is used
//   - A variable lookup fails
//   - An out-of-bounds list index is accessed
func (o Object) IsNil() bool {
	return o.h == 0 || o.i == nil
}

// StringList returns the list as a []string for convenience.
//
// This is equivalent to calling [Object.List] and then converting each element
// to a string, but more convenient when you know all elements are strings.
//
//	interp.String("a b c").StringList()  // ["a", "b", "c"], nil
func (o Object) StringList() ([]string, error) {
	items, err := o.List()
	if err != nil {
		return nil, err
	}
	result := make([]string, len(items))
	for i, item := range items {
		result[i] = item.String()
	}
	return result, nil
}

// Len returns the length of a list or dict.
//
// For lists, returns the number of elements.
// For dicts, returns the number of key-value pairs times 2 (list representation).
// For non-collections or errors, returns 0.
//
//	interp.List(a, b, c).Len()        // 3
//	interp.DictKV("k", "v").Len()     // 2 (as list: "k v")
//	interp.String("hello").Len()      // 1 (single-element list)
func (o Object) Len() int {
	if o.i == nil {
		return 0
	}
	return o.i.ListLen(o.h)
}

// Index returns the element at index i for lists.
//
// Uses zero-based indexing. Returns an empty string object if the index is
// out of bounds or the object cannot be converted to a list.
//
//	list := interp.List(interp.String("a"), interp.String("b"))
//	list.Index(0).String()  // "a"
//	list.Index(5).String()  // "" (out of bounds)
func (o Object) Index(i int) Object {
	if o.i == nil {
		return Object{}
	}
	h := o.i.ListIndex(o.h, i)
	if h == 0 {
		return Object{o.i, o.i.InternString("")}
	}
	return Object{o.i, h}
}

// Append adds an element to a list, mutating it in place.
//
// If the object is not a list, it is first converted to one.
// The string representation is invalidated and will be regenerated on next access.
//
//	list := interp.List(interp.Int(1), interp.Int(2))
//	list.Append(interp.Int(3))
//	list.String()  // "1 2 3"
func (o Object) Append(elem Object) {
	if o.i == nil || elem.i == nil {
		return
	}
	o.i.ListAppend(o.h, elem.h)
}

// Get returns the value for a dict key.
//
// Returns (value, true) if the key exists, (Object{}, false) if not.
// If the object cannot be converted to a dict, returns (Object{}, false).
//
//	dict := interp.DictKV("name", "Alice")
//	v, ok := dict.Get("name")   // "Alice", true
//	v, ok = dict.Get("missing") // Object{}, false
func (o Object) Get(key string) (Object, bool) {
	if o.i == nil {
		return Object{}, false
	}
	h, ok := o.i.DictGet(o.h, key)
	if !ok {
		return Object{}, false
	}
	return Object{o.i, h}, true
}

// Set sets a dict key to a value, mutating the dict in place.
//
// If the key already exists, its value is updated. If not, the key is added.
// The string representation is invalidated and will be regenerated on next access.
//
//	dict := interp.Dict()
//	dict.Set("name", interp.String("Alice"))
//	dict.Set("age", interp.Int(30))
func (o Object) Set(key string, val Object) {
	if o.i == nil || val.i == nil {
		return
	}
	o.i.DictSet(o.h, key, val.h)
}

// Keys returns the keys of a dict in insertion order.
//
// Returns nil if the object cannot be converted to a dict or is nil.
//
//	dict := interp.DictKV("b", 2, "a", 1)
//	dict.Keys()  // ["b", "a"] (insertion order preserved)
func (o Object) Keys() []string {
	if o.i == nil {
		return nil
	}
	return o.i.DictKeys(o.h)
}

// Interp is a TCL interpreter instance.
//
// Create a new interpreter with [New] and always call [Interp.Close] when done.
// An interpreter is not safe for concurrent use from multiple goroutines.
//
//	interp := feather.New()
//	defer interp.Close()
//	result, err := interp.Eval("expr 2 + 2")
type Interp struct {
	i *interp.Interp
}

// New creates a new TCL interpreter with all standard commands registered.
//
// The interpreter must be closed with [Interp.Close] when no longer needed
// to release resources.
//
//	interp := feather.New()
//	defer interp.Close()
func New() *Interp {
	return &Interp{
		i: interp.NewInterp(),
	}
}

// Close releases resources associated with the interpreter.
//
// After Close is called, the interpreter and all Objects created from it
// become invalid. Always use defer to ensure Close is called.
func (i *Interp) Close() {
	i.i.Close()
}

// -----------------------------------------------------------------------------
// Object Creation
// -----------------------------------------------------------------------------

// String creates a string Object.
//
//	s := interp.String("hello world")
//	s.Type()   // "string"
//	s.String() // "hello world"
func (i *Interp) String(s string) Object {
	return Object{i.i, i.i.InternString(s)}
}

// Int creates an integer Object.
//
//	n := interp.Int(42)
//	n.Type()   // "int"
//	n.String() // "42"
//	n.Int()    // 42, nil
func (i *Interp) Int(v int64) Object {
	return Object{i.i, i.i.NewInt(v)}
}

// Float creates a floating-point Object.
//
//	f := interp.Float(3.14)
//	f.Type()   // "double"
//	f.String() // "3.14"
//	f.Float()  // 3.14, nil
func (i *Interp) Float(v float64) Object {
	return Object{i.i, i.i.NewDouble(v)}
}

// Bool creates a boolean Object, stored as int 1 (true) or 0 (false).
//
// TCL has no native boolean type; booleans are represented as integers.
//
//	b := interp.Bool(true)
//	b.Type()   // "int"
//	b.String() // "1"
//	b.Bool()   // true, nil
func (i *Interp) Bool(v bool) Object {
	if v {
		return Object{i.i, i.i.NewInt(1)}
	}
	return Object{i.i, i.i.NewInt(0)}
}

// List creates a list Object from the given items.
//
//	list := interp.List(interp.String("a"), interp.Int(1), interp.Bool(true))
//	list.Type()   // "list"
//	list.String() // "a 1 1"
//	list.Len()    // 3
func (i *Interp) List(items ...Object) Object {
	l := i.i.NewList()
	for _, item := range items {
		i.i.ListAppend(l, item.h)
	}
	return Object{i.i, l}
}

// ListFrom creates a list Object from a Go slice.
//
// Supported slice types:
//   - []string  - each element becomes a string object
//   - []int     - each element becomes an int object
//   - []int64   - each element becomes an int object
//   - []float64 - each element becomes a double object
//   - []any     - each element is auto-converted based on its type
//
// Example:
//
//	list := interp.ListFrom([]string{"a", "b", "c"})
//	list.String() // "a b c"
//
//	nums := interp.ListFrom([]int{1, 2, 3})
//	nums.String() // "1 2 3"
func (i *Interp) ListFrom(slice any) Object {
	l := i.i.NewList()
	switch s := slice.(type) {
	case []string:
		for _, v := range s {
			i.i.ListAppend(l, i.i.InternString(v))
		}
	case []int:
		for _, v := range s {
			i.i.ListAppend(l, i.i.NewInt(int64(v)))
		}
	case []int64:
		for _, v := range s {
			i.i.ListAppend(l, i.i.NewInt(v))
		}
	case []float64:
		for _, v := range s {
			i.i.ListAppend(l, i.i.NewDouble(v))
		}
	case []any:
		for _, v := range s {
			i.i.ListAppend(l, i.anyToHandle(v))
		}
	}
	return Object{i.i, l}
}

// Dict creates an empty dict Object.
//
// Use [Object.Set] to add key-value pairs, or use [Interp.DictKV] or
// [Interp.DictFrom] to create a populated dict.
//
//	dict := interp.Dict()
//	dict.Set("key", interp.String("value"))
func (i *Interp) Dict() Object {
	return Object{i.i, i.i.NewDict()}
}

// DictKV creates a dict Object from alternating key-value pairs.
//
// Keys should be strings (non-strings are converted via fmt.Sprintf).
// Values are auto-converted based on their Go type.
//
//	dict := interp.DictKV("name", "Alice", "age", 30, "active", true)
//	dict.String() // "name Alice age 30 active 1"
//
//	v, _ := dict.Get("name")
//	v.String() // "Alice"
func (i *Interp) DictKV(kvs ...any) Object {
	d := i.i.NewDict()
	for j := 0; j+1 < len(kvs); j += 2 {
		key, ok := kvs[j].(string)
		if !ok {
			key = fmt.Sprintf("%v", kvs[j])
		}
		d = i.i.DictSet(d, key, i.anyToHandle(kvs[j+1]))
	}
	return Object{i.i, d}
}

// DictFrom creates a dict Object from a Go map.
//
// Values are auto-converted based on their Go type.
// Note: Go maps have undefined iteration order, so dict key order may vary.
//
//	dict := interp.DictFrom(map[string]any{
//	    "name": "Alice",
//	    "age":  30,
//	})
func (i *Interp) DictFrom(m map[string]any) Object {
	d := i.i.NewDict()
	for k, v := range m {
		d = i.i.DictSet(d, k, i.anyToHandle(v))
	}
	return Object{i.i, d}
}

// anyToHandle converts any Go value to an internal object handle.
// Used internally for auto-conversion in SetVar, DictKV, etc.
func (i *Interp) anyToHandle(v any) interp.FeatherObj {
	switch val := v.(type) {
	case string:
		return i.i.InternString(val)
	case int:
		return i.i.NewInt(int64(val))
	case int64:
		return i.i.NewInt(val)
	case float64:
		return i.i.NewDouble(val)
	case bool:
		if val {
			return i.i.NewInt(1)
		}
		return i.i.NewInt(0)
	case Object:
		return val.h
	default:
		return i.i.InternString(fmt.Sprintf("%v", v))
	}
}

// -----------------------------------------------------------------------------
// Script Evaluation
// -----------------------------------------------------------------------------

// Eval evaluates a TCL script and returns the result.
//
// Multiple commands can be separated by semicolons or newlines.
// Returns an error if the script has a syntax error or a command fails.
//
//	result, err := interp.Eval("set x 10; expr {$x * 2}")
//	if err != nil {
//	    log.Fatal(err)
//	}
//	fmt.Println(result.String()) // "20"
func (i *Interp) Eval(script string) (Object, error) {
	_, err := i.i.Eval(script)
	if err != nil {
		return Object{}, err
	}
	return Object{i.i, i.i.ResultHandle()}, nil
}

// Call invokes a single TCL command with the given arguments.
//
// Arguments are automatically converted from Go types to TCL values.
// This is a convenience wrapper around [Interp.Eval] for single command invocation.
//
//	result, err := interp.Call("expr", "2 + 2")
//	result, err := interp.Call("llength", myList)
//	result, err := interp.Call("myns::proc", arg1, arg2)
func (i *Interp) Call(cmd string, args ...any) (Object, error) {
	script := cmd
	for _, arg := range args {
		script += " " + toTclString(arg)
	}
	return i.Eval(script)
}

// -----------------------------------------------------------------------------
// Variables
// -----------------------------------------------------------------------------

// Var returns the value of a variable as an Object.
//
// Returns an empty string object if the variable does not exist.
// The returned object preserves the variable's type (int, list, foreign, etc.).
//
//	interp.SetVar("x", 42)
//	v := interp.Var("x")
//	v.Int()  // 42, nil
//	v.Type() // "int" (if SetVar preserved type) or "string"
func (i *Interp) Var(name string) Object {
	h := i.i.GetVarHandle(name)
	if h == 0 {
		// Variable not found, return empty string object
		return Object{i.i, i.i.InternString("")}
	}
	return Object{i.i, h}
}

// SetVar sets a variable to a value.
//
// The value is automatically converted from Go types to TCL:
//   - string, int, int64, float64, bool are converted directly
//   - []string becomes a TCL list
//   - Other types use fmt.Sprintf("%v", val)
//
//	interp.SetVar("name", "Alice")
//	interp.SetVar("count", 42)
//	interp.SetVar("items", []string{"a", "b", "c"})
func (i *Interp) SetVar(name string, val any) {
	i.i.SetVar(name, toTclString(val))
}

// SetVars sets multiple variables at once from a map.
//
// This is a convenience method equivalent to calling [Interp.SetVar] for each entry.
//
//	interp.SetVars(map[string]any{
//	    "x": 1,
//	    "y": 2,
//	    "name": "Alice",
//	})
func (i *Interp) SetVars(vars map[string]any) {
	for name, val := range vars {
		i.i.SetVar(name, toTclString(val))
	}
}

// GetVars returns multiple variables as a map.
//
// This is a convenience method equivalent to calling [Interp.Var] for each name.
// Variables that don't exist will have empty string values in the result.
//
//	vars := interp.GetVars("x", "y", "z")
//	fmt.Println(vars["x"].Int())
func (i *Interp) GetVars(names ...string) map[string]Object {
	result := make(map[string]Object, len(names))
	for _, name := range names {
		result[name] = i.Var(name)
	}
	return result
}

// -----------------------------------------------------------------------------
// Command Registration
// -----------------------------------------------------------------------------

// CommandFunc is the signature for custom commands registered with [Interp.RegisterCommand].
//
// The function receives:
//   - i: the interpreter (for creating objects, accessing variables, etc.)
//   - cmd: the command name as invoked
//   - args: the arguments passed to the command
//
// Return [OK] for success or [Error]/[Errorf] for failure.
type CommandFunc func(i *Interp, cmd Object, args []Object) Result

// RegisterCommand adds a command using the low-level CommandFunc interface.
//
// Use this when you need full control over argument handling, access to the
// interpreter, or custom error messages. For simpler cases, use [Interp.Register].
//
//	interp.RegisterCommand("sum", func(i *feather.Interp, cmd feather.Object, args []feather.Object) feather.Result {
//	    if len(args) < 2 {
//	        return feather.Errorf("wrong # args: should be \"%s a b\"", cmd.String())
//	    }
//	    a, err := args[0].Int()
//	    if err != nil {
//	        return feather.Error(err.Error())
//	    }
//	    b, err := args[1].Int()
//	    if err != nil {
//	        return feather.Error(err.Error())
//	    }
//	    return feather.OK(a + b)
//	})
func (i *Interp) RegisterCommand(name string, fn CommandFunc) {
	i.i.Register(name, func(ii *interp.Interp, cmd interp.FeatherObj, args []interp.FeatherObj) interp.FeatherResult {
		objArgs := make([]Object, len(args))
		for j, h := range args {
			objArgs[j] = Object{ii, h}
		}
		r := fn(i, Object{ii, cmd}, objArgs)
		if r.code == interp.ResultError {
			ii.SetErrorString(r.val)
		} else {
			ii.SetResultString(r.val)
		}
		return r.code
	})
}

// Register adds a command with automatic argument conversion.
//
// The function's signature determines how arguments are converted:
//   - string parameters receive the string representation
//   - int/int64 parameters parse the argument as an integer
//   - float64 parameters parse as a floating-point number
//   - bool parameters use TCL boolean rules
//   - []string parameters receive remaining args as a list
//   - Variadic parameters (...string, ...int) consume remaining arguments
//
// Return types are also auto-converted:
//   - string, int, int64, float64, bool become the command result
//   - error causes the command to fail with the error message
//   - (T, error) returns T on success or fails on error
//
// Examples:
//
//	// Simple function
//	interp.Register("greet", func(name string) string {
//	    return "Hello, " + name
//	})
//
//	// With error handling
//	interp.Register("divide", func(a, b int) (int, error) {
//	    if b == 0 {
//	        return 0, errors.New("division by zero")
//	    }
//	    return a / b, nil
//	})
//
//	// Variadic
//	interp.Register("join", func(sep string, parts ...string) string {
//	    return strings.Join(parts, sep)
//	})
func (i *Interp) Register(name string, fn any) {
	wrapper := wrapFunc(i, fn)
	i.i.Register(name, wrapper)
}

// SetUnknownHandler sets a handler called when a command is not found.
//
// The handler receives the unknown command name and its arguments. It can:
//   - Implement the command dynamically
//   - Delegate to another system
//   - Return an error for truly unknown commands
//
// Set to nil to restore default behavior (return "invalid command" error).
//
//	interp.SetUnknownHandler(func(i *feather.Interp, cmd feather.Object, args []feather.Object) feather.Result {
//	    // Try to auto-load the command
//	    if loaded := tryLoadCommand(cmd.String()); loaded {
//	        return i.Call(cmd.String(), args...)
//	    }
//	    return feather.Errorf("unknown command: %s", cmd.String())
//	})
func (i *Interp) SetUnknownHandler(fn CommandFunc) {
	i.i.SetUnknownHandler(func(ii *interp.Interp, cmd interp.FeatherObj, args []interp.FeatherObj) interp.FeatherResult {
		objArgs := make([]Object, len(args))
		for j, h := range args {
			objArgs[j] = Object{ii, h}
		}
		r := fn(i, Object{ii, cmd}, objArgs)
		if r.code == interp.ResultError {
			ii.SetErrorString(r.val)
		} else {
			ii.SetResultString(r.val)
		}
		return r.code
	})
}

// -----------------------------------------------------------------------------
// Parsing
// -----------------------------------------------------------------------------

// Parse checks if a script is syntactically complete.
//
// This is useful for implementing REPLs that need to detect incomplete input
// (unclosed braces, brackets, or quotes).
//
//	pr := interp.Parse("set x {")
//	if pr.Status == feather.ParseIncomplete {
//	    // Prompt for more input
//	}
func (i *Interp) Parse(script string) ParseResult {
	pr := i.i.Parse(script)
	return ParseResult{
		Status:  ParseStatus(pr.Status),
		Message: pr.ErrorMessage,
	}
}

// Internal returns the underlying interp.Interp for advanced usage.
//
// This is an escape hatch for accessing features not yet exposed in the
// public API. Use with caution; the internal API may change.
func (i *Interp) Internal() *interp.Interp {
	return i.i
}

// -----------------------------------------------------------------------------
// Command Results
// -----------------------------------------------------------------------------

// Result represents the result of a command execution.
//
// Create results using [OK], [Error], or [Errorf].
type Result struct {
	code interp.FeatherResult
	val  string
}

// OK returns a successful result with a value.
//
// The value is auto-converted to a TCL string representation.
//
//	return feather.OK("success")
//	return feather.OK(42)
//	return feather.OK([]string{"a", "b"})
func OK(v any) Result {
	return Result{code: interp.ResultOK, val: toTclString(v)}
}

// Error returns an error result with a message.
//
//	return feather.Error("something went wrong")
func Error(msg string) Result {
	return Result{code: interp.ResultError, val: msg}
}

// Errorf returns a formatted error result.
//
//	return feather.Errorf("expected %d args, got %d", want, got)
func Errorf(format string, args ...any) Result {
	return Result{code: interp.ResultError, val: fmt.Sprintf(format, args...)}
}

// -----------------------------------------------------------------------------
// Parse Status
// -----------------------------------------------------------------------------

// ParseStatus indicates the result of parsing a script.
type ParseStatus int

const (
	// ParseOK indicates the script is syntactically complete and valid.
	ParseOK ParseStatus = ParseStatus(interp.ParseOK)

	// ParseIncomplete indicates the script has unclosed braces, brackets, or quotes.
	ParseIncomplete ParseStatus = ParseStatus(interp.ParseIncomplete)

	// ParseError indicates a syntax error in the script.
	ParseError ParseStatus = ParseStatus(interp.ParseError)
)

// ParseResult holds the result of parsing a script.
type ParseResult struct {
	// Status indicates whether parsing succeeded, found incomplete input, or failed.
	Status ParseStatus

	// Message contains an error message if Status is ParseError.
	Message string
}

// -----------------------------------------------------------------------------
// Foreign Types
// -----------------------------------------------------------------------------

// TypeDef defines a foreign type that can be exposed to TCL.
//
// Foreign types allow Go structs to be used as TCL objects with methods.
// See [RegisterType] for usage.
type TypeDef[T any] struct {
	// New is the constructor function, called when "TypeName new" is evaluated.
	// Required.
	New func() T

	// Methods maps method names to Go functions.
	// Each function's first parameter must be the receiver type T.
	// Additional parameters and return values are auto-converted.
	Methods map[string]any

	// String optionally provides a custom string representation.
	// If nil, a default "<TypeName:address>" format is used.
	String func(T) string

	// Destroy is called when the object is garbage collected or explicitly destroyed.
	// Use for cleanup (closing files, connections, etc.).
	Destroy func(T)
}

// RegisterType registers a foreign type with the interpreter.
//
// After registration, the type name becomes a command that supports "new"
// to create instances. Instances can then call methods using $obj method args.
//
// Example:
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
func RegisterType[T any](fi *Interp, name string, def TypeDef[T]) error {
	return interp.DefineType[T](fi.i, name, interp.ForeignTypeDef[T]{
		New:       def.New,
		Methods:   interp.Methods(def.Methods),
		StringRep: def.String,
		Destroy:   def.Destroy,
	})
}

// Register is deprecated; use [RegisterType] instead.
//
// Deprecated: This function was renamed for clarity.
func Register[T any](fi *Interp, name string, def TypeDef[T]) error {
	return RegisterType[T](fi, name, def)
}
