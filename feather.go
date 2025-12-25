// Package feather provides an embeddable TCL interpreter.
//
// feather is a pure, minimal implementation of TCL designed for embedding
// into Go applications. It provides a clean, idiomatic Go API while preserving
// TCL's powerful metaprogramming capabilities.
//
// Basic usage:
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
// Registering Go functions:
//
//	interp.Register("greet", func(name string) string {
//	    return "Hello, " + name + "!"
//	})
//	result, _ := interp.Eval(`greet World`)
//	// result.String() == "Hello, World!"
//
// Registering commands with full control:
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
// Exposing Go types:
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

// Object represents a TCL value. Only valid while the owning interpreter lives.
// Object provides lazy access to value representations through shimmering.
type Object struct {
	i *interp.Interp
	h interp.FeatherObj
}

// String returns the string representation of the object.
func (o Object) String() string {
	if o.i == nil {
		return ""
	}
	return o.i.GetString(o.h)
}

// Int returns the integer representation of the object.
// Returns an error if the value cannot be converted to an integer.
func (o Object) Int() (int64, error) {
	if o.i == nil {
		return 0, fmt.Errorf("nil object")
	}
	return o.i.GetInt(o.h)
}

// Float returns the floating-point representation of the object.
// Returns an error if the value cannot be converted to a float.
func (o Object) Float() (float64, error) {
	if o.i == nil {
		return 0, fmt.Errorf("nil object")
	}
	return o.i.GetDouble(o.h)
}

// Bool returns the boolean representation of the object.
// TCL truthy: "1", "true", "yes", "on" (case-insensitive)
// TCL falsy: "0", "false", "no", "off" (case-insensitive)
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
// Returns an error if the value cannot be parsed as a TCL list.
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
// Returns an error if the value cannot be converted to a dict
// (e.g., odd number of elements).
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

// Type returns the native type: "string", "int", "double", "list", "dict",
// or a foreign type name.
func (o Object) Type() string {
	if o.i == nil {
		return "string"
	}
	return o.i.Type(o.h)
}

// IsNil returns true if this is a nil/empty object.
func (o Object) IsNil() bool {
	return o.h == 0 || o.i == nil
}

// Interp is a TCL interpreter instance.
type Interp struct {
	i *interp.Interp
}

// New creates a new TCL interpreter.
func New() *Interp {
	return &Interp{
		i: interp.NewInterp(),
	}
}

// Close releases resources associated with the interpreter.
// Must be called when the interpreter is no longer needed.
func (i *Interp) Close() {
	i.i.Close()
}

// Object creation methods

// String creates a string Object.
func (i *Interp) String(s string) Object {
	return Object{i.i, i.i.InternString(s)}
}

// Int creates an integer Object.
func (i *Interp) Int(v int64) Object {
	return Object{i.i, i.i.NewInt(v)}
}

// Float creates a floating-point Object.
func (i *Interp) Float(v float64) Object {
	return Object{i.i, i.i.NewDouble(v)}
}

// List creates a list Object from the given items.
func (i *Interp) List(items ...Object) Object {
	l := i.i.NewList()
	for _, item := range items {
		i.i.ListAppend(l, item.h)
	}
	return Object{i.i, l}
}

// Dict creates an empty dict Object.
func (i *Interp) Dict() Object {
	return Object{i.i, i.i.NewDict()}
}

// Eval evaluates a TCL script and returns the result.
func (i *Interp) Eval(script string) (Object, error) {
	_, err := i.i.Eval(script)
	if err != nil {
		return Object{}, err
	}
	return Object{i.i, i.i.ResultHandle()}, nil
}

// Call invokes a TCL command with the given arguments.
// Arguments are automatically converted from Go types to TCL values.
func (i *Interp) Call(cmd string, args ...any) (Object, error) {
	script := cmd
	for _, arg := range args {
		script += " " + toTclString(arg)
	}
	return i.Eval(script)
}

// Var returns the value of a variable as an Object.
func (i *Interp) Var(name string) Object {
	val := i.i.GetVar(name)
	return Object{i.i, i.i.InternString(val)}
}

// SetVar sets a variable to a value.
// The value is automatically converted from Go types to TCL.
func (i *Interp) SetVar(name string, val any) {
	i.i.SetVar(name, toTclString(val))
}

// CommandFunc is the signature for custom commands.
// Commands receive the interpreter, command name, and arguments.
type CommandFunc func(i *Interp, cmd Object, args []Object) Result

// RegisterCommand adds a command using the uniform CommandFunc interface.
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

// Register adds a command with automatic argument conversion (convenience API).
// Supports signatures like:
//   - func(args ...string) string
//   - func(a int, b string) (string, error)
//   - func() error
func (i *Interp) Register(name string, fn any) {
	wrapper := wrapFunc(i, fn)
	i.i.Register(name, wrapper)
}

// Parse checks if a script is syntactically complete.
// Returns ParseOK, ParseIncomplete, or ParseError.
func (i *Interp) Parse(script string) ParseResult {
	pr := i.i.Parse(script)
	return ParseResult{
		Status:  ParseStatus(pr.Status),
		Message: pr.ErrorMessage,
	}
}

// Internal returns the underlying interp.Interp for advanced usage.
// This is an escape hatch for features not yet exposed in the public API.
func (i *Interp) Internal() *interp.Interp {
	return i.i
}

// Result represents a command result.
type Result struct {
	code interp.FeatherResult
	val  string
}

// OK returns a successful result with a value.
func OK(v any) Result {
	return Result{code: interp.ResultOK, val: toTclString(v)}
}

// Error returns an error result.
func Error(msg string) Result {
	return Result{code: interp.ResultError, val: msg}
}

// Errorf returns a formatted error result.
func Errorf(format string, args ...any) Result {
	return Result{code: interp.ResultError, val: fmt.Sprintf(format, args...)}
}

// ParseStatus indicates the result of parsing a script.
type ParseStatus int

const (
	ParseOK         ParseStatus = ParseStatus(interp.ParseOK)
	ParseIncomplete ParseStatus = ParseStatus(interp.ParseIncomplete)
	ParseError      ParseStatus = ParseStatus(interp.ParseError)
)

// ParseResult holds the result of parsing a script.
type ParseResult struct {
	Status  ParseStatus
	Message string
}

// TypeDef defines a foreign type that can be exposed to TCL.
type TypeDef[T any] struct {
	// New is the constructor function. Called when "TypeName new" is evaluated.
	New func() T

	// Methods maps method names to implementations.
	// Each method should be a function where the first argument is the receiver (T).
	Methods map[string]any

	// String optionally provides a custom string representation.
	String func(T) string

	// Destroy is called when the object is destroyed.
	Destroy func(T)
}

// RegisterType registers a foreign type with the interpreter.
// After registration, the type name becomes a command that supports "new".
//
// Example:
//
//	feather.RegisterType[*http.ServeMux](interp, "Mux", feather.TypeDef[*http.ServeMux]{
//	    New: func() *http.ServeMux { return http.NewServeMux() },
//	    Methods: map[string]any{
//	        "handle": func(m *http.ServeMux, pattern, handler string) { ... },
//	    },
//	})
func RegisterType[T any](fi *Interp, name string, def TypeDef[T]) error {
	return interp.DefineType[T](fi.i, name, interp.ForeignTypeDef[T]{
		New:       def.New,
		Methods:   interp.Methods(def.Methods),
		StringRep: def.String,
		Destroy:   def.Destroy,
	})
}

// Deprecated: Register is renamed to RegisterType
func Register[T any](fi *Interp, name string, def TypeDef[T]) error {
	return RegisterType[T](fi, name, def)
}
