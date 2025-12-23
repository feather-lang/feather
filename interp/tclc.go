package interp

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#include "tclc.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"runtime/cgo"
	"strconv"
	"strings"
	"sync"
	"unsafe"
)

type TclResult uint

// Result codes matching TclResult enum
const (
	ResultOK       TclResult = C.TCL_OK
	ResultError    TclResult = C.TCL_ERROR
	ResultReturn   TclResult = C.TCL_RETURN
	ResultBreak    TclResult = C.TCL_BREAK
	ResultContinue TclResult = C.TCL_CONTINUE
)

// EvalFlags matching TclEvalFlags enum
const (
	EvalLocal  = C.TCL_EVAL_LOCAL
	EvalGlobal = C.TCL_EVAL_GLOBAL
)

// ParseStatus matching TclParseStatus enum
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

// Handle is the Go type for TclHandle
type Handle = uintptr

// TclInterp is a handle to an interpreter instance
type TclInterp Handle

// TclObj is a handle to an object
type TclObj Handle

// CommandFunc is the signature for host command implementations.
// Commands receive the interpreter, the command name and a list of argument objects.
//
// # In case of an error, the command should set the interpreter's error information and return ResultError
//
// To return a value, the command should set the interpreter's result value and return ResultOK
type CommandFunc func(i *Interp, cmd TclObj, args []TclObj) TclResult

// varLink represents a link to a variable in another frame (for upvar)
type varLink struct {
	targetLevel int    // frame level where the target variable lives
	targetName  string // name of the variable in the target frame
}

// CallFrame represents an execution frame on the call stack.
// Each frame has its own variable environment.
type CallFrame struct {
	cmd   TclObj            // command being evaluated
	args  TclObj            // arguments to the command
	vars  map[string]TclObj // local variable storage
	links map[string]varLink // upvar links: local name -> target variable
	level int               // frame index on the call stack
}

// Procedure represents a user-defined procedure
type Procedure struct {
	name   TclObj
	params TclObj
	body   TclObj
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
	builtin C.TclBuiltinCmd  // function pointer (only for CmdBuiltin)
	proc    *Procedure       // procedure info (only for CmdProc)
}

// Interp represents a TCL interpreter instance
type Interp struct {
	handle         TclInterp
	objects        map[TclObj]*Object
	commands       map[string]*Command // unified command table
	globalNS       TclObj              // global namespace object
	nextID         TclObj
	result         TclObj
	returnOptions  TclObj       // options from the last return command
	frames         []*CallFrame // call stack (frame 0 is global)
	active         int          // currently active frame index
	recursionLimit int          // maximum call stack depth (0 means use default)
	mu             sync.Mutex

	// UnknownHandler is called when an unknown command is invoked.
	UnknownHandler CommandFunc
}

// Object represents a TCL object
type Object struct {
	stringVal string
	cstr      *C.char // cached C string for passing to C code
	intVal    int64
	isInt     bool
	dblVal    float64
	isDouble  bool
	listItems []TclObj
	isList    bool
}

// NewInterp creates a new interpreter
func NewInterp() *Interp {
	interp := &Interp{
		objects:  make(map[TclObj]*Object),
		commands: make(map[string]*Command),
		nextID:   1,
	}
	// Initialize the global frame (frame 0)
	globalFrame := &CallFrame{
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: 0,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0
	// Use cgo.Handle to allow C callbacks to find this interpreter
	interp.handle = TclInterp(cgo.NewHandle(interp))
	// Create the global namespace object
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

// DefaultRecursionLimit is the default maximum call stack depth.
const DefaultRecursionLimit = 1000

// SetRecursionLimit sets the maximum call stack depth.
// If limit is 0 or negative, the default limit (1000) is used.
func (i *Interp) SetRecursionLimit(limit int) {
	i.mu.Lock()
	defer i.mu.Unlock()
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
func (i *Interp) Handle() TclInterp {
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
		resultStr = i.listToString(obj)
		// For parse errors, extract the error message (4th element) directly from the list
		if ParseStatus(status) == ParseError && obj.isList && len(obj.listItems) >= 4 {
			if msgObj := i.getObject(obj.listItems[3]); msgObj != nil {
				errorMsg = msgObj.stringVal
			}
		}
	}

	return ParseResult{
		Status:       ParseStatus(status),
		Result:       resultStr,
		ErrorMessage: errorMsg,
	}
}

// listToString converts a list object to its TCL string representation for display.
// This includes outer braces for non-empty lists (used for parse result display).
func (i *Interp) listToString(obj *Object) string {
	if obj == nil {
		return ""
	}
	if !obj.isList {
		if obj.isInt {
			return fmt.Sprintf("%d", obj.intVal)
		}
		return obj.stringVal
	}
	// Build TCL list representation: {elem1 elem2 ...}
	var result string
	for idx, itemHandle := range obj.listItems {
		itemObj := i.getObject(itemHandle)
		if itemObj == nil {
			continue
		}
		if idx > 0 {
			result += " "
		}
		// Handle different object types
		if itemObj.isInt {
			result += fmt.Sprintf("%d", itemObj.intVal)
		} else if itemObj.isList {
			result += i.listToString(itemObj)
		} else {
			// Quote strings that contain spaces
			if strings.ContainsAny(itemObj.stringVal, " \t\n") {
				result += "{" + itemObj.stringVal + "}"
			} else {
				result += itemObj.stringVal
			}
		}
	}
	if len(obj.listItems) > 0 {
		result = "{" + result + "}"
	}
	return result
}

// listToValue converts a list object to its TCL value string representation.
// This does NOT include outer braces (used when returning list as a value).
func (i *Interp) listToValue(obj *Object) string {
	if obj == nil {
		return ""
	}
	if !obj.isList {
		if obj.isInt {
			return fmt.Sprintf("%d", obj.intVal)
		}
		return obj.stringVal
	}
	// Build TCL list value: elem1 elem2 ...
	// Elements with spaces are braced, but the list itself is not wrapped
	var result string
	for idx, itemHandle := range obj.listItems {
		itemObj := i.getObject(itemHandle)
		if itemObj == nil {
			continue
		}
		if idx > 0 {
			result += " "
		}
		// Handle different object types
		if itemObj.isInt {
			result += fmt.Sprintf("%d", itemObj.intVal)
		} else if itemObj.isList {
			// Nested lists need to be braced
			nested := i.listToValue(itemObj)
			if len(itemObj.listItems) > 0 || strings.ContainsAny(nested, " \t\n") {
				result += "{" + nested + "}"
			} else {
				result += nested
			}
		} else {
			// Quote strings that contain spaces or special chars
			if strings.ContainsAny(itemObj.stringVal, " \t\n{}") {
				result += "{" + itemObj.stringVal + "}"
			} else {
				result += itemObj.stringVal
			}
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
		var code C.TclResult = C.TCL_OK
		if i.returnOptions != 0 {
			items, err := i.GetList(i.returnOptions)
			if err == nil {
				for j := 0; j+1 < len(items); j += 2 {
					key := i.GetString(items[j])
					if key == "-code" {
						if codeVal, err := i.GetInt(items[j+1]); err == nil {
							code = C.TclResult(codeVal)
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

// EvalError represents an evaluation error
type EvalError struct {
	Message string
}

func (e *EvalError) Error() string {
	return e.Message
}

// internString stores a string and returns its handle
func (i *Interp) internString(s string) TclObj {
	i.mu.Lock()
	defer i.mu.Unlock()

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{stringVal: s}
	return id
}

// InternString stores a string and returns its handle.
func (i *Interp) InternString(s string) TclObj {
	return i.internString(s)
}

// getObject retrieves an object by handle
func (i *Interp) getObject(h TclObj) *Object {
	i.mu.Lock()
	defer i.mu.Unlock()
	return i.objects[h]
}

// GetString returns the string representation of an object.
// Performs shimmering: converts int/double/list representations to string as needed.
func (i *Interp) GetString(h TclObj) string {
	if obj := i.getObject(h); obj != nil {
		// Shimmer: int → string
		if obj.isInt && obj.stringVal == "" {
			obj.stringVal = fmt.Sprintf("%d", obj.intVal)
		}
		// Shimmer: double → string
		if obj.isDouble && obj.stringVal == "" {
			obj.stringVal = strconv.FormatFloat(obj.dblVal, 'g', -1, 64)
		}
		// Shimmer: list → string (use listToValue for proper TCL semantics)
		if obj.isList && obj.stringVal == "" {
			obj.stringVal = i.listToValue(obj)
		}
		return obj.stringVal
	}
	return ""
}

// GetInt returns the integer representation of an object.
// Performs shimmering: parses string representation as integer if needed.
// Returns an error if the value cannot be converted to an integer.
func (i *Interp) GetInt(h TclObj) (int64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	// Already an integer
	if obj.isInt {
		return obj.intVal, nil
	}
	// Shimmer from double if available
	if obj.isDouble {
		obj.intVal = int64(obj.dblVal)
		obj.isInt = true
		return obj.intVal, nil
	}
	// Shimmer: string → int
	val, err := strconv.ParseInt(obj.stringVal, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("expected integer but got %q", obj.stringVal)
	}
	// Cache the parsed value
	obj.intVal = val
	obj.isInt = true
	return val, nil
}

// GetDouble returns the floating-point representation of an object.
// Performs shimmering: parses string representation as double if needed.
// Returns an error if the value cannot be converted to a double.
func (i *Interp) GetDouble(h TclObj) (float64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	// Already a double
	if obj.isDouble {
		return obj.dblVal, nil
	}
	// Shimmer from int if available
	if obj.isInt {
		obj.dblVal = float64(obj.intVal)
		obj.isDouble = true
		return obj.dblVal, nil
	}
	// Shimmer: string → double
	val, err := strconv.ParseFloat(obj.stringVal, 64)
	if err != nil {
		return 0, fmt.Errorf("expected floating-point number but got %q", obj.stringVal)
	}
	// Cache the parsed value
	obj.dblVal = val
	obj.isDouble = true
	return val, nil
}

// GetList returns the list representation of an object.
// Performs shimmering: parses string representation as list if needed.
// Returns an error if the value cannot be converted to a list.
func (i *Interp) GetList(h TclObj) ([]TclObj, error) {
	obj := i.getObject(h)
	if obj == nil {
		return nil, fmt.Errorf("nil object")
	}
	// Already a list
	if obj.isList {
		return obj.listItems, nil
	}
	// Shimmer: string → list
	// Parse the string as a TCL list
	items, err := i.parseList(obj.stringVal)
	if err != nil {
		return nil, err
	}
	// Cache the parsed list
	obj.listItems = items
	obj.isList = true
	return items, nil
}

// parseList parses a TCL list string into a slice of object handles.
func (i *Interp) parseList(s string) ([]TclObj, error) {
	var items []TclObj
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
func (i *Interp) SetResult(obj TclObj) {
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
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	frame.vars[name] = i.nextID
	i.objects[i.nextID] = &Object{stringVal: value}
	i.nextID++
}

// GetVar returns the string value of a variable from the current frame, or empty string if not found.
func (i *Interp) GetVar(name string) string {
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	if val, ok := frame.vars[name]; ok {
		if obj := i.objects[val]; obj != nil {
			return obj.stringVal
		}
	}
	return ""
}

func getInterp(h C.TclInterp) *Interp {
	return cgo.Handle(h).Value().(*Interp)
}

// Keep unused import to ensure cgo is used
var _ = unsafe.Pointer(nil)
