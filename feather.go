package feather

import (
	"fmt"
	"reflect"
	"runtime/cgo"
	"strings"
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
	handle          FeatherInterp
	objects         map[FeatherObj]*Obj // permanent storage (foreign objects)
	scratch         map[FeatherObj]*Obj // scratch arena (temporary objects, reset after eval)
	scratchNextID   FeatherObj          // next ID for scratch arena (has high bit set)
	globalNS        FeatherObj          // global namespace object (FeatherObj handle for "::")
	namespaces      map[string]*Namespace
	globalNamespace *Namespace
	nextID          FeatherObj // next ID for permanent storage (no high bit)
	result          *Obj       // current result (persistent, not handle)
	returnOptions   *Obj       // options from the last return command (persistent)
	frames          []*CallFrame
	active          int  // currently active frame index
	recursionLimit  int  // maximum call stack depth (0 means use default)
	scriptPath      *Obj // current script file being executed (nil = none)
	builders        map[FeatherObj]*strings.Builder
	evalDepth       int  // tracks nested eval calls for scratch arena management

	// Commands holds registered Go command implementations.
	// Low-level API. May change between versions.
	Commands map[string]InternalCommandFunc

	// ForeignRegistry stores foreign type definitions for the high-level API.
	ForeignRegistry *ForeignRegistry

	unknownHandler InternalCommandFunc
}

// -----------------------------------------------------------------------------
// objHandle - Low-Level Handle Wrapper (internal)
// -----------------------------------------------------------------------------

// objHandle wraps a FeatherObj handle with its interpreter for method access.
// This type exists only in Go and never crosses the CGo boundary.
type objHandle struct {
	h FeatherObj
	i *Interp
}

// wrap creates an objHandle for method-based access to handle data.
func (i *Interp) wrap(h FeatherObj) objHandle {
	return objHandle{h: h, i: i}
}

// raw returns the underlying FeatherObj handle.
func (o objHandle) raw() FeatherObj { return o.h }

// string returns the string representation of the handle.
func (o objHandle) str() string {
	return o.i.getString(o.h)
}

// intVal returns the integer value, or error if not convertible.
func (o objHandle) intVal() (int64, error) {
	return o.i.getInt(o.h)
}

// doubleVal returns the float64 value, or error if not convertible.
func (o objHandle) doubleVal() (float64, error) {
	return o.i.getDouble(o.h)
}

// listVal returns list elements as objHandles, or error if not a list.
func (o objHandle) listVal() ([]objHandle, error) {
	handles, err := o.i.getList(o.h)
	if err != nil {
		return nil, err
	}
	result := make([]objHandle, len(handles))
	for idx, h := range handles {
		result[idx] = objHandle{h: h, i: o.i}
	}
	return result, nil
}

// dictVal returns dict as map with objHandle values, or error if not a dict.
// Returns the items map, key order slice, and any error.
func (o objHandle) dictVal() (map[string]objHandle, []string, error) {
	items, order, err := o.i.getDict(o.h)
	if err != nil {
		return nil, nil, err
	}
	result := make(map[string]objHandle, len(items))
	for k, h := range items {
		result[k] = objHandle{h: h, i: o.i}
	}
	return result, order, nil
}

// isForeign returns true if this is a foreign object.
func (o objHandle) isForeign() bool {
	return o.i.getForeignType(o.h) != ""
}

// foreignType returns the type name if foreign, empty string otherwise.
func (o objHandle) foreignType() string {
	return o.i.getForeignType(o.h)
}

// foreignValue returns the Go value if foreign, nil otherwise.
func (o objHandle) foreignValue() any {
	return o.i.getForeignValue(o.h)
}

// New creates a new TCL interpreter with all standard commands registered.
//
// The interpreter must be closed with [Interp.Close] when no longer needed
// to release resources.
//
//	interp := feather.New()
//	defer interp.Close()
func New() *Interp {
	interp := &Interp{
		objects:       make(map[FeatherObj]*Obj),
		scratch:       make(map[FeatherObj]*Obj),
		scratchNextID: scratchHandleBit | 1, // Start scratch IDs with high bit set
		namespaces:    make(map[string]*Namespace),
		builders:      make(map[FeatherObj]*strings.Builder),
		Commands:      make(map[string]InternalCommandFunc),
		nextID:        1, // Permanent IDs start at 1 (no high bit)
	}
	// Create the global namespace
	globalNS := &Namespace{
		fullPath: "::",
		parent:   nil,
		children: make(map[string]*Namespace),
		vars:     make(map[string]*Obj),
		commands: make(map[string]*Command),
	}
	interp.globalNamespace = globalNS
	interp.namespaces["::"] = globalNS
	// Initialize the global frame (frame 0)
	globalFrame := &CallFrame{
		locals: globalNS,
		links:  make(map[string]varLink),
		level:  0,
		ns:     globalNS,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0
	// Use cgo.Handle to allow C callbacks to find this interpreter
	interp.handle = FeatherInterp(cgo.NewHandle(interp))
	// Create the global namespace object (FeatherObj handle for "::")
	interp.globalNS = interp.internStringPermanent("::")
	// Initialize the C interpreter
	callCInterpInit(interp.handle)
	return interp
}

// Close releases resources associated with the interpreter.
//
// After Close is called, the interpreter and all *Obj values created from it
// become invalid. Always use defer to ensure Close is called.
func (i *Interp) Close() {
	cgo.Handle(i.handle).Delete()
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
	return &Obj{bytes: s, interp: i}
}

// Int creates an integer object.
//
//	n := interp.Int(42)
//	n.Type()   // "int"
//	n.String() // "42"
func (i *Interp) Int(v int64) *Obj {
	return &Obj{intrep: IntType(v), interp: i}
}

// Double creates a floating-point object.
//
//	d := interp.Double(3.14)
//	d.Type()   // "double"
//	d.String() // "3.14"
func (i *Interp) Double(v float64) *Obj {
	return &Obj{intrep: DoubleType(v), interp: i}
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
		return &Obj{intrep: IntType(1), interp: i}
	}
	return &Obj{intrep: IntType(0), interp: i}
}

// List creates a list object from the given items.
//
//	list := interp.List(interp.String("a"), interp.Int(1), interp.Bool(true))
//	list.Type()   // "list"
//	list.String() // "a 1 1"
func (i *Interp) List(items ...*Obj) *Obj {
	return &Obj{intrep: ListType(items), interp: i}
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
			items[j] = i.String(v)
		}
	case []int:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = i.Int(int64(v))
		}
	case []int64:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = i.Int(v)
		}
	case []float64:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = i.Double(v)
		}
	case []any:
		items = make([]*Obj, len(s))
		for j, v := range s {
			items[j] = i.anyToObj(v)
		}
	}
	return i.List(items...)
}

// Dict creates an empty dict object.
//
// For populated dicts, use [Interp.DictKV] or [Interp.DictFrom]:
//
//	dict := interp.DictKV("name", "Alice", "age", 30)
func (i *Interp) Dict() *Obj {
	return &Obj{intrep: &DictType{Items: make(map[string]*Obj)}, interp: i}
}

// Obj creates an object with a custom ObjType internal representation.
//
// Use this when implementing custom shimmering types:
//
//	type RegexType struct {
//	    pattern string
//	    re      *regexp.Regexp
//	}
//	func (t *RegexType) Name() string         { return "regex" }
//	func (t *RegexType) UpdateString() string { return t.pattern }
//	func (t *RegexType) Dup() feather.ObjType { return t }
//
//	obj := interp.Obj(&RegexType{pattern: "^foo", re: re})
func (i *Interp) Obj(intrep ObjType) *Obj {
	return &Obj{intrep: intrep, interp: i}
}

// DictKV creates a dict object from alternating key-value pairs.
//
// Keys should be strings (non-strings are converted via fmt.Sprintf).
// Values are auto-converted based on their Go type.
//
//	dict := interp.DictKV("name", "Alice", "age", 30, "active", true)
//	dict.String() // "name Alice age 30 active 1"
func (i *Interp) DictKV(kvs ...any) *Obj {
	items := make(map[string]*Obj)
	order := make([]string, 0, len(kvs)/2)
	for j := 0; j+1 < len(kvs); j += 2 {
		key, ok := kvs[j].(string)
		if !ok {
			key = fmt.Sprintf("%v", kvs[j])
		}
		if _, exists := items[key]; !exists {
			order = append(order, key)
		}
		items[key] = i.anyToObj(kvs[j+1])
	}
	return &Obj{intrep: &DictType{Items: items, Order: order}, interp: i}
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
	items := make(map[string]*Obj, len(m))
	order := make([]string, 0, len(m))
	for k, v := range m {
		order = append(order, k)
		items[k] = i.anyToObj(v)
	}
	return &Obj{intrep: &DictType{Items: items, Order: order}, interp: i}
}

// anyToObj converts any Go value to a *Obj.
// Used internally for auto-conversion in SetVar, DictKV, etc.
func (i *Interp) anyToObj(v any) *Obj {
	switch val := v.(type) {
	case string:
		return i.String(val)
	case int:
		return i.Int(int64(val))
	case int64:
		return i.Int(val)
	case float64:
		return i.Double(val)
	case bool:
		return i.Bool(val)
	case *Obj:
		if val.interp == nil {
			val.interp = i
		}
		return val
	default:
		return i.String(fmt.Sprintf("%v", v))
	}
}

// anyToHandle converts any Go value to an internal object handle.
// Used internally for auto-conversion in SetVar, etc.
func (i *Interp) anyToHandle(v any) FeatherObj {
	return i.handleForObj(i.anyToObj(v))
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
	_, err := i.eval(script)
	if err != nil {
		return nil, err
	}
	return i.objForHandle(i.ResultHandle()), nil
}

// EvalObj evaluates a TCL script contained in an object.
//
// This is equivalent to calling [Interp.Eval] with obj.String(), but may be
// more convenient when working with objects that already contain TCL code.
//
//	script := interp.String("expr 2 + 2")
//	result, err := interp.EvalObj(script)
func (i *Interp) EvalObj(obj *Obj) (*Obj, error) {
	return i.Eval(obj.String())
}

// Call invokes a single TCL command with the given arguments.
//
// Unlike building a command string and using [Interp.Eval], Call passes arguments
// directly to the command without TCL parsing. This means strings with special
// characters (unbalanced braces, $, [, etc.) are passed safely without escaping.
//
// Arguments can be Go types or *Obj values:
//   - *Obj: passed directly as-is
//   - string: converted to TCL string
//   - int, int64: converted to TCL integer
//   - float64: converted to TCL double
//   - bool: converted to 1 or 0
//   - []string: converted to TCL list
//   - Other types: converted via fmt.Sprintf
//
// Examples:
//
//	result, err := interp.Call("expr", "2 + 2")
//	result, err := interp.Call("llength", myList)
//	result, err := interp.Call("myns::proc", arg1, arg2)
//	result, err := interp.Call("usage", "complete", "hello { l", 9)  // unbalanced brace OK
func (i *Interp) Call(cmd string, args ...any) (*Obj, error) {
	// Build the command string with proper quoting
	parts := make([]string, len(args)+1)
	parts[0] = quote(cmd)
	for idx, arg := range args {
		parts[idx+1] = toTclString(arg)
	}

	script := strings.Join(parts, " ")
	result, err := i.Eval(script)
	if err != nil {
		return nil, err
	}
	return result, nil
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
//	v.Int()   // 42, nil
//	v.Type()  // "int" (if SetVar preserved type) or "string"
func (i *Interp) Var(name string) *Obj {
	h := i.GetVarHandle(name)
	if h == 0 {
		return i.String("")
	}
	return i.objForHandle(h)
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
	i.setVar(name, toTclString(val))
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
		i.setVar(name, toTclString(val))
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
	i.register(name, func(ii *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
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

// UnregisterCommand removes a previously registered command.
// This is used by destroy methods to make the command unavailable.
func (i *Interp) UnregisterCommand(name string) {
	delete(i.Commands, name)
	if i.globalNamespace != nil {
		delete(i.globalNamespace.commands, name)
	}
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
	i.register(name, wrapper)
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
	i.setUnknownHandler(func(ii *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
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
	pr := i.ParseInternal(script)
	return ParseResult{
		Status:  ParseStatus(pr.Status),
		Message: pr.ErrorMessage,
	}
}

// -----------------------------------------------------------------------------
// Parsing
// -----------------------------------------------------------------------------

// parseList parses a string into a list.
// This is used internally by Obj.List() for shimmering.
func (i *Interp) parseList(s string) ([]*Obj, error) {
	strHandle := i.internString(s)
	handles, err := i.getList(strHandle)
	if err != nil {
		return nil, err
	}
	result := make([]*Obj, len(handles))
	for j, h := range handles {
		result[j] = i.objForHandle(h)
	}
	return result, nil
}

// parseDict parses a string into a dict.
// This is used internally by Obj.Dict() for shimmering.
func (i *Interp) parseDict(s string) (*DictType, error) {
	strHandle := i.internString(s)
	items, order, err := i.getDict(strHandle)
	if err != nil {
		return nil, err
	}
	result := &DictType{
		Items: make(map[string]*Obj, len(items)),
		Order: order,
	}
	for k, h := range items {
		result.Items[k] = i.objForHandle(h)
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
	// For simple types, don't quote - just convert to string
	switch val := v.(type) {
	case string:
		return Result{code: ResultOK, val: val}
	case int:
		return Result{code: ResultOK, val: fmt.Sprintf("%d", val)}
	case int64:
		return Result{code: ResultOK, val: fmt.Sprintf("%d", val)}
	case float64:
		return Result{code: ResultOK, val: fmt.Sprintf("%g", val)}
	case bool:
		if val {
			return Result{code: ResultOK, val: "1"}
		}
		return Result{code: ResultOK, val: "0"}
	default:
		return Result{code: ResultOK, val: fmt.Sprintf("%v", v)}
	}
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
func RegisterType[T any](i *Interp, typeName string, def TypeDef[T]) error {
	if i.ForeignRegistry == nil {
		i.ForeignRegistry = newForeignRegistry()
	}

	i.ForeignRegistry.mu.Lock()
	defer i.ForeignRegistry.mu.Unlock()

	if def.New == nil {
		return fmt.Errorf("RegisterType: New function is required for type %s", typeName)
	}

	info := &foreignTypeInfo{
		name:         typeName,
		newFunc:      reflect.ValueOf(def.New),
		methods:      make(map[string]reflect.Value),
		receiverType: reflect.TypeOf((*T)(nil)).Elem(),
	}

	for name, fn := range def.Methods {
		info.methods[name] = reflect.ValueOf(fn)
	}

	if def.String != nil {
		info.stringRep = reflect.ValueOf(def.String)
	}
	if def.Destroy != nil {
		info.destroy = reflect.ValueOf(def.Destroy)
	}

	i.ForeignRegistry.types[typeName] = info
	i.ForeignRegistry.counters[typeName] = 1

	i.register(typeName, func(interp *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
		return interp.foreignConstructor(typeName, cmd, args)
	})

	return nil
}
