// Package main exports feather interpreter functions for use as a C shared library.
// Build with: go build -buildmode=c-shared -o libfeather.so .
package main

/*
#include <stdlib.h>
#include <stdint.h>

// Callback type for custom commands registered from C
typedef int (*FeatherCommandCallback)(void *userData, int argc, char **argv, char **result, char **error);

// Wrapper function to call C callback from Go
static inline int callCallback(FeatherCommandCallback cb, void *userData, int argc, char **argv, char **result, char **error) {
    return cb(userData, argc, argv, result, error);
}
*/
import "C"

import (
	"strconv"
	"strings"
	"sync"
	"unsafe"

	"github.com/feather-lang/feather"
)

// interpState holds interpreter state and cached results
type interpState struct {
	interp     *feather.Interp
	lastResult string
	lastError  string
	lastOK     bool
	// Parse state
	parseStatus  int
	parseMessage string
	parseResult  string // Structured result like "{INCOMPLETE 5 19}"
}

var (
	mu      sync.Mutex
	interps = make(map[uintptr]*interpState)
	nextID  uintptr = 1
)

func getInterp(id uintptr) *interpState {
	mu.Lock()
	defer mu.Unlock()
	return interps[id]
}

// -----------------------------------------------------------------------------
// Interpreter Lifecycle
// -----------------------------------------------------------------------------

//export FeatherNew
func FeatherNew() uintptr {
	interp := feather.New()
	state := &interpState{interp: interp}

	mu.Lock()
	id := nextID
	nextID++
	interps[id] = state
	mu.Unlock()

	return id
}

//export FeatherClose
func FeatherClose(id uintptr) {
	mu.Lock()
	defer mu.Unlock()

	if state, ok := interps[id]; ok {
		state.interp.Close()
		delete(interps, id)
		cleanupRegistry(id)
	}
}

// -----------------------------------------------------------------------------
// Script Evaluation
// -----------------------------------------------------------------------------

//export FeatherEval
func FeatherEval(id uintptr, script *C.char, scriptLen C.int) C.int {
	state := getInterp(id)
	if state == nil {
		return 0
	}

	goScript := C.GoStringN(script, scriptLen)
	result, err := state.interp.Eval(goScript)

	mu.Lock()
	if err != nil {
		state.lastError = err.Error()
		state.lastResult = ""
		state.lastOK = false
	} else {
		state.lastError = ""
		state.lastResult = result.String()
		state.lastOK = true
	}
	mu.Unlock()

	if err != nil {
		return 0
	}
	return 1
}

//export FeatherEvalResult
func FeatherEvalResult(id uintptr) *C.char {
	state := getInterp(id)
	if state == nil {
		return C.CString("")
	}
	mu.Lock()
	result := state.lastResult
	mu.Unlock()
	return C.CString(result)
}

//export FeatherEvalError
func FeatherEvalError(id uintptr) *C.char {
	state := getInterp(id)
	if state == nil {
		return C.CString("")
	}
	mu.Lock()
	errMsg := state.lastError
	mu.Unlock()
	return C.CString(errMsg)
}

//export FeatherEvalOK
func FeatherEvalOK(id uintptr) C.int {
	state := getInterp(id)
	if state == nil {
		return 0
	}
	mu.Lock()
	ok := state.lastOK
	mu.Unlock()
	if ok {
		return 1
	}
	return 0
}

// -----------------------------------------------------------------------------
// Parsing
// -----------------------------------------------------------------------------

// Parse status constants (match feather.ParseStatus values)
const (
	ParseOK         = 0
	ParseIncomplete = 1
	ParseError      = 2
)

//export FeatherParse
func FeatherParse(id uintptr, script *C.char, scriptLen C.int) C.int {
	state := getInterp(id)
	if state == nil {
		return ParseError
	}

	goScript := C.GoStringN(script, scriptLen)
	result := state.interp.Parse(goScript)

	// Also get the internal parse result for structured output
	internalResult := state.interp.ParseInternal(goScript)

	mu.Lock()
	state.parseStatus = int(result.Status)
	state.parseMessage = result.Message
	state.parseResult = internalResult.Result
	mu.Unlock()

	return C.int(result.Status)
}

//export FeatherParseMessage
func FeatherParseMessage(id uintptr) *C.char {
	state := getInterp(id)
	if state == nil {
		return C.CString("")
	}
	mu.Lock()
	msg := state.parseMessage
	mu.Unlock()
	return C.CString(msg)
}

//export FeatherParseResult
func FeatherParseResult(id uintptr) *C.char {
	state := getInterp(id)
	if state == nil {
		return C.CString("")
	}
	mu.Lock()
	result := state.parseResult
	mu.Unlock()
	return C.CString(result)
}

// -----------------------------------------------------------------------------
// Variables
// -----------------------------------------------------------------------------

//export FeatherSetVar
func FeatherSetVar(id uintptr, name *C.char, value *C.char) {
	state := getInterp(id)
	if state == nil {
		return
	}

	goName := C.GoString(name)
	goValue := C.GoString(value)
	state.interp.SetVar(goName, goValue)
}

//export FeatherGetVar
func FeatherGetVar(id uintptr, name *C.char) *C.char {
	state := getInterp(id)
	if state == nil {
		return C.CString("")
	}

	goName := C.GoString(name)
	obj := state.interp.Var(goName)
	return C.CString(obj.String())
}

// -----------------------------------------------------------------------------
// Command Registration
// -----------------------------------------------------------------------------

// commandCallbackInfo stores info about a registered C callback
type commandCallbackInfo struct {
	callback C.FeatherCommandCallback
	userData unsafe.Pointer
}

var (
	callbackMu   sync.Mutex
	callbackMap  = make(map[uintptr]map[string]*commandCallbackInfo) // interp id -> name -> callback
)

//export FeatherRegisterCommand
func FeatherRegisterCommand(id uintptr, name *C.char, callback C.FeatherCommandCallback, userData unsafe.Pointer) {
	state := getInterp(id)
	if state == nil {
		return
	}

	goName := C.GoString(name)

	// Store callback info
	callbackMu.Lock()
	if callbackMap[id] == nil {
		callbackMap[id] = make(map[string]*commandCallbackInfo)
	}
	info := &commandCallbackInfo{callback: callback, userData: userData}
	callbackMap[id][goName] = info
	callbackMu.Unlock()

	// Register the command with the interpreter
	state.interp.RegisterCommand(goName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		callbackMu.Lock()
		cbInfo := callbackMap[id][goName]
		callbackMu.Unlock()

		if cbInfo == nil {
			return feather.Error("callback not found")
		}

		// Convert args to C strings
		argc := C.int(len(args))
		var argv **C.char
		var argvSlice []*C.char
		if len(args) > 0 {
			argvSlice = make([]*C.char, len(args))
			for j, arg := range args {
				argvSlice[j] = C.CString(arg.String())
			}
			argv = &argvSlice[0]
			defer func() {
				for _, s := range argvSlice {
					C.free(unsafe.Pointer(s))
				}
			}()
		}

		// Call the C callback via wrapper
		var resultPtr, errorPtr *C.char
		status := C.callCallback(cbInfo.callback, cbInfo.userData, argc, argv, &resultPtr, &errorPtr)

		// Process result
		if status != 0 {
			errMsg := ""
			if errorPtr != nil {
				errMsg = C.GoString(errorPtr)
				C.free(unsafe.Pointer(errorPtr))
			}
			return feather.Error(errMsg)
		}

		result := ""
		if resultPtr != nil {
			result = C.GoString(resultPtr)
			C.free(unsafe.Pointer(resultPtr))
		}
		return feather.OK(result)
	})
}

// -----------------------------------------------------------------------------
// Foreign Types
// -----------------------------------------------------------------------------

// ForeignTypeInfo stores info about a registered foreign type
type ForeignTypeInfo struct {
	constructor C.FeatherCommandCallback
	methods     map[string]C.FeatherCommandCallback
	userData    unsafe.Pointer
	counter     int
}

// ForeignInstanceInfo stores info about a foreign object instance
type ForeignInstanceInfo struct {
	typeName   string
	handleName string
	userData   unsafe.Pointer
}

var (
	foreignMu        sync.Mutex
	foreignTypes     = make(map[uintptr]map[string]*ForeignTypeInfo)     // interp id -> type name -> info
	foreignInstances = make(map[uintptr]map[string]*ForeignInstanceInfo) // interp id -> handle name -> instance
)

//export FeatherRegisterForeignType
func FeatherRegisterForeignType(id uintptr, typeName *C.char, constructor C.FeatherCommandCallback, userData unsafe.Pointer) {
	state := getInterp(id)
	if state == nil {
		return
	}

	goTypeName := C.GoString(typeName)

	foreignMu.Lock()
	if foreignTypes[id] == nil {
		foreignTypes[id] = make(map[string]*ForeignTypeInfo)
	}
	if foreignInstances[id] == nil {
		foreignInstances[id] = make(map[string]*ForeignInstanceInfo)
	}
	info := &ForeignTypeInfo{
		constructor: constructor,
		methods:     make(map[string]C.FeatherCommandCallback),
		userData:    userData,
		counter:     1,
	}
	foreignTypes[id][goTypeName] = info
	foreignMu.Unlock()

	// Register as a command that handles "new" subcommand
	state.interp.RegisterCommand(goTypeName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		if len(args) == 0 {
			return feather.Errorf("wrong # args: should be \"%s subcommand ?arg ...?\"", goTypeName)
		}

		subCmd := args[0].String()
		if subCmd != "new" {
			return feather.Errorf("unknown subcommand \"%s\": must be new", subCmd)
		}

		// Get type info
		foreignMu.Lock()
		typeInfo := foreignTypes[id][goTypeName]
		if typeInfo == nil {
			foreignMu.Unlock()
			return feather.Error("type not found")
		}

		// Generate handle name with lowercase type name (e.g., "counter1")
		handleName := strings.ToLower(goTypeName) + uintptrToString(uintptr(typeInfo.counter))
		typeInfo.counter++
		foreignMu.Unlock()

		if typeInfo.constructor == nil {
			return feather.Error("constructor not found")
		}

		// Call C constructor to get user data for this instance
		var resultPtr, errorPtr *C.char
		status := C.callCallback(typeInfo.constructor, typeInfo.userData, 0, nil, &resultPtr, &errorPtr)

		if status != 0 {
			errMsg := "constructor failed"
			if errorPtr != nil {
				errMsg = C.GoString(errorPtr)
				C.free(unsafe.Pointer(errorPtr))
			}
			return feather.Error(errMsg)
		}

		// resultPtr contains the instance user data as a hex pointer string (e.g., "0x7f...")
		// Parse it to get the actual pointer
		var instanceData unsafe.Pointer
		if resultPtr != nil {
			ptrStr := C.GoString(resultPtr)
			C.free(unsafe.Pointer(resultPtr))
			// Parse hex pointer (remove 0x prefix if present)
			ptrStr = strings.TrimPrefix(ptrStr, "0x")
			ptrStr = strings.TrimPrefix(ptrStr, "0X")
			if ptrVal, err := strconv.ParseUint(ptrStr, 16, 64); err == nil {
				instanceData = unsafe.Pointer(uintptr(ptrVal))
			}
		}

		// Store instance info
		instanceInfo := &ForeignInstanceInfo{
			typeName:   goTypeName,
			handleName: handleName,
			userData:   instanceData,
		}
		foreignMu.Lock()
		foreignInstances[id][handleName] = instanceInfo
		foreignMu.Unlock()

		// Register the handle as a command for method dispatch
		i.RegisterCommand(handleName, func(ii *feather.Interp, cmdObj *feather.Obj, methodArgs []*feather.Obj) feather.Result {
			if len(methodArgs) == 0 {
				return feather.Errorf("wrong # args: should be \"%s method ?arg ...?\"", handleName)
			}

			methodName := methodArgs[0].String()
			args := methodArgs[1:]

			// Handle built-in destroy method
			if methodName == "destroy" {
				foreignMu.Lock()
				delete(foreignInstances[id], handleName)
				foreignMu.Unlock()
				// Note: We don't unregister the command here to match Go behavior
				return feather.OK("")
			}

			// Get instance and type info
			foreignMu.Lock()
			inst := foreignInstances[id][handleName]
			if inst == nil {
				foreignMu.Unlock()
				return feather.Errorf("invalid object handle \"%s\"", handleName)
			}
			tInfo := foreignTypes[id][inst.typeName]
			if tInfo == nil {
				foreignMu.Unlock()
				return feather.Errorf("unknown type \"%s\"", inst.typeName)
			}
			methodCb, ok := tInfo.methods[methodName]
			foreignMu.Unlock()

			if !ok {
				// List available methods in error
				var methodList []string
				for name := range tInfo.methods {
					methodList = append(methodList, name)
				}
				methodList = append(methodList, "destroy")
				return feather.Errorf("unknown method \"%s\": must be %s", methodName, strings.Join(methodList, ", "))
			}

			// Convert args to C strings
			argc := C.int(len(args))
			var argv **C.char
			var argvSlice []*C.char
			if len(args) > 0 {
				argvSlice = make([]*C.char, len(args))
				for j, arg := range args {
					argvSlice[j] = C.CString(arg.String())
				}
				argv = &argvSlice[0]
				defer func() {
					for _, s := range argvSlice {
						C.free(unsafe.Pointer(s))
					}
				}()
			}

			// Call the method callback with instance userData
			var resultStr, errorStr *C.char
			methodStatus := C.callCallback(methodCb, inst.userData, argc, argv, &resultStr, &errorStr)

			if methodStatus != 0 {
				errMsg := ""
				if errorStr != nil {
					errMsg = C.GoString(errorStr)
					C.free(unsafe.Pointer(errorStr))
				}
				return feather.Error(errMsg)
			}

			result := ""
			if resultStr != nil {
				result = C.GoString(resultStr)
				C.free(unsafe.Pointer(resultStr))
			}
			return feather.OK(result)
		})

		return feather.OK(handleName)
	})
}

//export FeatherRegisterForeignMethod
func FeatherRegisterForeignMethod(id uintptr, typeName *C.char, methodName *C.char, callback C.FeatherCommandCallback) {
	goTypeName := C.GoString(typeName)
	goMethodName := C.GoString(methodName)

	foreignMu.Lock()
	if foreignTypes[id] != nil && foreignTypes[id][goTypeName] != nil {
		foreignTypes[id][goTypeName].methods[goMethodName] = callback
	}
	foreignMu.Unlock()
}

func uintptrToString(v uintptr) string {
	const digits = "0123456789"
	if v == 0 {
		return "0"
	}
	var buf [20]byte
	i := len(buf)
	for v > 0 {
		i--
		buf[i] = digits[v%10]
		v /= 10
	}
	return string(buf[i:])
}

// -----------------------------------------------------------------------------
// Memory Management
// -----------------------------------------------------------------------------

//export FeatherFreeString
func FeatherFreeString(s *C.char) {
	if s != nil {
		C.free(unsafe.Pointer(s))
	}
}

// -----------------------------------------------------------------------------
// Main (required for c-shared build)
// -----------------------------------------------------------------------------

func main() {}
