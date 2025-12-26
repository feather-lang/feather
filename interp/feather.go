package interp

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#cgo LDFLAGS: ${SRCDIR}/../build/libfeather.a
#include "feather.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"runtime/cgo"
	"strings"
	"unsafe"
)

type FeatherResult uint

// Result codes matching FeatherResult enum
const (
	ResultOK       FeatherResult = C.TCL_OK
	ResultError    FeatherResult = C.TCL_ERROR
	ResultReturn   FeatherResult = C.TCL_RETURN
	ResultBreak    FeatherResult = C.TCL_BREAK
	ResultContinue FeatherResult = C.TCL_CONTINUE
)

// EvalFlags matching FeatherEvalFlags enum
const (
	EvalLocal  = C.TCL_EVAL_LOCAL
	EvalGlobal = C.TCL_EVAL_GLOBAL
)

// ParseStatus matching FeatherParseStatus enum
type ParseStatus uint

const (
	ParseOK         ParseStatus = C.TCL_PARSE_OK
	ParseIncomplete ParseStatus = C.TCL_PARSE_INCOMPLETE
	ParseError      ParseStatus = C.TCL_PARSE_ERROR
)

// ParseResult holds the result of parsing a script
type ParseResult struct {
	Status       ParseStatus
	Result       string // The interpreter's result string (e.g., "{INCOMPLETE 5 20}")
	ErrorMessage string // For ParseError, the error message from the result list
}

// Handle is the Go type for FeatherHandle
type Handle = uintptr

// FeatherInterp is a handle to an interpreter instance
type FeatherInterp Handle

// FeatherObj is a handle to an object
type FeatherObj Handle

// CommandFunc is the signature for host command implementations.
// Commands receive the interpreter, the command name and a list of argument objects.
//
// # In case of an error, the command should set the interpreter's error information and return ResultError
//
// To return a value, the command should set the interpreter's result value and return ResultOK
type CommandFunc func(i *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult

// varLink represents a link to a variable in another frame (for upvar)
// or a link to a namespace variable (for variable command)
type varLink struct {
	targetLevel int    // frame level where the target variable lives (-1 for namespace links)
	targetName  string // name of the variable in the target frame

	// For namespace variable links (set when targetLevel == -1)
	nsPath string // absolute namespace path, e.g., "::foo"
	nsName string // variable name in namespace
}

// Namespace represents a namespace in the hierarchy
type Namespace struct {
	fullPath       string
	parent         *Namespace
	children       map[string]*Namespace
	vars           map[string]FeatherObj
	commands       map[string]*Command // commands defined in this namespace
	exportPatterns []string            // patterns for exported commands (e.g., "get*", "set*")
}

// CallFrame represents an execution frame on the call stack.
// Each frame has its own variable environment.
type CallFrame struct {
	cmd    FeatherObj         // command being evaluated
	args   FeatherObj         // arguments to the command
	locals *Namespace         // local variable storage (for global frame, this IS the :: namespace)
	links  map[string]varLink // upvar links: local name -> target variable
	level  int                // frame index on the call stack
	ns     *Namespace         // current namespace context
}

// Procedure represents a user-defined procedure
type Procedure struct {
	name   FeatherObj
	params FeatherObj
	body   FeatherObj
}

// CommandType indicates the type of a command
type CommandType int

const (
	CmdNone    CommandType = 0 // command doesn't exist
	CmdBuiltin CommandType = 1 // it's a builtin command
	CmdProc    CommandType = 2 // it's a user-defined procedure
)

// Command represents an entry in the unified command table
type Command struct {
	cmdType CommandType      // type of command
	builtin C.FeatherBuiltinCmd  // function pointer (only for CmdBuiltin)
	proc    *Procedure       // procedure info (only for CmdProc)
}

// TraceEntry represents a single trace registration
type TraceEntry struct {
	ops    string // space-separated operations: "read write" or "rename delete"
	script FeatherObj // the command prefix to invoke
}

// Interp represents a TCL interpreter instance
type Interp struct {
	handle         FeatherInterp
	objects        map[FeatherObj]*Obj
	globalNS       FeatherObj              // global namespace object (FeatherObj handle for "::")
	namespaces     map[string]*Namespace // namespace path -> Namespace
	globalNamespace *Namespace           // the global namespace "::"
	nextID         FeatherObj
	result         FeatherObj
	returnOptions  FeatherObj       // options from the last return command
	frames         []*CallFrame // call stack (frame 0 is global)
	active         int          // currently active frame index
	recursionLimit int          // maximum call stack depth (0 means use default)
	scriptPath     FeatherObj       // current script file being executed (0 = none)
	varTraces      map[string][]TraceEntry // variable name -> traces
	cmdTraces      map[string][]TraceEntry // command name -> traces

	// Commands holds registered Go command implementations.
	// These are dispatched via the unknown handler when the C layer
	// doesn't find a builtin or proc.
	Commands map[string]CommandFunc

	// ForeignRegistry stores foreign type definitions for the high-level API.
	// Set by DefineType when registering foreign types.
	ForeignRegistry *ForeignRegistry
}

// NewInterp creates a new interpreter
func NewInterp() *Interp {
	interp := &Interp{
		objects:    make(map[FeatherObj]*Obj),
		namespaces: make(map[string]*Namespace),
		varTraces:  make(map[string][]TraceEntry),
		cmdTraces:  make(map[string][]TraceEntry),
		Commands:   make(map[string]CommandFunc),
		nextID:     1,
	}
	// Create the global namespace
	globalNS := &Namespace{
		fullPath: "::",
		parent:   nil,
		children: make(map[string]*Namespace),
		vars:     make(map[string]FeatherObj),
		commands: make(map[string]*Command),
	}
	interp.globalNamespace = globalNS
	interp.namespaces["::"] = globalNS
	// Initialize the global frame (frame 0)
	// The global frame's locals IS the global namespace - unified storage
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
	interp.globalNS = interp.internString("::")
	// Initialize the C interpreter (registers builtins)
	callCInterpInit(interp.handle)
	return interp
}

// Close releases resources associated with the interpreter.
// Must be called when the interpreter is no longer needed.
func (i *Interp) Close() {
	cgo.Handle(i.handle).Delete()
}

// Register adds a Go command to the interpreter.
// The command will be invoked when the C layer doesn't find a builtin or proc.
func (i *Interp) Register(name string, fn CommandFunc) {
	i.Commands[name] = fn
	// Also register in interpreter's namespace storage for enumeration.
	// These are Go commands dispatched via bind.unknown, not C builtins.
	// We set builtin to nil so the C code falls through to unknown handler.
	i.globalNamespace.commands[name] = &Command{
		cmdType: CmdBuiltin,
		builtin: nil, // nil means dispatch via bind.unknown
	}
}

// dispatch handles command lookup and execution for Go-registered commands.
func (i *Interp) dispatch(cmd FeatherObj, args []FeatherObj) FeatherResult {
	cmdStr := i.GetString(cmd)
	if fn, ok := i.Commands[cmdStr]; ok {
		return fn(i, cmd, args)
	}
	i.SetErrorString("invalid command name \"" + cmdStr + "\"")
	return ResultError
}

// DefaultRecursionLimit is the default maximum call stack depth.
const DefaultRecursionLimit = 1000

// SetRecursionLimit sets the maximum call stack depth.
// If limit is 0 or negative, the default limit (1000) is used.
func (i *Interp) SetRecursionLimit(limit int) {
	if limit <= 0 {
		i.recursionLimit = DefaultRecursionLimit
	} else {
		i.recursionLimit = limit
	}
}

// getRecursionLimit returns the effective recursion limit.
func (i *Interp) getRecursionLimit() int {
	if i.recursionLimit <= 0 {
		return DefaultRecursionLimit
	}
	return i.recursionLimit
}

// Handle returns the interpreter's handle
func (i *Interp) Handle() FeatherInterp {
	return i.handle
}

// Parse parses a script string and returns the parse status and result.
func (i *Interp) Parse(script string) ParseResult {
	scriptHandle := i.internString(script)

	// Call the C parser
	status := callCParse(i.handle, scriptHandle)

	var resultStr string
	var errorMsg string
	if obj := i.getObject(i.result); obj != nil {
		resultStr = i.objToString(obj)
		// For parse errors, extract the error message (4th element) directly from the list
		if ParseStatus(status) == ParseError {
			if listItems, err := AsList(obj); err == nil && len(listItems) >= 4 {
				if msgObj := listItems[3]; msgObj != nil {
					errorMsg = msgObj.String()
				}
			}
		}
	}

	return ParseResult{
		Status:       ParseStatus(status),
		Result:       resultStr,
		ErrorMessage: errorMsg,
	}
}

// objToString converts an *Obj to its TCL string representation for display.
// This includes outer braces for non-empty lists (used for parse result display).
func (i *Interp) objToString(obj *Obj) string {
	if obj == nil {
		return ""
	}
	// Try list first
	if listItems, err := AsList(obj); err == nil {
		// Build TCL list representation: {elem1 elem2 ...}
		var result string
		for idx, itemObj := range listItems {
			if itemObj == nil {
				continue
			}
			if idx > 0 {
				result += " "
			}
			// Handle different object types based on intrep
			switch itemObj.intrep.(type) {
			case IntType:
				result += itemObj.String()
			case ListType:
				result += i.objToString(itemObj)
			default:
				// Quote strings that contain spaces or are empty
				s := itemObj.String()
				if len(s) == 0 || strings.ContainsAny(s, " \t\n") {
					result += "{" + s + "}"
				} else {
					result += s
				}
			}
		}
		if len(listItems) > 0 {
			result = "{" + result + "}"
		}
		return result
	}
	// Check for int type
	if _, ok := obj.intrep.(IntType); ok {
		return obj.String()
	}
	// Default: return string value
	return obj.String()
}

// objToValue converts an *Obj to its TCL value string representation.
// This does NOT include outer braces (used when returning list as a value).
func (i *Interp) objToValue(obj *Obj) string {
	if obj == nil {
		return ""
	}
	// Try list first
	if listItems, err := AsList(obj); err == nil {
		// Build TCL list value: elem1 elem2 ...
		// Elements with spaces are braced, but the list itself is not wrapped
		var result string
		for idx, itemObj := range listItems {
			if itemObj == nil {
				continue
			}
			if idx > 0 {
				result += " "
			}
			// Handle different object types based on intrep
			switch itemObj.intrep.(type) {
			case IntType:
				result += itemObj.String()
			case DoubleType:
				result += itemObj.String()
			case ListType:
				// Nested lists need to be braced
				nested := i.objToValue(itemObj)
				nestedList, _ := AsList(itemObj)
				if len(nestedList) > 0 || strings.ContainsAny(nested, " \t\n") {
					result += "{" + nested + "}"
				} else {
					result += nested
				}
			default:
				// Quote strings that contain spaces, special chars, or are empty
				s := itemObj.String()
				if len(s) == 0 || strings.ContainsAny(s, " \t\n{}") {
					result += "{" + s + "}"
				} else {
					result += s
				}
			}
		}
		return result
	}
	// Check for int type
	if _, ok := obj.intrep.(IntType); ok {
		return obj.String()
	}
	// Default: return string value
	return obj.String()
}

// dictObjToValue converts a dict *Obj to its TCL string representation.
// Dicts are represented as lists: key1 value1 key2 value2 ...
func (i *Interp) dictObjToValue(obj *Obj) string {
	if obj == nil {
		return ""
	}
	d, ok := obj.intrep.(*DictType)
	if !ok {
		return ""
	}
	var result string
	first := true
	for _, key := range d.Order {
		if !first {
			result += " "
		}
		first = false
		// Quote key if needed
		if len(key) == 0 || strings.ContainsAny(key, " \t\n{}") {
			result += "{" + key + "}"
		} else {
			result += key
		}
		result += " "
		// Get value and convert to string
		if val, ok := d.Items[key]; ok {
			valStr := val.String()
			// Quote value if needed
			if len(valStr) == 0 || strings.ContainsAny(valStr, " \t\n{}") {
				result += "{" + valStr + "}"
			} else {
				result += valStr
			}
		} else {
			result += "{}"
		}
	}
	return result
}

// Eval evaluates a script string using the C interpreter
func (i *Interp) Eval(script string) (string, error) {
	scriptHandle := i.internString(script)

	// Call the C interpreter
	result := callCEval(i.handle, scriptHandle)

	if result == C.TCL_OK {
		return i.GetString(i.result), nil
	}

	// Handle TCL_RETURN at top level - apply the return options
	if result == C.TCL_RETURN {
		// Get return options and apply the code
		var code C.FeatherResult = C.TCL_OK
		if i.returnOptions != 0 {
			items, err := i.GetList(i.returnOptions)
			if err == nil {
				for j := 0; j+1 < len(items); j += 2 {
					key := i.GetString(items[j])
					if key == "-code" {
						if codeVal, err := i.GetInt(items[j+1]); err == nil {
							code = C.FeatherResult(codeVal)
						}
					}
				}
			}
		}
		// Apply the extracted code
		if code == C.TCL_OK {
			return i.GetString(i.result), nil
		}
		if code == C.TCL_ERROR {
			return "", &EvalError{Message: i.GetString(i.result)}
		}
		if code == C.TCL_BREAK {
			return "", &EvalError{Message: "invoked \"break\" outside of a loop"}
		}
		if code == C.TCL_CONTINUE {
			return "", &EvalError{Message: "invoked \"continue\" outside of a loop"}
		}
		// For other codes, treat as ok
		return i.GetString(i.result), nil
	}

	// Convert break/continue outside loop to errors at the top level
	if result == C.TCL_BREAK {
		return "", &EvalError{Message: "invoked \"break\" outside of a loop"}
	}
	if result == C.TCL_CONTINUE {
		return "", &EvalError{Message: "invoked \"continue\" outside of a loop"}
	}

	return "", &EvalError{Message: i.GetString(i.result)}
}

// Result returns the current result string
func (i *Interp) Result() string {
	return i.GetString(i.result)
}

// ResultHandle returns the current result object handle.
func (i *Interp) ResultHandle() FeatherObj {
	return i.result
}

// EvalError represents an evaluation error
type EvalError struct {
	Message string
}

func (e *EvalError) Error() string {
	return e.Message
}

// ResultInfo contains type information about a TCL value.
type ResultInfo struct {
	String      string
	IsInt       bool
	IntVal      int64
	IsDouble    bool
	DoubleVal   float64
	IsList      bool
	ListItems   []ResultInfo
	IsDict      bool
	DictKeys    []string
	DictValues  map[string]ResultInfo
	IsForeign   bool
	ForeignType string
}

// EvalTyped evaluates a script and returns typed result info.
func (i *Interp) EvalTyped(script string) (ResultInfo, error) {
	_, err := i.Eval(script)
	if err != nil {
		return ResultInfo{}, err
	}
	return i.getResultInfo(i.result), nil
}

// getResultInfo extracts type information from a FeatherObj.
func (i *Interp) getResultInfo(h FeatherObj) ResultInfo {
	obj := i.objects[h]
	if obj == nil {
		return ResultInfo{String: ""}
	}

	info := ResultInfo{
		String: obj.String(),
	}

	// Check type based on intrep
	switch t := obj.intrep.(type) {
	case IntType:
		info.IsInt = true
		info.IntVal = int64(t)
	case DoubleType:
		info.IsDouble = true
		info.DoubleVal = float64(t)
	case ListType:
		info.IsList = true
		info.ListItems = make([]ResultInfo, len(t))
		for idx, item := range t {
			info.ListItems[idx] = i.objToResultInfo(item)
		}
	case *DictType:
		info.IsDict = true
		info.DictKeys = t.Order
		info.DictValues = make(map[string]ResultInfo, len(t.Items))
		for k, v := range t.Items {
			info.DictValues[k] = i.objToResultInfo(v)
		}
	case *ForeignType:
		info.IsForeign = true
		info.ForeignType = t.TypeName
	}

	return info
}

// objToResultInfo returns information about an *Obj for inspection.
func (i *Interp) objToResultInfo(obj *Obj) ResultInfo {
	if obj == nil {
		return ResultInfo{String: ""}
	}

	info := ResultInfo{
		String: obj.String(),
	}

	// Check type based on intrep
	switch t := obj.intrep.(type) {
	case IntType:
		info.IsInt = true
		info.IntVal = int64(t)
	case DoubleType:
		info.IsDouble = true
		info.DoubleVal = float64(t)
	case ListType:
		info.IsList = true
		info.ListItems = make([]ResultInfo, len(t))
		for idx, item := range t {
			info.ListItems[idx] = i.objToResultInfo(item)
		}
	case *DictType:
		info.IsDict = true
		info.DictKeys = t.Order
		info.DictValues = make(map[string]ResultInfo, len(t.Items))
		for k, v := range t.Items {
			info.DictValues[k] = i.objToResultInfo(v)
		}
	case *ForeignType:
		info.IsForeign = true
		info.ForeignType = t.TypeName
	}

	return info
}

// internString stores a string and returns its handle
func (i *Interp) internString(s string) FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = NewString(s)
	return id
}

// InternString stores a string and returns its handle.
func (i *Interp) InternString(s string) FeatherObj {
	return i.internString(s)
}

// registerObj stores an *Obj and returns its handle.
// Used when we need to give C a handle to an existing *Obj.
func (i *Interp) registerObj(obj *Obj) FeatherObj {
	if obj == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = obj
	return id
}

// createListFromHandles creates a new list from a slice of handles.
// Used by callbacks that work with FeatherObj handles.
func (i *Interp) createListFromHandles(handles []FeatherObj) FeatherObj {
	items := make([]*Obj, len(handles))
	for idx, h := range handles {
		items[idx] = i.getObject(h)
	}
	return i.registerObj(&Obj{intrep: ListType(items)})
}

// getListItems returns the list items as *Obj slice.
// Performs shimmering if needed. Returns nil on error.
func (i *Interp) getListItems(h FeatherObj) []*Obj {
	obj := i.getObject(h)
	if obj == nil {
		return nil
	}
	if items, err := AsList(obj); err == nil {
		return items
	}
	// Try shimmering via GetList (which parses strings)
	if handles, err := i.GetList(h); err == nil {
		items := make([]*Obj, len(handles))
		for idx, h := range handles {
			items[idx] = i.getObject(h)
		}
		return items
	}
	return nil
}

// setListItems sets the list items directly.
func (i *Interp) setListItems(h FeatherObj, items []*Obj) {
	obj := i.getObject(h)
	if obj == nil {
		return
	}
	obj.intrep = ListType(items)
	obj.Invalidate()
}

// NewForeign creates a new foreign object with the given type name and Go value.
// The string representation is generated as "<TypeName:id>".
// Returns the handle to the new foreign object.
func (i *Interp) NewForeign(typeName string, value any) FeatherObj {
	id := i.nextID
	i.nextID++
	obj := NewForeign(typeName, value)
	// Override the string representation to include the handle ID
	obj.bytes = fmt.Sprintf("<%s:%d>", typeName, id)
	i.objects[id] = obj
	return id
}

// IsForeign returns true if the object is a foreign object.
// Also checks if the string representation is a foreign handle name.
func (i *Interp) IsForeign(h FeatherObj) bool {
	if obj := i.getObject(h); obj != nil {
		if _, ok := obj.intrep.(*ForeignType); ok {
			return true
		}
		// Check if string value is a foreign handle name
		if i.ForeignRegistry != nil {
			i.ForeignRegistry.mu.RLock()
			_, ok := i.ForeignRegistry.instances[obj.String()]
			i.ForeignRegistry.mu.RUnlock()
			if ok {
				return true
			}
		}
	}
	return false
}

// GetForeignType returns the type name of a foreign object, or empty string if not foreign.
// Also checks if the string representation is a foreign handle name.
func (i *Interp) GetForeignType(h FeatherObj) string {
	if obj := i.getObject(h); obj != nil {
		if ft, ok := obj.intrep.(*ForeignType); ok {
			return ft.TypeName
		}
		// Check if string value is a foreign handle name
		if i.ForeignRegistry != nil {
			i.ForeignRegistry.mu.RLock()
			instance, ok := i.ForeignRegistry.instances[obj.String()]
			i.ForeignRegistry.mu.RUnlock()
			if ok {
				return instance.typeName
			}
		}
	}
	return ""
}

// GetForeignValue returns the Go value of a foreign object, or nil if not foreign.
func (i *Interp) GetForeignValue(h FeatherObj) any {
	if obj := i.getObject(h); obj != nil {
		if ft, ok := obj.intrep.(*ForeignType); ok {
			return ft.Value
		}
	}
	return nil
}

// getObject retrieves an object by handle
func (i *Interp) getObject(h FeatherObj) *Obj {
	return i.objects[h]
}

// GetString returns the string representation of an object.
// Performs shimmering: converts int/double/list/dict representations to string as needed.
func (i *Interp) GetString(h FeatherObj) string {
	if obj := i.getObject(h); obj != nil {
		return obj.String()
	}
	return ""
}

// DisplayName returns a user-friendly name for a command.
// Strips the leading "::" for global namespace commands (e.g., "::set" -> "set")
// but preserves the full path for nested namespaces (e.g., "::foo::bar" stays as-is).
func (i *Interp) DisplayName(name string) string {
	if len(name) > 2 && name[0] == ':' && name[1] == ':' {
		// Check if there's another :: after the initial one
		rest := name[2:]
		if !strings.Contains(rest, "::") {
			// Global namespace only - strip the leading ::
			return rest
		}
	}
	return name
}

// GetInt returns the integer representation of an object.
// Performs shimmering: parses string representation as integer if needed.
// Returns an error if the value cannot be converted to an integer.
func (i *Interp) GetInt(h FeatherObj) (int64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	return AsInt(obj)
}

// GetDouble returns the floating-point representation of an object.
// Performs shimmering: parses string representation as double if needed.
// Returns an error if the value cannot be converted to a double.
func (i *Interp) GetDouble(h FeatherObj) (float64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	return AsDouble(obj)
}

// GetList returns the list representation of an object as handles.
// Performs shimmering: parses string representation as list if needed.
// Returns an error if the value cannot be converted to a list.
func (i *Interp) GetList(h FeatherObj) ([]FeatherObj, error) {
	obj := i.getObject(h)
	if obj == nil {
		return nil, fmt.Errorf("nil object")
	}
	// Try to get list via AsList (works for ListType)
	if list, err := AsList(obj); err == nil {
		// Convert []*Obj to []FeatherObj handles
		handles := make([]FeatherObj, len(list))
		for idx, item := range list {
			handles[idx] = i.registerObj(item)
		}
		return handles, nil
	}
	// Shimmer: string → list via parseList
	items, err := i.parseList(obj.String())
	if err != nil {
		return nil, err
	}
	// Convert parsed items to *Obj and store as ListType
	objItems := make([]*Obj, len(items))
	for idx, h := range items {
		objItems[idx] = i.getObject(h)
	}
	obj.intrep = ListType(objItems)
	return items, nil
}

// GetDict returns the dict representation of an object as handles.
// Performs shimmering: parses string/list representation as dict if needed.
// Returns an error if the value cannot be converted to a dict (odd number of elements).
func (i *Interp) GetDict(h FeatherObj) (map[string]FeatherObj, []string, error) {
	obj := i.getObject(h)
	if obj == nil {
		return nil, nil, fmt.Errorf("nil object")
	}
	// Try to get dict via AsDict (works for DictType)
	if d, err := AsDict(obj); err == nil {
		// Convert map[string]*Obj to map[string]FeatherObj handles
		handles := make(map[string]FeatherObj, len(d.Items))
		for k, v := range d.Items {
			handles[k] = i.registerObj(v)
		}
		return handles, d.Order, nil
	}
	// Shimmer: string/list → dict
	// First get as list (which handles parsing if needed)
	items, err := i.GetList(h)
	if err != nil {
		return nil, nil, err
	}
	// Must have even number of elements
	if len(items)%2 != 0 {
		return nil, nil, fmt.Errorf("missing value to go with key")
	}
	// Build dict
	dictItems := make(map[string]*Obj)
	var dictOrder []string
	for j := 0; j < len(items); j += 2 {
		key := i.GetString(items[j])
		val := i.getObject(items[j+1])
		// If key already exists, update value but keep order position
		if _, exists := dictItems[key]; !exists {
			dictOrder = append(dictOrder, key)
		}
		dictItems[key] = val
	}
	// Cache the parsed dict
	obj.intrep = &DictType{Items: dictItems, Order: dictOrder}
	// Return handles
	handles := make(map[string]FeatherObj, len(dictItems))
	for k, v := range dictItems {
		handles[k] = i.registerObj(v)
	}
	return handles, dictOrder, nil
}

// IsNativeDict returns true if the object has a native dict representation
// (not just convertible to dict via shimmering).
func (i *Interp) IsNativeDict(h FeatherObj) bool {
	obj := i.getObject(h)
	if obj == nil {
		return false
	}
	_, ok := obj.intrep.(*DictType)
	return ok
}

// IsNativeList returns true if the object has a native list representation
// (not just convertible to list via shimmering).
func (i *Interp) IsNativeList(h FeatherObj) bool {
	obj := i.getObject(h)
	if obj == nil {
		return false
	}
	_, ok := obj.intrep.(ListType)
	return ok
}

// parseList parses a TCL list string into a slice of object handles.
func (i *Interp) parseList(s string) ([]FeatherObj, error) {
	var items []FeatherObj
	s = strings.TrimSpace(s)
	if s == "" {
		return items, nil
	}

	pos := 0
	for pos < len(s) {
		// Skip whitespace
		for pos < len(s) && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n') {
			pos++
		}
		if pos >= len(s) {
			break
		}

		var elem string
		if s[pos] == '{' {
			// Braced element
			depth := 1
			start := pos + 1
			pos++
			for pos < len(s) && depth > 0 {
				if s[pos] == '{' {
					depth++
				} else if s[pos] == '}' {
					depth--
				}
				pos++
			}
			if depth != 0 {
				return nil, fmt.Errorf("unmatched brace in list")
			}
			elem = s[start : pos-1]
		} else if s[pos] == '"' {
			// Quoted element
			start := pos + 1
			pos++
			for pos < len(s) && s[pos] != '"' {
				if s[pos] == '\\' && pos+1 < len(s) {
					pos++
				}
				pos++
			}
			if pos >= len(s) {
				return nil, fmt.Errorf("unmatched quote in list")
			}
			elem = s[start:pos]
			pos++ // skip closing quote
		} else {
			// Bare word
			start := pos
			for pos < len(s) && s[pos] != ' ' && s[pos] != '\t' && s[pos] != '\n' {
				pos++
			}
			elem = s[start:pos]
		}
		items = append(items, i.internString(elem))
	}
	return items, nil
}

// SetResult sets the interpreter's result to the given object.
func (i *Interp) SetResult(obj FeatherObj) {
	i.result = obj
}

// SetResultString sets the interpreter's result to a string value.
func (i *Interp) SetResultString(s string) {
	i.result = i.internString(s)
}

// SetErrorString sets the interpreter's result to an error message.
func (i *Interp) SetErrorString(s string) {
	i.result = i.internString(s)
}

// SetVar sets a variable by name to a string value in the current frame.
func (i *Interp) SetVar(name, value string) {
	frame := i.frames[i.active]
	frame.locals.vars[name] = i.registerObj(NewString(value))
}

// GetVar returns the string value of a variable from the current frame, or empty string if not found.
func (i *Interp) GetVar(name string) string {
	frame := i.frames[i.active]
	if val, ok := frame.locals.vars[name]; ok {
		if obj := i.getObject(val); obj != nil {
			return obj.String()
		}
	}
	return ""
}

func getInterp(h C.FeatherInterp) *Interp {
	return cgo.Handle(h).Value().(*Interp)
}

// Keep unused import to ensure cgo is used
var _ = unsafe.Pointer(nil)
