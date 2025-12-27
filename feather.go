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
//	interp.RegisterCommand("sum", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
//	    if len(args) < 2 {
//	        return feather.Errorf("wrong # args: should be \"%s a b\"", cmd.String())
//	    }
//	    a, _ := feather.AsInt(args[0])
//	    b, _ := feather.AsInt(args[1])
//	    return feather.OK(a + b)
//	})
//
// # Working with Values
//
// The [*Obj] type represents TCL values and supports shimmering (lazy type conversion):
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
//	s.String()            // always succeeds
//	feather.AsInt(n)      // (int64, error) - parses if needed
//	feather.AsDouble(f)   // (float64, error)
//	feather.AsBool(b)     // (bool, error) - TCL boolean rules
//	feather.AsList(list)  // ([]*Obj, error)
//	feather.AsDict(dict)  // (*DictType, error)
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
)

// Interp is a TCL interpreter instance.
//
// Create a new interpreter with [New] and always call [Interp.Close] when done.
// An interpreter is not safe for concurrent use from multiple goroutines.
//
//	interp := feather.New()
//	defer interp.Close()
//	result, err := interp.Eval("expr 2 + 2")
type Interp struct {
	i *InternalInterp
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
		i: NewInternalInterp(),
	}
}

// Close releases resources associated with the interpreter.
//
// After Close is called, the interpreter and all *Obj values created from it
// become invalid. Always use defer to ensure Close is called.
func (i *Interp) Close() {
	i.i.Close()
}

// -----------------------------------------------------------------------------
// Object Creation
// -----------------------------------------------------------------------------

// String creates a string object.
//
//	s := interp.String("hello world")
//	s.Type()   // "string"
//	s.String() // "hello world"
func (i *Interp) String(s string) *Obj {
	return NewStringObj(s)
}

// Int creates an integer object.
//
//	n := interp.Int(42)
//	n.Type()   // "int"
//	n.String() // "42"
func (i *Interp) Int(v int64) *Obj {
	return NewIntObj(v)
}

// Float creates a floating-point object.
//
//	f := interp.Float(3.14)
//	f.Type()   // "double"
//	f.String() // "3.14"
func (i *Interp) Float(v float64) *Obj {
	return NewDoubleObj(v)
}

// Bool creates a boolean object, stored as int 1 (true) or 0 (false).
//
// TCL has no native boolean type; booleans are represented as integers.
//
//	b := interp.Bool(true)
//	b.Type()   // "int"
//	b.String() // "1"
func (i *Interp) Bool(v bool) *Obj {
	if v {
		return NewIntObj(1)
	}
	return NewIntObj(0)
}

// List creates a list object from the given items.
//
//	list := interp.List(interp.String("a"), interp.Int(1), interp.Bool(true))
//	list.Type()   // "list"
//	list.String() // "a 1 1"
func (i *Interp) List(items ...*Obj) *Obj {
	return NewListObj(items...)
}

// ListFrom creates a list object from a Go slice.
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
func (i *Interp) ListFrom(slice any) *Obj {
	var items []*Obj
	switch s := slice.(type) {
	case []string:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = NewStringObj(v)
		}
	case []int:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = NewIntObj(int64(v))
		}
	case []int64:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = NewIntObj(v)
		}
	case []float64:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = NewDoubleObj(v)
		}
	case []any:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = i.anyToObj(v)
		}
	}
	return NewListObj(items...)
}

// Dict creates an empty dict object.
//
// Use dict helper functions to add key-value pairs, or use [Interp.DictKV] or
// [Interp.DictFrom] to create a populated dict.
//
//	dict := interp.Dict()
//	feather.ObjDictSet(dict, "key", interp.String("value"))
func (i *Interp) Dict() *Obj {
	return NewDictObj()
}

// DictKV creates a dict object from alternating key-value pairs.
//
// Keys should be strings (non-strings are converted via fmt.Sprintf).
// Values are auto-converted based on their Go type.
//
//	dict := interp.DictKV("name", "Alice", "age", 30, "active", true)
//	dict.String() // "name Alice age 30 active 1"
func (i *Interp) DictKV(kvs ...any) *Obj {
	d := NewDictObj()
	for j := 0; j+1 < len(kvs); j += 2 {
		key, ok := kvs[j].(string)
		if !ok {
			key = fmt.Sprintf("%v", kvs[j])
		}
		ObjDictSet(d, key, i.anyToObj(kvs[j+1]))
	}
	return d
}

// DictFrom creates a dict object from a Go map.
//
// Values are auto-converted based on their Go type.
// Note: Go maps have undefined iteration order, so dict key order may vary.
//
//	dict := interp.DictFrom(map[string]any{
//	    "name": "Alice",
//	    "age":  30,
//	})
func (i *Interp) DictFrom(m map[string]any) *Obj {
	d := NewDictObj()
	for k, v := range m {
		ObjDictSet(d, k, i.anyToObj(v))
	}
	return d
}

// anyToObj converts any Go value to a *Obj.
// Used internally for auto-conversion in SetVar, DictKV, etc.
func (i *Interp) anyToObj(v any) *Obj {
	switch val := v.(type) {
	case string:
		return NewStringObj(val)
	case int:
		return NewIntObj(int64(val))
	case int64:
		return NewIntObj(val)
	case float64:
		return NewDoubleObj(val)
	case bool:
		if val {
			return NewIntObj(1)
		}
		return NewIntObj(0)
	case *Obj:
		return val
	default:
		return NewStringObj(fmt.Sprintf("%v", v))
	}
}

// anyToHandle converts any Go value to an internal object handle.
// Used internally for auto-conversion in SetVar, etc.
func (i *Interp) anyToHandle(v any) FeatherObj {
	return i.i.handleForObj(i.anyToObj(v))
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
func (i *Interp) Eval(script string) (*Obj, error) {
	_, err := i.i.Eval(script)
	if err != nil {
		return nil, err
	}
	return i.i.objForHandle(i.i.ResultHandle()), nil
}

// Call invokes a single TCL command with the given arguments.
//
// Arguments are automatically converted from Go types to TCL values.
// This is a convenience wrapper around [Interp.Eval] for single command invocation.
//
//	result, err := interp.Call("expr", "2 + 2")
//	result, err := interp.Call("llength", myList)
//	result, err := interp.Call("myns::proc", arg1, arg2)
func (i *Interp) Call(cmd string, args ...any) (*Obj, error) {
	script := cmd
	for _, arg := range args {
		script += " " + toTclString(arg)
	}
	return i.Eval(script)
}

// -----------------------------------------------------------------------------
// Variables
// -----------------------------------------------------------------------------

// Var returns the value of a variable as a *Obj.
//
// Returns an empty string object if the variable does not exist.
// The returned object preserves the variable's type (int, list, foreign, etc.).
//
//	interp.SetVar("x", 42)
//	v := interp.Var("x")
//	feather.AsInt(v)  // 42, nil
//	v.Type()          // "int" (if SetVar preserved type) or "string"
func (i *Interp) Var(name string) *Obj {
	h := i.i.GetVarHandle(name)
	if h == 0 {
		return NewStringObj("")
	}
	return i.i.objForHandle(h)
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
func (i *Interp) GetVars(names ...string) map[string]*Obj {
	result := make(map[string]*Obj, len(names))
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
type CommandFunc func(i *Interp, cmd *Obj, args []*Obj) Result

// RegisterCommand adds a command using the low-level CommandFunc interface.
//
// Use this when you need full control over argument handling, access to the
// interpreter, or custom error messages. For simpler cases, use [Interp.Register].
//
//	interp.RegisterCommand("sum", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
//	    if len(args) < 2 {
//	        return feather.Errorf("wrong # args: should be \"%s a b\"", cmd.String())
//	    }
//	    a, err := feather.AsInt(args[0])
//	    if err != nil {
//	        return feather.Error(err.Error())
//	    }
//	    b, err := feather.AsInt(args[1])
//	    if err != nil {
//	        return feather.Error(err.Error())
//	    }
//	    return feather.OK(a + b)
//	})
func (i *Interp) RegisterCommand(name string, fn CommandFunc) {
	i.i.Register(name, func(ii *InternalInterp, cmd FeatherObj, args []FeatherObj) FeatherResult {
		objArgs := make([]*Obj, len(args))
		for j, h := range args {
			objArgs[j] = ii.objForHandle(h)
		}
		cmdObj := ii.objForHandle(cmd)
		r := fn(i, cmdObj, objArgs)
		if r.hasObj && r.obj != nil {
			h := ii.handleForObj(r.obj)
			if r.code == ResultError {
				ii.SetError(h)
			} else {
				ii.SetResult(h)
			}
		} else if r.code == ResultError {
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
//	interp.SetUnknownHandler(func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
//	    // Try to auto-load the command
//	    if loaded := tryLoadCommand(cmd.String()); loaded {
//	        return i.Call(cmd.String(), args...)
//	    }
//	    return feather.Errorf("unknown command: %s", cmd.String())
//	})
func (i *Interp) SetUnknownHandler(fn CommandFunc) {
	i.i.SetUnknownHandler(func(ii *InternalInterp, cmd FeatherObj, args []FeatherObj) FeatherResult {
		objArgs := make([]*Obj, len(args))
		for j, h := range args {
			objArgs[j] = ii.objForHandle(h)
		}
		cmdObj := ii.objForHandle(cmd)
		r := fn(i, cmdObj, objArgs)
		if r.hasObj && r.obj != nil {
			h := ii.handleForObj(r.obj)
			if r.code == ResultError {
				ii.SetError(h)
			} else {
				ii.SetResult(h)
			}
		} else if r.code == ResultError {
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

// Internal returns the underlying InternalInterp for advanced usage.
//
// This is an escape hatch for accessing features not yet exposed in the
// public API. Use with caution; the internal API may change.
func (i *Interp) Internal() *InternalInterp {
	return i.i
}

// -----------------------------------------------------------------------------
// Parsing
// -----------------------------------------------------------------------------

// ParseList parses a string into a list.
//
// Use this when you have a string that needs to be parsed as a TCL list.
// For objects that are already lists, use [AsList] instead.
//
//	items, err := interp.ParseList("{a b} c d")
//	// items = []*Obj{"a b", "c", "d"}
func (i *Interp) ParseList(s string) ([]*Obj, error) {
	strHandle := i.i.InternString(s)
	handles, err := i.i.GetList(strHandle)
	if err != nil {
		return nil, err
	}
	result := make([]*Obj, len(handles))
	for j, h := range handles {
		result[j] = i.i.objForHandle(h)
	}
	return result, nil
}

// ParseDict parses a string into a dict.
//
// Use this when you have a string that needs to be parsed as a TCL dict.
// For objects that are already dicts, use [AsDict] instead.
//
//	d, err := interp.ParseDict("name Alice age 30")
//	// d.Items["name"].String() == "Alice"
func (i *Interp) ParseDict(s string) (*DictType, error) {
	strHandle := i.i.InternString(s)
	items, order, err := i.i.GetDict(strHandle)
	if err != nil {
		return nil, err
	}
	result := &DictType{
		Items: make(map[string]*Obj, len(items)),
		Order: order,
	}
	for k, h := range items {
		result.Items[k] = i.i.objForHandle(h)
	}
	return result, nil
}

// -----------------------------------------------------------------------------
// Command Results
// -----------------------------------------------------------------------------

// Result represents the result of a command execution.
//
// Create results using [OK], [Error], or [Errorf].
type Result struct {
	code   FeatherResult
	val    string // used when obj is nil
	obj    *Obj   // used when non-nil (preserves type)
	hasObj bool   // true if obj should be used
}

// OK returns a successful result with a value.
//
// The value is auto-converted to a TCL string representation.
// Pass a [*Obj] directly to preserve its internal type (int, list, dict, etc.).
//
//	return feather.OK("success")
//	return feather.OK(42)
//	return feather.OK([]string{"a", "b"})
//	return feather.OK(myObj)  // preserves *Obj type
func OK(v any) Result {
	if o, ok := v.(*Obj); ok {
		return Result{code: ResultOK, obj: o, hasObj: true}
	}
	return Result{code: ResultOK, val: toTclString(v)}
}

// Error returns an error result with a message or *Obj.
//
// Pass a string for simple error messages, or a [*Obj] for structured errors.
//
//	return feather.Error("something went wrong")
//	return feather.Error(errDict)  // structured error
func Error(v any) Result {
	if o, ok := v.(*Obj); ok {
		return Result{code: ResultError, obj: o, hasObj: true}
	}
	if s, ok := v.(string); ok {
		return Result{code: ResultError, val: s}
	}
	return Result{code: ResultError, val: toTclString(v)}
}

// Errorf returns a formatted error result.
//
//	return feather.Errorf("expected %d args, got %d", want, got)
func Errorf(format string, args ...any) Result {
	return Result{code: ResultError, val: fmt.Sprintf(format, args...)}
}

// -----------------------------------------------------------------------------
// Parse Status
// -----------------------------------------------------------------------------

// ParseStatus indicates the result of parsing a script.
type ParseStatus int

const (
	// ParseOK indicates the script is syntactically complete and valid.
	ParseOK ParseStatus = ParseStatus(InternalParseOK)

	// ParseIncomplete indicates the script has unclosed braces, brackets, or quotes.
	ParseIncomplete ParseStatus = ParseStatus(InternalParseIncomplete)

	// ParseError indicates a syntax error in the script.
	ParseError ParseStatus = ParseStatus(InternalParseError)
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
	return DefineType[T](fi.i, name, ForeignTypeDef[T]{
		New:       def.New,
		Methods:   Methods(def.Methods),
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
