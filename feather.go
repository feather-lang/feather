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
// Exposing Go types:
//
//	feather.Register[*MyService](interp, "Service", feather.TypeDef[*MyService]{
//	    New: func() *MyService { return NewMyService() },
//	    Methods: map[string]any{
//	        "doWork": (*MyService).DoWork,
//	    },
//	})
//	interp.Eval(`set svc [Service new]; $svc doWork`)
package feather

import (
	"github.com/feather-lang/feather/interp"
)

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

// Eval evaluates a TCL script and returns the result.
func (i *Interp) Eval(script string) (Value, error) {
	result, err := i.i.EvalTyped(script)
	if err != nil {
		return nil, err
	}
	return resultInfoToValue(result), nil
}

// resultInfoToValue converts interp.ResultInfo to a Value.
func resultInfoToValue(r interp.ResultInfo) Value {
	switch {
	case r.IsForeign:
		return foreignValue{typeName: r.ForeignType, handle: r.String}
	case r.IsDict:
		keys := r.DictKeys
		values := make(map[string]Value, len(r.DictValues))
		for k, v := range r.DictValues {
			values[k] = resultInfoToValue(v)
		}
		return dictValue{keys: keys, values: values}
	case r.IsList:
		items := make([]Value, len(r.ListItems))
		for i, item := range r.ListItems {
			items[i] = resultInfoToValue(item)
		}
		return listValue(items)
	case r.IsDouble:
		return floatValue(r.DoubleVal)
	case r.IsInt:
		return intValue(r.IntVal)
	default:
		return stringValue(r.String)
	}
}

// Call invokes a TCL command with the given arguments.
// Arguments are automatically converted from Go types to TCL values.
func (i *Interp) Call(cmd string, args ...any) (Value, error) {
	// Build command string with proper quoting
	script := cmd
	for _, arg := range args {
		script += " " + toTclString(arg)
	}
	return i.Eval(script)
}

// Var returns the value of a variable, or nil if not found.
func (i *Interp) Var(name string) Value {
	val := i.i.GetVar(name)
	if val == "" {
		// Check if variable actually exists with empty value
		// For now, return empty string value
		return stringValue(val)
	}
	return stringValue(val)
}

// SetVar sets a variable to a value.
// The value is automatically converted from Go types to TCL.
func (i *Interp) SetVar(name string, val any) {
	i.i.SetVar(name, toTclString(val))
}

// Register adds a Go function as a TCL command.
// The function signature is inspected via reflection to determine
// how to convert arguments and return values.
//
// Supported function signatures:
//   - func(args ...string) string
//   - func(arg1 string, arg2 int) (string, error)
//   - func() error
//   - etc.
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

// Register registers a foreign type with the interpreter.
// After registration, the type name becomes a command that supports "new".
//
// Example:
//
//	feather.Register[*http.ServeMux](interp, "Mux", feather.TypeDef[*http.ServeMux]{
//	    New: func() *http.ServeMux { return http.NewServeMux() },
//	    Methods: map[string]any{
//	        "handle": func(m *http.ServeMux, pattern, handler string) { ... },
//	    },
//	})
func Register[T any](fi *Interp, name string, def TypeDef[T]) error {
	return interp.DefineType[T](fi.i, name, interp.ForeignTypeDef[T]{
		New:       def.New,
		Methods:   interp.Methods(def.Methods),
		StringRep: def.String,
		Destroy:   def.Destroy,
	})
}
