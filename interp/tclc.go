package interp

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#include "tclc.h"
#include <stdlib.h>
*/
import "C"
import (
	"sync"
	"unsafe"
)

// Result codes matching TclResult enum
const (
	ResultOK       = C.TCL_OK
	ResultError    = C.TCL_ERROR
	ResultReturn   = C.TCL_RETURN
	ResultBreak    = C.TCL_BREAK
	ResultContinue = C.TCL_CONTINUE
)

// EvalFlags matching TclEvalFlags enum
const (
	EvalLocal  = C.TCL_EVAL_LOCAL
	EvalGlobal = C.TCL_EVAL_GLOBAL
)

// Handle is the Go type for TclHandle
type Handle = uint32

// Interp represents a TCL interpreter instance
type Interp struct {
	handle  Handle
	objects map[Handle]*Object
	nextID  Handle
	result  Handle
	mu      sync.Mutex

	// UnknownHandler is called when an unknown command is invoked
	UnknownHandler func(interp *Interp, cmd string, args []string) (string, error)
}

// Object represents a TCL object
type Object struct {
	stringVal string
	cstr      *C.char // cached C string for passing to C code
	intVal    int64
	isInt     bool
}

// Global registry of interpreters (cgo callbacks need to look them up)
var (
	interpRegistry   = make(map[Handle]*Interp)
	interpRegistryMu sync.RWMutex
	nextInterpID     Handle = 1
)

// NewInterp creates a new interpreter
func NewInterp() *Interp {
	interpRegistryMu.Lock()
	defer interpRegistryMu.Unlock()

	id := nextInterpID
	nextInterpID++

	interp := &Interp{
		handle:  id,
		objects: make(map[Handle]*Object),
		nextID:  1,
	}
	interpRegistry[id] = interp
	return interp
}

// Handle returns the interpreter's handle
func (i *Interp) Handle() Handle {
	return i.handle
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
func (i *Interp) internString(s string) Handle {
	i.mu.Lock()
	defer i.mu.Unlock()

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{stringVal: s}
	return id
}

// getObject retrieves an object by handle
func (i *Interp) getObject(h Handle) *Object {
	i.mu.Lock()
	defer i.mu.Unlock()
	return i.objects[h]
}

func getInterp(h C.TclInterp) *Interp {
	interpRegistryMu.RLock()
	defer interpRegistryMu.RUnlock()
	return interpRegistry[Handle(h)]
}

// Keep unused import to ensure cgo is used
var _ = unsafe.Pointer(nil)
