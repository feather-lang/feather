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

// EvalFlags matching FeatherEvalFlags enum
const (
	EvalLocal  = C.TCL_EVAL_LOCAL
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
type InternalCommandFunc func(i *InternalInterp, cmd FeatherObj, args []FeatherObj) FeatherResult

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

// InternalInterp represents a TCL interpreter instance (low-level)
type InternalInterp struct {
	handle         FeatherInterp
	objects        map[FeatherObj]*Obj // permanent storage (foreign objects)
	scratch        map[FeatherObj]*Obj // scratch arena (temporary objects, reset after eval)
	scratchNextID  FeatherObj          // next ID for scratch arena (has high bit set)
	globalNS       FeatherObj              // global namespace object (FeatherObj handle for "::")
	namespaces     map[string]*Namespace // namespace path -> Namespace
	globalNamespace *Namespace           // the global namespace "::"
	nextID         FeatherObj            // next ID for permanent storage (no high bit)
	result         *Obj                  // current result (persistent, not handle)
	returnOptions  *Obj                  // options from the last return command (persistent)
	frames         []*CallFrame // call stack (frame 0 is global)
	active         int          // currently active frame index
	recursionLimit int          // maximum call stack depth (0 means use default)
	scriptPath     *Obj             // current script file being executed (nil = none)

	// builders stores string builders for the byte-at-a-time string API.
	// Used by goStringBuilderNew/AppendByte/AppendObj/Finish.
	builders map[FeatherObj]*strings.Builder

	// Commands holds registered Go command implementations.
	// These are dispatched via the unknown handler when the C layer
	// doesn't find a builtin or proc.
	Commands map[string]InternalCommandFunc

	// ForeignRegistry stores foreign type definitions for the high-level API.
	// Set by DefineType when registering foreign types.
	ForeignRegistry *ForeignRegistry

	// unknownHandler is called when a command is not found in Commands.
	// If nil, the default behavior is to return an error.
	unknownHandler InternalCommandFunc
}

// NewInternalInterp creates a new interpreter
func NewInternalInterp() *InternalInterp {
	interp := &InternalInterp{
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
	// Use permanent storage since this must persist across evals
	interp.globalNS = interp.internStringPermanent("::")
	// Initialize the C interpreter (registers builtins)
	callCInterpInit(interp.handle)
	return interp
}

// Close releases resources associated with the interpreter.
// Must be called when the interpreter is no longer needed.
func (i *InternalInterp) Close() {
	cgo.Handle(i.handle).Delete()
}

// isScratchHandle returns true if the handle belongs to the scratch arena.
func isScratchHandle(h FeatherObj) bool {
	return h&scratchHandleBit != 0
}

// resetScratch clears the scratch arena, releasing all temporary objects.
// Called after each top-level eval completes.
func (i *InternalInterp) resetScratch() {
	i.scratch = make(map[FeatherObj]*Obj)
	i.scratchNextID = scratchHandleBit | 1
}

// internStringScratch creates a string object in the scratch arena.
// Use for temporary strings that don't need to persist after eval.
func (i *InternalInterp) internStringScratch(s string) FeatherObj {
	id := i.scratchNextID
	i.scratchNextID++
	i.scratch[id] = NewStringObj(s)
	return id
}

// registerObjScratch stores an *Obj in the scratch arena and returns its handle.
// Use when C code needs a handle to a Go object during eval.
func (i *InternalInterp) registerObjScratch(obj *Obj) FeatherObj {
	if obj == nil {
		return 0
	}
	id := i.scratchNextID
	i.scratchNextID++
	i.scratch[id] = obj
	return id
}

// Register adds a Go command to the interpreter.
// The command will be invoked when the C layer doesn't find a builtin or proc.
func (i *InternalInterp) Register(name string, fn InternalCommandFunc) {
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
func (i *InternalInterp) dispatch(cmd FeatherObj, args []FeatherObj) FeatherResult {
	cmdStr := i.GetString(cmd)
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
func (i *InternalInterp) SetUnknownHandler(fn InternalCommandFunc) {
	i.unknownHandler = fn
}

// DefaultRecursionLimit is the default maximum call stack depth.
const DefaultRecursionLimit = 1000

// SetRecursionLimit sets the maximum call stack depth.
// If limit is 0 or negative, the default limit (1000) is used.
func (i *InternalInterp) SetRecursionLimit(limit int) {
	if limit <= 0 {
		i.recursionLimit = DefaultRecursionLimit
	} else {
		i.recursionLimit = limit
	}
}

// getRecursionLimit returns the effective recursion limit.
func (i *InternalInterp) getRecursionLimit() int {
	if i.recursionLimit <= 0 {
		return DefaultRecursionLimit
	}
	return i.recursionLimit
}

// Handle returns the interpreter's handle
func (i *InternalInterp) Handle() FeatherInterp {
	return i.handle
}

// Parse parses a script string and returns the parse status and result.
// This is a convenience alias for ParseInternal.
func (i *InternalInterp) Parse(script string) ParseResultInternal {
	return i.ParseInternal(script)
}

// ParseInternal parses a script string and returns the parse status and result.
func (i *InternalInterp) ParseInternal(script string) ParseResultInternal {
	scriptHandle := i.internString(script)

	// Call the C parser
	status := callCParse(i.handle, scriptHandle)

	var resultStr string
	var errorMsg string
	if i.result != nil {
		resultStr = i.objToString(i.result)
		// For parse errors, extract the error message (4th element) directly from the list
		if InternalParseStatus(status) == InternalParseError {
			if listItems, err := AsList(i.result); err == nil && len(listItems) >= 4 {
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
func (i *InternalInterp) objToString(obj *Obj) string {
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
func (i *InternalInterp) objToValue(obj *Obj) string {
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
func (i *InternalInterp) dictObjToValue(obj *Obj) string {
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

// resultString returns the result as a string, handling nil.
func (i *InternalInterp) resultString() string {
	if i.result == nil {
		return ""
	}
	return i.result.String()
}

// Eval evaluates a script string using the C interpreter
func (i *InternalInterp) Eval(script string) (string, error) {
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
			if items, err := AsList(i.returnOptions); err == nil {
				for j := 0; j+1 < len(items); j += 2 {
					key := items[j].String()
					if key == "-code" {
						if codeVal, err := AsInt(items[j+1]); err == nil {
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
func (i *InternalInterp) Result() string {
	if i.result == nil {
		return ""
	}
	return i.result.String()
}

// ResultHandle returns the current result object handle.
// Creates a scratch handle for C code to use.
func (i *InternalInterp) ResultHandle() FeatherObj {
	return i.registerObjScratch(i.result)
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
func (i *InternalInterp) EvalTyped(script string) (ResultInfo, error) {
	_, err := i.Eval(script)
	if err != nil {
		return ResultInfo{}, err
	}
	return i.objToResultInfo(i.result), nil
}

// objToResultInfo returns information about an *Obj for inspection.
func (i *InternalInterp) objToResultInfo(obj *Obj) ResultInfo {
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

// internString stores a string in the scratch arena and returns its handle.
// Use internStringPermanent for strings that need to persist after eval.
func (i *InternalInterp) internString(s string) FeatherObj {
	return i.internStringScratch(s)
}

// internStringPermanent stores a string in permanent storage.
// Use for strings that must persist across evals (e.g., namespace paths).
func (i *InternalInterp) internStringPermanent(s string) FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = NewStringObj(s)
	return id
}

// InternString stores a string and returns its handle.
func (i *InternalInterp) InternString(s string) FeatherObj {
	return i.internString(s)
}

// registerObj stores an *Obj in the scratch arena and returns its handle.
// Used when we need to give C a handle to an existing *Obj.
func (i *InternalInterp) registerObj(obj *Obj) FeatherObj {
	return i.registerObjScratch(obj)
}

// registerObjPermanent stores an *Obj in permanent storage.
// Use for objects that must persist across evals (e.g., foreign objects).
func (i *InternalInterp) registerObjPermanent(obj *Obj) FeatherObj {
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
func (i *InternalInterp) createListFromHandles(handles []FeatherObj) FeatherObj {
	items := make([]*Obj, len(handles))
	for idx, h := range handles {
		items[idx] = i.getObject(h)
	}
	return i.registerObj(&Obj{intrep: ListType(items)})
}

// getListItems returns the list items as *Obj slice.
// Performs shimmering if needed. Returns nil on error.
func (i *InternalInterp) getListItems(h FeatherObj) []*Obj {
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
func (i *InternalInterp) setListItems(h FeatherObj, items []*Obj) {
	obj := i.getObject(h)
	if obj == nil {
		return
	}
	obj.intrep = ListType(items)
	obj.invalidate()
}

// NewForeignHandle creates a new foreign object with the given type name and Go value.
// The string representation is generated as "<TypeName:id>".
// Returns the handle to the new foreign object.
// Foreign objects are stored in permanent storage (not scratch) for explicit lifecycle.
func (i *InternalInterp) NewForeignHandle(typeName string, value any) FeatherObj {
	id := i.nextID
	i.nextID++
	obj := NewForeignObj(typeName, value)
	// Override the string representation to include the handle ID
	obj.bytes = fmt.Sprintf("<%s:%d>", typeName, id)
	// Use permanent storage - foreign objects have explicit lifecycle management
	i.objects[id] = obj
	return id
}

// IsForeignHandle returns true if the object is a foreign object.
// Also checks if the string representation is a foreign handle name.
func (i *InternalInterp) IsForeignHandle(h FeatherObj) bool {
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
func (i *InternalInterp) GetForeignType(h FeatherObj) string {
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
func (i *InternalInterp) GetForeignValue(h FeatherObj) any {
	if obj := i.getObject(h); obj != nil {
		if ft, ok := obj.intrep.(*ForeignType); ok {
			return ft.Value
		}
	}
	return nil
}

// getObject retrieves an object by handle from either arena.
func (i *InternalInterp) getObject(h FeatherObj) *Obj {
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
func (i *InternalInterp) handleForObj(o *Obj) FeatherObj {
	if o == nil {
		return 0
	}
	return i.registerObj(o)
}

// objForHandle returns the *Obj for a FeatherObj handle.
// Used when bridging from internal handle-based operations to public *Obj API.
func (i *InternalInterp) objForHandle(h FeatherObj) *Obj {
	if h == 0 {
		return nil
	}
	return i.getObject(h)
}

// GetString returns the string representation of an object.
// Performs shimmering: converts int/double/list/dict representations to string as needed.
func (i *InternalInterp) GetString(h FeatherObj) string {
	if obj := i.getObject(h); obj != nil {
		return obj.String()
	}
	return ""
}

// DisplayName returns a user-friendly name for a command.
// Strips the leading "::" for global namespace commands (e.g., "::set" -> "set")
// but preserves the full path for nested namespaces (e.g., "::foo::bar" stays as-is).
func (i *InternalInterp) DisplayName(name string) string {
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
func (i *InternalInterp) GetInt(h FeatherObj) (int64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	return AsInt(obj)
}

// GetDouble returns the floating-point representation of an object.
// Performs shimmering: parses string representation as double if needed.
// Returns an error if the value cannot be converted to a double.
func (i *InternalInterp) GetDouble(h FeatherObj) (float64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	return AsDouble(obj)
}

// GetList returns the list representation of an object as handles.
// Performs shimmering: parses string representation as list if needed.
// Returns an error if the value cannot be converted to a list.
func (i *InternalInterp) GetList(h FeatherObj) ([]FeatherObj, error) {
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
	items, err := AsList(listObj)
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
func (i *InternalInterp) GetDict(h FeatherObj) (map[string]FeatherObj, []string, error) {
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

// ListLen returns the number of elements in a list object.
//
// If the object is not already a list, it parses the string representation.
// Returns 0 if the object is nil, empty, or cannot be converted to a list.
//
// Note: For dicts, this returns the number of key-value pairs times 2
// (the list representation length).
func (i *InternalInterp) ListLen(h FeatherObj) int {
	items, err := i.GetList(h)
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
func (i *InternalInterp) ListIndex(h FeatherObj, idx int) FeatherObj {
	items, err := i.GetList(h)
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
func (i *InternalInterp) DictGet(h FeatherObj, key string) (FeatherObj, bool) {
	items, _, err := i.GetDict(h)
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
func (i *InternalInterp) DictKeys(h FeatherObj) []string {
	_, order, err := i.GetDict(h)
	if err != nil {
		return nil
	}
	return order
}

// IsNativeDict returns true if the object has a native dict representation
// (not just convertible to dict via shimmering).
func (i *InternalInterp) IsNativeDict(h FeatherObj) bool {
	obj := i.getObject(h)
	if obj == nil {
		return false
	}
	_, ok := obj.intrep.(*DictType)
	return ok
}

// IsNativeList returns true if the object has a native list representation
// (not just convertible to list via shimmering).
func (i *InternalInterp) IsNativeList(h FeatherObj) bool {
	obj := i.getObject(h)
	if obj == nil {
		return false
	}
	_, ok := obj.intrep.(ListType)
	return ok
}



// SetResult sets the interpreter's result to the given object handle.
// The object is retrieved from the arena and stored directly.
func (i *InternalInterp) SetResult(obj FeatherObj) {
	i.result = i.getObject(obj)
}

// SetResultObj sets the interpreter's result to the given *Obj directly.
func (i *InternalInterp) SetResultObj(obj *Obj) {
	i.result = obj
}

// SetResultString sets the interpreter's result to a string value.
func (i *InternalInterp) SetResultString(s string) {
	i.result = NewStringObj(s)
}

// SetErrorString sets the interpreter's result to an error message.
func (i *InternalInterp) SetErrorString(s string) {
	i.result = NewStringObj(s)
}

// SetError sets the interpreter's result to the given object handle (for error results).
// This is symmetric with SetResult but semantically indicates an error value.
func (i *InternalInterp) SetError(obj FeatherObj) {
	i.result = i.getObject(obj)
}

// SetVar sets a variable by name to a string value in the current frame.
func (i *InternalInterp) SetVar(name, value string) {
	frame := i.frames[i.active]
	frame.locals.vars[name] = NewStringObj(value)
}

// GetVar returns the string value of a variable from the current frame, or empty string if not found.
func (i *InternalInterp) GetVar(name string) string {
	frame := i.frames[i.active]
	if val, ok := frame.locals.vars[name]; ok && val != nil {
		return val.String()
	}
	return ""
}

// GetVarHandle returns the object handle for a variable, preserving its type.
// Returns 0 if the variable is not found.
func (i *InternalInterp) GetVarHandle(name string) FeatherObj {
	frame := i.frames[i.active]
	if val, ok := frame.locals.vars[name]; ok && val != nil {
		return i.registerObjScratch(val)
	}
	return 0
}

func getInternalInterp(h C.FeatherInterp) *InternalInterp {
	return cgo.Handle(h).Value().(*InternalInterp)
}

// storeBuilder stores a string builder and returns a handle for it.
func (i *InternalInterp) storeBuilder(b *strings.Builder) FeatherObj {
	id := i.nextID
	i.nextID++
	i.builders[id] = b
	return id
}

// getBuilder retrieves a string builder by handle.
func (i *InternalInterp) getBuilder(h FeatherObj) *strings.Builder {
	return i.builders[h]
}

// releaseBuilder removes a builder from storage.
func (i *InternalInterp) releaseBuilder(h FeatherObj) {
	delete(i.builders, h)
}

// Keep unused import to ensure cgo is used
var _ = unsafe.Pointer(nil)
