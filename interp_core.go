package feather

/*
#cgo CFLAGS: -I${SRCDIR}/src
#include "feather.h"
#include "host.h"
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

// Eval flags control variable resolution scope during script evaluation.
const (
	// EvalLocal evaluates the script in the current call frame.
	// Variables are resolved in the local scope first, then in enclosing scopes.
	// This is the default behavior for most TCL commands.
	EvalLocal = C.TCL_EVAL_LOCAL

	// EvalGlobal evaluates the script in the global (top-level) scope.
	// Variables are resolved only in the global namespace, ignoring local frames.
	// Use this when a script should not see or modify local variables.
	EvalGlobal = C.TCL_EVAL_GLOBAL
)

// InternalParseStatus matching FeatherParseStatus enum
type InternalParseStatus uint

const (
	InternalParseOK         InternalParseStatus = C.TCL_PARSE_OK
	InternalParseIncomplete InternalParseStatus = C.TCL_PARSE_INCOMPLETE
	InternalParseError      InternalParseStatus = C.TCL_PARSE_ERROR
)

// ParseResultInternal holds the result of parsing a script
type ParseResultInternal struct {
	Status       InternalParseStatus
	Result       string // The interpreter's result string (e.g., "{INCOMPLETE 5 20}")
	ErrorMessage string // For ParseError, the error message from the result list
}

// Handle is the Go type for FeatherHandle
type Handle = uintptr

// FeatherInterp is a handle to an interpreter instance
type FeatherInterp Handle

// FeatherObj is a handle to an object
type FeatherObj Handle

// InternalCommandFunc is the signature for host command implementations.
// Commands receive the interpreter, the command name and a list of argument objects.
//
// # In case of an error, the command should set the interpreter's error information and return ResultError
//
// To return a value, the command should set the interpreter's result value and return ResultOK
//
// Low-level API. May change between versions.
type InternalCommandFunc func(i *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult

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
	vars           map[string]*Obj     // variables stored directly as *Obj (not handles)
	commands       map[string]*Command // commands defined in this namespace
	exportPatterns []string            // patterns for exported commands (e.g., "get*", "set*")
}

// CallFrame represents an execution frame on the call stack.
// Each frame has its own variable environment.
type CallFrame struct {
	cmd    *Obj               // command being evaluated (persistent)
	args   *Obj               // arguments to the command (persistent)
	locals *Namespace         // local variable storage (for global frame, this IS the :: namespace)
	links  map[string]varLink // upvar links: local name -> target variable
	level  int                // frame index on the call stack
	ns     *Namespace         // current namespace context
	line   int                // line number where command was invoked (0 = not set)
	lambda *Obj               // lambda expression for apply frames (nil = not apply)
}

// Procedure represents a user-defined procedure
type Procedure struct {
	name   *Obj // procedure name (persistent)
	params *Obj // parameter list (persistent)
	body   *Obj // procedure body (persistent)
}

// CommandType indicates the type of a command
type InternalCommandType int

const (
	CmdNone    InternalCommandType = 0 // command doesn't exist
	CmdBuiltin InternalCommandType = 1 // it's a builtin command
	CmdProc    InternalCommandType = 2 // it's a user-defined procedure
)

// Command represents an entry in the unified command table
type Command struct {
	cmdType InternalCommandType      // type of command
	builtin C.FeatherBuiltinCmd  // function pointer (only for CmdBuiltin)
	proc    *Procedure       // procedure info (only for CmdProc)
}

// scratchHandleBit is the high bit used to mark scratch arena handles.
// Handles with this bit set belong to the scratch arena (temporary objects).
// Handles without this bit belong to permanent storage (foreign objects, etc.).
const scratchHandleBit FeatherObj = 1 << 63

// isScratchHandle returns true if the handle belongs to the scratch arena.
func isScratchHandle(h FeatherObj) bool {
	return h&scratchHandleBit != 0
}

// resetScratch clears the scratch arena, releasing all temporary objects.
// Called after each top-level eval completes.
func (i *Interp) resetScratch() {
	i.scratch = make(map[FeatherObj]*Obj)
	i.scratchNextID = scratchHandleBit | 1
}

// internStringScratch creates a string object in the scratch arena.
// Use for temporary strings that don't need to persist after eval.
func (i *Interp) internStringScratch(s string) FeatherObj {
	id := i.scratchNextID
	i.scratchNextID++
	i.scratch[id] = i.String(s)
	return id
}

// registerObjScratch stores an *Obj in the scratch arena and returns its handle.
// Use when C code needs a handle to a Go object during eval.
func (i *Interp) registerObjScratch(obj *Obj) FeatherObj {
	if obj == nil {
		return 0
	}
	id := i.scratchNextID
	i.scratchNextID++
	i.scratch[id] = obj
	return id
}

// register adds a Go command to the interpreter (internal).
// The command will be invoked when the C layer doesn't find a builtin or proc.
func (i *Interp) register(name string, fn InternalCommandFunc) {
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
	cmdStr := i.getString(cmd)
	if fn, ok := i.Commands[cmdStr]; ok {
		return fn(i, cmd, args)
	}
	if i.unknownHandler != nil {
		return i.unknownHandler(i, cmd, args)
	}
	i.SetErrorString("invalid command name \"" + cmdStr + "\"")
	return ResultError
}

// SetUnknownHandler sets a handler that is called when a command is not found.
// The handler receives the command name and arguments, and can implement
// custom command resolution (e.g., auto-loading, dynamic dispatch).
// Set to nil to restore default behavior (return error).
func (i *Interp) setUnknownHandler(fn InternalCommandFunc) {
	i.unknownHandler = fn
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

// ParseInternal parses a script string and returns the parse status and result.
// Low-level API. May change between versions.
func (i *Interp) ParseInternal(script string) ParseResultInternal {
	scriptHandle := i.internString(script)

	// Call the C parser
	status := callCParse(i.handle, scriptHandle)

	var resultStr string
	var errorMsg string
	if i.result != nil {
		resultStr = i.objToString(i.result)
		// For parse errors, extract the error message (4th element) directly from the list
		if InternalParseStatus(status) == InternalParseError {
			if listItems, err := asList(i.result); err == nil && len(listItems) >= 4 {
				if msgObj := listItems[3]; msgObj != nil {
					errorMsg = msgObj.String()
				}
			}
		}
	}

	return ParseResultInternal{
		Status:       InternalParseStatus(status),
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
	if listItems, err := asList(obj); err == nil {
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
	if listItems, err := asList(obj); err == nil {
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
				nestedList, _ := asList(itemObj)
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

// resultString returns the result as a string, handling nil.
func (i *Interp) resultString() string {
	if i.result == nil {
		return ""
	}
	return i.result.String()
}

// eval evaluates a script string using the C interpreter (internal).
func (i *Interp) eval(script string) (string, error) {
	scriptHandle := i.internStringScratch(script)

	// Reset scratch arena after eval completes
	defer i.resetScratch()

	// Call the C interpreter
	result := callCEval(i.handle, scriptHandle)

	if result == C.TCL_OK {
		return i.resultString(), nil
	}

	// Handle TCL_RETURN at top level - apply the return options
	if result == C.TCL_RETURN {
		// Get return options and apply the code
		var code C.FeatherResult = C.TCL_OK
		if i.returnOptions != nil {
			if items, err := asList(i.returnOptions); err == nil {
				for j := 0; j+1 < len(items); j += 2 {
					key := items[j].String()
					if key == "-code" {
						if codeVal, err := asInt(items[j+1]); err == nil {
							code = C.FeatherResult(codeVal)
						}
					}
				}
			}
		}
		// Apply the extracted code
		if code == C.TCL_OK {
			return i.resultString(), nil
		}
		if code == C.TCL_ERROR {
			return "", &EvalError{Message: i.resultString()}
		}
		if code == C.TCL_BREAK {
			return "", &EvalError{Message: "invoked \"break\" outside of a loop"}
		}
		if code == C.TCL_CONTINUE {
			return "", &EvalError{Message: "invoked \"continue\" outside of a loop"}
		}
		// For other codes, treat as ok
		return i.resultString(), nil
	}

	// Convert break/continue outside loop to errors at the top level
	if result == C.TCL_BREAK {
		return "", &EvalError{Message: "invoked \"break\" outside of a loop"}
	}
	if result == C.TCL_CONTINUE {
		return "", &EvalError{Message: "invoked \"continue\" outside of a loop"}
	}

	return "", &EvalError{Message: i.resultString()}
}

// Result returns the current result string
func (i *Interp) Result() string {
	if i.result == nil {
		return ""
	}
	return i.result.String()
}

// ResultHandle returns the current result object handle.
// Creates a scratch handle for C code to use.
func (i *Interp) ResultHandle() FeatherObj {
	return i.registerObjScratch(i.result)
}

// EvalError represents an evaluation error
type EvalError struct {
	Message string
}

func (e *EvalError) Error() string {
	return e.Message
}

// internString stores a string in the scratch arena and returns its handle.
// Use internStringPermanent for strings that need to persist after eval.
func (i *Interp) internString(s string) FeatherObj {
	return i.internStringScratch(s)
}

// internStringPermanent stores a string in permanent storage.
// Use for strings that must persist across evals (e.g., namespace paths).
func (i *Interp) internStringPermanent(s string) FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = i.String(s)
	return id
}


// registerObj stores an *Obj in the scratch arena and returns its handle.
// Used when we need to give C a handle to an existing *Obj.
func (i *Interp) registerObj(obj *Obj) FeatherObj {
	return i.registerObjScratch(obj)
}

// registerObjPermanent stores an *Obj in permanent storage.
// Use for objects that must persist across evals (e.g., foreign objects).
func (i *Interp) registerObjPermanent(obj *Obj) FeatherObj {
	if obj == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = obj
	return id
}

// NewForeignHandle creates a new foreign object with the given type name and Go value.
// The string representation is generated as "<TypeName:id>".
// Returns the handle to the new foreign object.
// Foreign objects are stored in permanent storage (not scratch) for explicit lifecycle.
func (i *Interp) NewForeignHandle(typeName string, value any) FeatherObj {
	id := i.nextID
	i.nextID++
	obj := &Obj{intrep: &ForeignType{TypeName: typeName, Value: value}, interp: i}
	// Override the string representation to include the handle ID
	obj.bytes = fmt.Sprintf("<%s:%d>", typeName, id)
	// Use permanent storage - foreign objects have explicit lifecycle management
	i.objects[id] = obj
	return id
}

// NewForeignHandleNamed creates a new foreign object with a custom string representation.
// This is used by the C API to create foreign objects with specific handle names.
// Returns the *Obj pointer for use in feather.OK() and the FeatherObj handle.
func (i *Interp) NewForeignHandleNamed(typeName, handleName string, value any) (*Obj, FeatherObj) {
	obj := &Obj{intrep: &ForeignType{TypeName: typeName, Value: value}, interp: i}
	obj.bytes = handleName
	// Store in permanent storage
	id := i.nextID
	i.nextID++
	i.objects[id] = obj
	return obj, id
}

// IsForeignHandle returns true if the object is a foreign object.
// Also checks if the string representation is a foreign handle name.
func (i *Interp) IsForeignHandle(h FeatherObj) bool {
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
func (i *Interp) getForeignType(h FeatherObj) string {
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
func (i *Interp) getForeignValue(h FeatherObj) any {
	if obj := i.getObject(h); obj != nil {
		if ft, ok := obj.intrep.(*ForeignType); ok {
			return ft.Value
		}
	}
	return nil
}

// getObject retrieves an object by handle from either arena.
func (i *Interp) getObject(h FeatherObj) *Obj {
	if h == 0 {
		return nil
	}
	if isScratchHandle(h) {
		return i.scratch[h]
	}
	return i.objects[h]
}

// handleForObj returns a FeatherObj handle for a *Obj, registering it if needed.
// Used when bridging from public *Obj API to internal handle-based operations.
func (i *Interp) handleForObj(o *Obj) FeatherObj {
	if o == nil {
		return 0
	}
	return i.registerObj(o)
}

// objForHandle returns the *Obj for a FeatherObj handle.
// Used when bridging from internal handle-based operations to public *Obj API.
func (i *Interp) objForHandle(h FeatherObj) *Obj {
	if h == 0 {
		return nil
	}
	return i.getObject(h)
}

// GetString returns the string representation of an object.
// Performs shimmering: converts int/double/list/dict representations to string as needed.
func (i *Interp) getString(h FeatherObj) string {
	if obj := i.getObject(h); obj != nil {
		return obj.String()
	}
	return ""
}


// GetInt returns the integer representation of an object.
// Performs shimmering: parses string representation as integer if needed.
// Returns an error if the value cannot be converted to an integer.
func (i *Interp) getInt(h FeatherObj) (int64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	return asInt(obj)
}

// GetDouble returns the floating-point representation of an object.
// Performs shimmering: parses string representation as double if needed.
// Returns an error if the value cannot be converted to a double.
func (i *Interp) getDouble(h FeatherObj) (float64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	return asDouble(obj)
}

// GetList returns the list representation of an object as handles.
// Performs shimmering: parses string representation as list if needed.
// Returns an error if the value cannot be converted to a list.
func (i *Interp) getList(h FeatherObj) ([]FeatherObj, error) {
	obj := i.getObject(h)
	if obj == nil {
		return nil, fmt.Errorf("nil object")
	}
	// Try to get list via asList (works for ListType)
	if list, err := asList(obj); err == nil {
		// Convert []*Obj to []FeatherObj handles
		handles := make([]FeatherObj, len(list))
		for idx, item := range list {
			handles[idx] = i.registerObj(item)
		}
		return handles, nil
	}
	// Shimmer: string → list via C's feather_list_parse_obj
	strHandle := i.internString(obj.String())
	listHandle := FeatherObj(C.feather_list_parse_obj(nil, C.FeatherInterp(i.handle), C.FeatherObj(strHandle)))

	// Check for parse error (nil return means error, message in result)
	if listHandle == 0 {
		// Get error message from interp result
		if i.result != nil {
			return nil, fmt.Errorf("%s", i.result.String())
		}
		return nil, fmt.Errorf("failed to parse list")
	}

	// Get the list items from the parsed result
	listObj := i.getObject(listHandle)
	if listObj == nil {
		return nil, fmt.Errorf("failed to parse list")
	}
	items, err := asList(listObj)
	if err != nil {
		return nil, err
	}

	// Convert []*Obj to []FeatherObj handles
	handles := make([]FeatherObj, len(items))
	for idx, item := range items {
		handles[idx] = i.registerObj(item)
	}

	// Store as ListType on the original object for future lookups
	obj.intrep = ListType(items)
	return handles, nil
}

// GetDict returns the dict representation of an object as handles.
// Performs shimmering: parses string/list representation as dict if needed.
// Returns an error if the value cannot be converted to a dict (odd number of elements).
func (i *Interp) getDict(h FeatherObj) (map[string]FeatherObj, []string, error) {
	obj := i.getObject(h)
	if obj == nil {
		return nil, nil, fmt.Errorf("nil object")
	}
	// Try to get dict via asDict (works for DictType)
	if d, err := asDict(obj); err == nil {
		// Convert map[string]*Obj to map[string]FeatherObj handles
		handles := make(map[string]FeatherObj, len(d.Items))
		for k, v := range d.Items {
			handles[k] = i.registerObj(v)
		}
		return handles, d.Order, nil
	}
	// Shimmer: string/list → dict
	// First get as list (which handles parsing if needed)
	items, err := i.getList(h)
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
		key := i.getString(items[j])
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

// ListLen returns the number of elements in a list object.
//
// If the object is not already a list, it parses the string representation.
// Returns 0 if the object is nil, empty, or cannot be converted to a list.
//
// Note: For dicts, this returns the number of key-value pairs times 2
// (the list representation length).
func (i *Interp) ListLen(h FeatherObj) int {
	items, err := i.getList(h)
	if err != nil {
		return 0
	}
	return len(items)
}

// ListIndex returns the element at the given zero-based index in a list.
//
// If the object is not already a list, it parses the string representation.
// Returns 0 (invalid handle) if:
//   - The index is negative or out of bounds
//   - The object cannot be converted to a list
//   - The object handle is invalid
func (i *Interp) ListIndex(h FeatherObj, idx int) FeatherObj {
	items, err := i.getList(h)
	if err != nil || idx < 0 || idx >= len(items) {
		return 0
	}
	return items[idx]
}

// DictGet retrieves the value for a key in a dict object.
//
// If the object is not already a dict, it attempts to convert from list
// representation (must have even number of elements).
//
// Returns (handle, true) if the key exists, (0, false) otherwise.
// Also returns (0, false) if the object cannot be converted to a dict.
func (i *Interp) DictGet(h FeatherObj, key string) (FeatherObj, bool) {
	items, _, err := i.getDict(h)
	if err != nil {
		return 0, false
	}
	val, ok := items[key]
	return val, ok
}

// DictKeys returns all keys of a dict object in insertion order.
//
// If the object is not already a dict, it attempts to convert from list
// representation.
//
// Returns nil if the object cannot be converted to a dict.
func (i *Interp) DictKeys(h FeatherObj) []string {
	_, order, err := i.getDict(h)
	if err != nil {
		return nil
	}
	return order
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

// SetResult sets the interpreter's result to the given object handle.
// The object is retrieved from the arena and stored directly.
func (i *Interp) SetResult(obj FeatherObj) {
	i.result = i.getObject(obj)
}

// SetResultObj sets the interpreter's result to the given *Obj directly.
func (i *Interp) SetResultObj(obj *Obj) {
	i.result = obj
}

// SetResultString sets the interpreter's result to a string value.
func (i *Interp) SetResultString(s string) {
	i.result = i.String(s)
}

// SetErrorString sets the interpreter's result to an error message.
func (i *Interp) SetErrorString(s string) {
	i.result = i.String(s)
}

// SetError sets the interpreter's result to the given object handle (for error results).
// This is symmetric with SetResult but semantically indicates an error value.
func (i *Interp) SetError(obj FeatherObj) {
	i.result = i.getObject(obj)
}

// setVar sets a variable by name to a string value in the current frame (internal).
func (i *Interp) setVar(name, value string) {
	frame := i.frames[i.active]
	frame.locals.vars[name] = i.String(value)
}

// GetVar returns the string value of a variable from the current frame, or empty string if not found.
func (i *Interp) GetVar(name string) string {
	frame := i.frames[i.active]
	if val, ok := frame.locals.vars[name]; ok && val != nil {
		return val.String()
	}
	return ""
}

// GetVarHandle returns the object handle for a variable, preserving its type.
// Returns 0 if the variable is not found.
func (i *Interp) GetVarHandle(name string) FeatherObj {
	frame := i.frames[i.active]
	if val, ok := frame.locals.vars[name]; ok && val != nil {
		return i.registerObjScratch(val)
	}
	return 0
}

func getInterp(h C.FeatherInterp) *Interp {
	return cgo.Handle(h).Value().(*Interp)
}

// storeBuilder stores a string builder and returns a handle for it.
func (i *Interp) storeBuilder(b *strings.Builder) FeatherObj {
	id := i.nextID
	i.nextID++
	i.builders[id] = b
	return id
}

// getBuilder retrieves a string builder by handle.
func (i *Interp) getBuilder(h FeatherObj) *strings.Builder {
	return i.builders[h]
}

// releaseBuilder removes a builder from storage.
func (i *Interp) releaseBuilder(h FeatherObj) {
	delete(i.builders, h)
}

// Keep unused import to ensure cgo is used
var _ = unsafe.Pointer(nil)
