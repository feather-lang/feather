// exports.go provides C API exports for libfeather.
// Build with: go build -buildmode=c-shared -o libfeather.so .
package main

import "github.com/feather-lang/feather"

/*
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Handle types (size_t to match Go's export)
typedef size_t FeatherInterp;
typedef size_t FeatherObj;

// Callback type for custom commands registered from C.
// Commands receive handles and return result/error as handles.
// Return 0 for success, non-zero for error.
typedef int (*FeatherCmd)(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err);

// Wrapper function to call C callback from Go
static inline int callCCallback(FeatherCmd cb, void *data,
                                FeatherInterp interp, size_t argc, FeatherObj *argv,
                                FeatherObj *result, FeatherObj *err) {
    return cb(data, interp, argc, argv, result, err);
}

// Foreign type callbacks
typedef void* (*FeatherForeignNewFunc)(void *userData);
typedef int (*FeatherForeignInvokeFunc)(void *instance, FeatherInterp interp,
                                         const char *method, size_t argc, FeatherObj *argv,
                                         FeatherObj *result, FeatherObj *err);
typedef void (*FeatherForeignDestroyFunc)(void *instance);

// Wrapper functions for foreign type callbacks
static inline void* callForeignNew(FeatherForeignNewFunc fn, void *userData) {
    return fn(userData);
}

static inline int callForeignInvoke(FeatherForeignInvokeFunc fn, void *instance,
                                    FeatherInterp interp, const char *method,
                                    size_t argc, FeatherObj *argv,
                                    FeatherObj *result, FeatherObj *err) {
    return fn(instance, interp, method, argc, argv, result, err);
}

static inline void callForeignDestroy(FeatherForeignDestroyFunc fn, void *instance) {
    fn(instance);
}
*/
import "C"

import (
	"strings"
	"sync"
	"sync/atomic"
	"unsafe"
)

// =============================================================================
// Internal state management using integer handles (not Go pointers)
// =============================================================================

// exportState holds state for an exported interpreter
type exportState struct {
	interp *feather.Interp

	// Arena: maps handle ID -> *feather.Obj
	// Cleared at the END of the outermost FeatherEval() call
	arena       map[uint64]*feather.Obj
	nextArenaID uint64
	evalDepth   int32 // atomic counter for nested eval calls
	mu          sync.Mutex

	// Command callbacks
	callbacks map[string]*cCommandInfo

	// Foreign type callbacks
	foreignTypes map[string]*cForeignTypeInfo
}

// cCommandInfo stores C callback info
type cCommandInfo struct {
	callback C.FeatherCmd
	userData unsafe.Pointer
}

// cForeignTypeInfo stores C foreign type callbacks
type cForeignTypeInfo struct {
	newFn     C.FeatherForeignNewFunc
	invokeFn  C.FeatherForeignInvokeFunc
	destroyFn C.FeatherForeignDestroyFunc
	userData  unsafe.Pointer
	counter   int
	methods   []string // method names for info methods
}

// cForeignInstance stores info about a C foreign instance
type cForeignInstance struct {
	typeName string
	instance unsafe.Pointer
}

var (
	exportMu     sync.Mutex
	exportStates = make(map[uint64]*exportState)
	nextExportID uint64 = 1
)

func getExportState(id uint64) *exportState {
	exportMu.Lock()
	defer exportMu.Unlock()
	return exportStates[id]
}

// registerObj stores an object in the arena and returns its handle
func (s *exportState) registerObj(obj *feather.Obj) uint64 {
	if obj == nil {
		return 0
	}
	s.mu.Lock()
	defer s.mu.Unlock()

	s.nextArenaID++
	handle := s.nextArenaID
	s.arena[handle] = obj
	return handle
}

// getObj retrieves an object by handle
func (s *exportState) getObj(handle uint64) *feather.Obj {
	if handle == 0 {
		return nil
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.arena[handle]
}

// clearArena clears all arena objects (called at start of eval)
func (s *exportState) clearArena() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.arena = make(map[uint64]*feather.Obj)
	s.nextArenaID = 0
}

// =============================================================================
// Lifecycle
// =============================================================================

//export FeatherNew
func FeatherNew() C.size_t {
	interp := feather.New()

	state := &exportState{
		interp:       interp,
		arena:        make(map[uint64]*feather.Obj),
		nextArenaID:  0,
		callbacks:    make(map[string]*cCommandInfo),
		foreignTypes: make(map[string]*cForeignTypeInfo),
	}

	exportMu.Lock()
	id := nextExportID
	nextExportID++
	exportStates[id] = state
	exportMu.Unlock()

	return C.size_t(id)
}

//export FeatherClose
func FeatherClose(interp C.size_t) {
	exportMu.Lock()
	state := exportStates[uint64(interp)]
	if state != nil {
		delete(exportStates, uint64(interp))
	}
	exportMu.Unlock()

	if state != nil {
		state.interp.Close()
	}
}

// =============================================================================
// Evaluation
// =============================================================================

// Parse status constants (matching Go's feather.ParseStatus)
const (
	parseOK         = 0
	parseIncomplete = 1
	parseError      = 2
)

//export FeatherParse
func FeatherParse(interp C.size_t, script *C.char, length C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return parseError
	}

	goScript := C.GoStringN(script, C.int(length))
	pr := state.interp.Parse(goScript)

	switch pr.Status {
	case feather.ParseOK:
		return parseOK
	case feather.ParseIncomplete:
		return parseIncomplete
	default:
		return parseError
	}
}

//export FeatherParseInfo
func FeatherParseInfo(interp C.size_t, script *C.char, length C.size_t, result *C.size_t, errorObj *C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return parseError
	}

	goScript := C.GoStringN(script, C.int(length))
	pr := state.interp.ParseInternal(goScript)

	// Store the result string (e.g., "{INCOMPLETE 5 17}" or "{ERROR 0 10 {message}}")
	if result != nil && pr.Result != "" {
		resultObj := state.interp.String(pr.Result)
		*result = C.size_t(state.registerObj(resultObj))
	}

	// Return error message as FeatherObj
	if errorObj != nil && pr.ErrorMessage != "" {
		errO := state.interp.String(pr.ErrorMessage)
		*errorObj = C.size_t(state.registerObj(errO))
	}

	switch pr.Status {
	case feather.InternalParseOK:
		return parseOK
	case feather.InternalParseIncomplete:
		return parseIncomplete
	default:
		return parseError
	}
}

//export FeatherEval
func FeatherEval(interp C.size_t, script *C.char, length C.size_t, result *C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1 // error
	}

	// Track nesting depth atomically to support nested evals (e.g., source command)
	// Using atomic operations avoids potential mutex issues in CGo callback chains
	atomic.AddInt32(&state.evalDepth, 1)

	// Clear arena only at the END of the outermost eval
	defer func() {
		if atomic.AddInt32(&state.evalDepth, -1) == 0 {
			state.clearArena()
		}
	}()

	goScript := C.GoStringN(script, C.int(length))
	obj, err := state.interp.Eval(goScript)
	if err != nil {
		// On error, store error message as result
		errObj := state.interp.String(err.Error())
		handle := state.registerObj(errObj)
		if result != nil {
			*result = C.size_t(handle)
		}
		return 1
	}

	handle := state.registerObj(obj)
	if result != nil {
		*result = C.size_t(handle)
	}
	return 0 // OK
}

// =============================================================================
// Object Creation
// =============================================================================

//export FeatherString
func FeatherString(interp C.size_t, s *C.char, length C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	goStr := C.GoStringN(s, C.int(length))
	obj := state.interp.String(goStr)
	return C.size_t(state.registerObj(obj))
}

//export FeatherInt
func FeatherInt(interp C.size_t, val C.int64_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	obj := state.interp.Int(int64(val))
	return C.size_t(state.registerObj(obj))
}

//export FeatherDouble
func FeatherDouble(interp C.size_t, val C.double) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	obj := state.interp.Double(float64(val))
	return C.size_t(state.registerObj(obj))
}

//export FeatherList
func FeatherList(interp C.size_t, argc C.size_t, argv *C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	var objs []*feather.Obj
	if argc > 0 && argv != nil {
		itemHandles := unsafe.Slice(argv, int(argc))
		objs = make([]*feather.Obj, int(argc))
		for i := 0; i < int(argc); i++ {
			objs[i] = state.getObj(uint64(itemHandles[i]))
		}
	}

	obj := state.interp.List(objs...)
	return C.size_t(state.registerObj(obj))
}

//export FeatherDict
func FeatherDict(interp C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	obj := state.interp.DictKV()
	return C.size_t(state.registerObj(obj))
}

// =============================================================================
// Type Conversion
// =============================================================================

//export FeatherAsInt
func FeatherAsInt(interp C.size_t, obj C.size_t, def C.int64_t) C.int64_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return def
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return def
	}

	val, err := o.Int()
	if err != nil {
		return def
	}
	return C.int64_t(val)
}

//export FeatherAsDouble
func FeatherAsDouble(interp C.size_t, obj C.size_t, def C.double) C.double {
	state := getExportState(uint64(interp))
	if state == nil {
		return def
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return def
	}

	val, err := o.Double()
	if err != nil {
		return def
	}
	return C.double(val)
}

//export FeatherAsBool
func FeatherAsBool(interp C.size_t, obj C.size_t, def C.int) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return def
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return def
	}

	val, err := o.Bool()
	if err != nil {
		return def
	}
	if val {
		return 1
	}
	return 0
}

// =============================================================================
// String Operations
// =============================================================================

//export FeatherLen
func FeatherLen(interp C.size_t, obj C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return 0
	}

	return C.size_t(len(o.String()))
}

//export FeatherByteAt
func FeatherByteAt(interp C.size_t, obj C.size_t, index C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return -1
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return -1
	}

	str := o.String()
	if int(index) >= len(str) {
		return -1
	}
	return C.int(str[int(index)])
}

//export FeatherEq
func FeatherEq(interp C.size_t, a C.size_t, b C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	objA := state.getObj(uint64(a))
	objB := state.getObj(uint64(b))
	if objA == nil || objB == nil {
		return 0
	}

	if objA.String() == objB.String() {
		return 1
	}
	return 0
}

//export FeatherCmp
func FeatherCmp(interp C.size_t, a C.size_t, b C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	objA := state.getObj(uint64(a))
	objB := state.getObj(uint64(b))
	if objA == nil || objB == nil {
		return 0
	}

	strA := objA.String()
	strB := objB.String()

	if strA < strB {
		return -1
	} else if strA > strB {
		return 1
	}
	return 0
}

//export FeatherCopy
func FeatherCopy(interp C.size_t, obj C.size_t, buf *C.char, length C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return 0
	}

	str := o.String()
	toCopy := len(str)
	if toCopy > int(length) {
		toCopy = int(length)
	}

	if toCopy > 0 && buf != nil {
		// Copy string bytes to the buffer
		goSlice := unsafe.Slice((*byte)(unsafe.Pointer(buf)), toCopy)
		copy(goSlice, str[:toCopy])
	}

	return C.size_t(toCopy)
}

// =============================================================================
// List Operations
// =============================================================================

//export FeatherListLen
func FeatherListLen(interp C.size_t, list C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(list))
	if o == nil {
		return 0
	}

	l, err := o.List()
	if err != nil {
		return 0
	}
	return C.size_t(len(l))
}

//export FeatherListAt
func FeatherListAt(interp C.size_t, list C.size_t, index C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(list))
	if o == nil {
		return 0
	}

	l, err := o.List()
	if err != nil || int(index) >= len(l) {
		return 0
	}

	return C.size_t(state.registerObj(l[int(index)]))
}

//export FeatherListPush
func FeatherListPush(interp C.size_t, list C.size_t, item C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	listObj := state.getObj(uint64(list))
	itemObj := state.getObj(uint64(item))
	if listObj == nil {
		return 0
	}

	l, _ := listObj.List()
	if l == nil {
		l = []*feather.Obj{}
	}

	newList := make([]*feather.Obj, len(l)+1)
	copy(newList, l)
	newList[len(l)] = itemObj

	result := state.interp.List(newList...)
	return C.size_t(state.registerObj(result))
}

// =============================================================================
// Dict Operations
// =============================================================================

//export FeatherDictLen
func FeatherDictLen(interp C.size_t, dict C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(dict))
	if o == nil {
		return 0
	}

	d, err := o.Dict()
	if err != nil {
		return 0
	}
	return C.size_t(len(d.Items))
}

//export FeatherDictGet
func FeatherDictGet(interp C.size_t, dict C.size_t, key C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	dictObj := state.getObj(uint64(dict))
	keyObj := state.getObj(uint64(key))
	if dictObj == nil || keyObj == nil {
		return 0
	}

	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}

	keyStr := keyObj.String()
	if val, exists := d.Items[keyStr]; exists {
		return C.size_t(state.registerObj(val))
	}
	return 0
}

//export FeatherDictSet
func FeatherDictSet(interp C.size_t, dict C.size_t, key C.size_t, value C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	dictObj := state.getObj(uint64(dict))
	keyObj := state.getObj(uint64(key))
	valueObj := state.getObj(uint64(value))
	if dictObj == nil || keyObj == nil {
		return 0
	}

	d, _ := dictObj.Dict()
	items := make(map[string]*feather.Obj)
	var order []string
	if d != nil {
		for k, v := range d.Items {
			items[k] = v
		}
		order = append(order, d.Order...)
	}

	keyStr := keyObj.String()
	// If key doesn't exist, add to order
	if _, exists := items[keyStr]; !exists {
		order = append(order, keyStr)
	}
	items[keyStr] = valueObj

	// Build KV pairs for DictKV
	kvPairs := make([]any, 0, len(order)*2)
	for _, k := range order {
		kvPairs = append(kvPairs, k, items[k])
	}

	result := state.interp.DictKV(kvPairs...)
	return C.size_t(state.registerObj(result))
}

//export FeatherDictHas
func FeatherDictHas(interp C.size_t, dict C.size_t, key C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	dictObj := state.getObj(uint64(dict))
	keyObj := state.getObj(uint64(key))
	if dictObj == nil || keyObj == nil {
		return 0
	}

	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}

	keyStr := keyObj.String()
	if _, exists := d.Items[keyStr]; exists {
		return 1
	}
	return 0
}

//export FeatherDictKeys
func FeatherDictKeys(interp C.size_t, dict C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	dictObj := state.getObj(uint64(dict))
	if dictObj == nil {
		return 0
	}

	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}

	keys := make([]*feather.Obj, 0, len(d.Items))
	for _, k := range d.Order {
		keys = append(keys, state.interp.String(k))
	}

	result := state.interp.List(keys...)
	return C.size_t(state.registerObj(result))
}

// =============================================================================
// Variables
// =============================================================================

//export FeatherSetVar
func FeatherSetVar(interp C.size_t, name *C.char, val C.size_t) {
	state := getExportState(uint64(interp))
	if state == nil {
		return
	}

	goName := C.GoString(name)
	obj := state.getObj(uint64(val))
	if obj != nil {
		state.interp.SetVar(goName, obj.String())
	}
}

//export FeatherGetVar
func FeatherGetVar(interp C.size_t, name *C.char) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	goName := C.GoString(name)
	obj := state.interp.Var(goName)
	if obj == nil {
		return 0
	}
	// Don't return empty strings as valid
	if obj.String() == "" {
		return 0
	}
	return C.size_t(state.registerObj(obj))
}

// =============================================================================
// Command Registration
// =============================================================================

//export FeatherRegister
func FeatherRegister(interp C.size_t, name *C.char, fn C.FeatherCmd, data unsafe.Pointer) {
	state := getExportState(uint64(interp))
	if state == nil {
		return
	}

	goName := C.GoString(name)
	interpHandle := uint64(interp)

	// Store callback info
	info := &cCommandInfo{
		callback: fn,
		userData: data,
	}
	state.mu.Lock()
	state.callbacks[goName] = info
	state.mu.Unlock()

	// Register Go wrapper that calls C callback
	state.interp.RegisterCommand(goName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		// Convert args to handles
		argc := len(args)
		var cArgs *C.size_t
		if argc > 0 {
			cArgs = (*C.size_t)(C.malloc(C.size_t(argc) * C.size_t(unsafe.Sizeof(C.size_t(0)))))
			argSlice := unsafe.Slice(cArgs, argc)
			for j, arg := range args {
				argSlice[j] = C.size_t(state.registerObj(arg))
			}
		}

		var result C.size_t
		var errHandle C.size_t

		// Call C callback with interpreter handle and arg handles
		ret := C.callCCallback(info.callback, info.userData, C.size_t(interpHandle),
			C.size_t(argc), cArgs, &result, &errHandle)

		// Free the args array (handles are managed by arena)
		if cArgs != nil {
			C.free(unsafe.Pointer(cArgs))
		}

		if ret != 0 {
			// Get error object from handle
			if errHandle != 0 {
				errObj := state.getObj(uint64(errHandle))
				if errObj != nil {
					return feather.Error(errObj)
				}
			}
			return feather.Error("command failed")
		}

		// Get result object from handle
		if result != 0 {
			resultObj := state.getObj(uint64(result))
			if resultObj != nil {
				return feather.OK(resultObj)
			}
		}
		return feather.OK(i.String(""))
	})
}

// =============================================================================
// Foreign Type Registration
// =============================================================================

//export FeatherRegisterForeignMethod
func FeatherRegisterForeignMethod(interp C.size_t, typeName *C.char, methodName *C.char) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	goTypeName := C.GoString(typeName)
	goMethodName := C.GoString(methodName)

	state.mu.Lock()
	info, ok := state.foreignTypes[goTypeName]
	if !ok {
		state.mu.Unlock()
		return 1 // Type not registered
	}
	info.methods = append(info.methods, goMethodName)
	state.mu.Unlock()

	// Update the Go ForeignRegistry
	state.interp.RegisterCForeignType(goTypeName, info.methods)

	return 0
}

//export FeatherRegisterForeign
func FeatherRegisterForeign(interp C.size_t, typeName *C.char,
	newFn C.FeatherForeignNewFunc, invokeFn C.FeatherForeignInvokeFunc,
	destroyFn C.FeatherForeignDestroyFunc, userData unsafe.Pointer) C.int {

	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	goTypeName := C.GoString(typeName)
	interpHandle := uint64(interp)

	// Store foreign type info
	info := &cForeignTypeInfo{
		newFn:     newFn,
		invokeFn:  invokeFn,
		destroyFn: destroyFn,
		userData:  userData,
		counter:   0,
		methods:   nil, // will be populated by FeatherRegisterForeignMethod
	}
	state.mu.Lock()
	state.foreignTypes[goTypeName] = info
	state.mu.Unlock()

	// Register constructor command: TypeName new -> creates instance
	state.interp.RegisterCommand(goTypeName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		if len(args) < 1 {
			return feather.Error("wrong # args: should be \"" + goTypeName + " subcommand ?arg ...?\"")
		}

		method := args[0].String()
		if method != "new" {
			return feather.Error("unknown subcommand \"" + method + "\": must be new")
		}

		// Create new instance via C callback
		instance := C.callForeignNew(info.newFn, info.userData)
		if instance == nil {
			return feather.Error("failed to create " + goTypeName + " instance")
		}

		// Generate unique instance ID and handle name (lowercase like Go version)
		state.mu.Lock()
		info.counter++
		instanceID := info.counter
		state.mu.Unlock()

		// Use lowercase handle name like Go version (e.g., "counter1" for first instance)
		instanceName := strings.ToLower(goTypeName) + itoa(instanceID)

		// Create a proper foreign object with ForeignType intrep and custom handle name
		foreignObj, foreignHandle := i.NewForeignHandleNamed(goTypeName, instanceName, nil)

		// Register instance in ForeignRegistry for info commands
		i.RegisterCForeignInstance(instanceName, goTypeName, foreignHandle)

		// Store instance info for C callbacks
		instanceInfo := &cForeignInstance{
			typeName: goTypeName,
			instance: instance,
		}

		// Register instance command
		i.RegisterCommand(instanceName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
			if len(args) < 1 {
				return feather.Error("wrong # args: should be \"" + instanceName + " method ?args...?\"")
			}

			methodName := args[0].String()

			// Handle destroy method
			if methodName == "destroy" {
				C.callForeignDestroy(info.destroyFn, instanceInfo.instance)
				// Unregister the command so subsequent calls fail
				i.UnregisterCommand(instanceName)
				return feather.OK(i.String(""))
			}

			// Convert remaining args to handles
			methodArgs := args[1:]
			argc := len(methodArgs)
			var cArgs *C.size_t
			if argc > 0 {
				cArgs = (*C.size_t)(C.malloc(C.size_t(argc) * C.size_t(unsafe.Sizeof(C.size_t(0)))))
				argSlice := unsafe.Slice(cArgs, argc)
				for j, arg := range methodArgs {
					argSlice[j] = C.size_t(state.registerObj(arg))
				}
			}

			var result C.size_t
			var errHandle C.size_t

			// Call C invoke callback with interpreter handle and arg handles
			cMethodName := C.CString(methodName)
			ret := C.callForeignInvoke(info.invokeFn, instanceInfo.instance,
				C.size_t(interpHandle), cMethodName, C.size_t(argc), cArgs, &result, &errHandle)
			C.free(unsafe.Pointer(cMethodName))

			// Free the args array
			if cArgs != nil {
				C.free(unsafe.Pointer(cArgs))
			}

			if ret != 0 {
				// Get error object from handle
				if errHandle != 0 {
					errObj := state.getObj(uint64(errHandle))
					if errObj != nil {
						return feather.Error(errObj)
					}
				}
				return feather.Error("method failed")
			}

			// Get result object from handle
			if result != 0 {
				resultObj := state.getObj(uint64(result))
				if resultObj != nil {
					return feather.OK(resultObj)
				}
			}
			return feather.OK(i.String(""))
		})

		// Return the foreign object (not a string)
		return feather.OK(foreignObj)
	})

	return 0
}

// itoa converts int to string without importing strconv
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	if n < 0 {
		return "-" + itoa(-n)
	}
	var digits []byte
	for n > 0 {
		digits = append([]byte{byte('0' + n%10)}, digits...)
		n /= 10
	}
	return string(digits)
}

func main() {}
