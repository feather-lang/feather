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
	Status ParseStatus
	Result string // The interpreter's result string (e.g., "{INCOMPLETE 5 20}")
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

// Interp represents a TCL interpreter instance
type Interp struct {
	handle  TclInterp
	objects map[TclObj]*Object
	nextID  TclObj
	result  TclObj
	mu      sync.Mutex

	// UnknownHandler is called when an unknown command is invoked.
	UnknownHandler CommandFunc
}

// Object represents a TCL object
type Object struct {
	stringVal string
	cstr      *C.char // cached C string for passing to C code
	intVal    int64
	isInt     bool
	listItems []TclObj
	isList    bool
}

// NewInterp creates a new interpreter
func NewInterp() *Interp {
	interp := &Interp{
		objects: make(map[TclObj]*Object),
		nextID:  1,
	}
	// Use cgo.Handle to allow C callbacks to find this interpreter
	interp.handle = TclInterp(cgo.NewHandle(interp))
	return interp
}

// Close releases resources associated with the interpreter.
// Must be called when the interpreter is no longer needed.
func (i *Interp) Close() {
	cgo.Handle(i.handle).Delete()
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
	if obj := i.getObject(i.result); obj != nil {
		resultStr = i.listToString(obj)
	}

	return ParseResult{
		Status: ParseStatus(status),
		Result: resultStr,
	}
}

// listToString converts a list object to its TCL string representation.
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

// Eval evaluates a script string using the C interpreter
func (i *Interp) Eval(script string) (string, error) {
	scriptHandle := i.internString(script)

	// Call the C interpreter
	result := callCEval(i.handle, scriptHandle)

	if result == C.TCL_OK {
		if obj := i.getObject(i.result); obj != nil {
			return obj.stringVal, nil
		}
		return "", nil
	}

	if obj := i.getObject(i.result); obj != nil {
		return "", &EvalError{Message: obj.stringVal}
	}
	return "", &EvalError{Message: "unknown error"}
}

// Result returns the current result string
func (i *Interp) Result() string {
	if obj := i.getObject(i.result); obj != nil {
		return obj.stringVal
	}
	return ""
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
func (i *Interp) GetString(h TclObj) string {
	if obj := i.getObject(h); obj != nil {
		return obj.stringVal
	}
	return ""
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

func getInterp(h C.TclInterp) *Interp {
	return cgo.Handle(h).Value().(*Interp)
}

// Keep unused import to ensure cgo is used
var _ = unsafe.Pointer(nil)
