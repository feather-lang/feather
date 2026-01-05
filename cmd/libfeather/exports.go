// exports.go provides C API exports for libfeather.
// Build with: go build -buildmode=c-shared -o libfeather.so .
package main

import "github.com/feather-lang/feather"

/*
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Callback type for custom commands registered from C
typedef int (*FeatherCommandCallback)(void *userData, int argc, char **argv, char **result, char **error);

// Wrapper function to call C callback from Go
static inline int callCCallback(FeatherCommandCallback cb, void *userData, int argc, char **argv, char **result, char **error) {
    return cb(userData, argc, argv, result, error);
}

// Foreign type callbacks
typedef void* (*FeatherForeignNewFunc)(void *userData);
typedef int (*FeatherForeignInvokeFunc)(void *instance, const char *method, int argc, char **argv, char **result, char **error);
typedef void (*FeatherForeignDestroyFunc)(void *instance);

// Wrapper functions for foreign type callbacks
static inline void* callForeignNew(FeatherForeignNewFunc fn, void *userData) {
    return fn(userData);
}

static inline int callForeignInvoke(FeatherForeignInvokeFunc fn, void *instance, const char *method, int argc, char **argv, char **result, char **error) {
    return fn(instance, method, argc, argv, result, error);
}

static inline void callForeignDestroy(FeatherForeignDestroyFunc fn, void *instance) {
    fn(instance);
}
*/
import "C"

import (
	"sync"
	"unsafe"
)

// =============================================================================
// Internal state management using integer handles (not Go pointers)
// =============================================================================

// exportState holds state for an exported interpreter
type exportState struct {
	interp *feather.Interp

	// Object handles: maps handle ID -> *feather.Obj
	objects    map[uint64]*feather.Obj
	nextObjID  uint64
	objRefCount map[uint64]int // Reference counts for objects
	mu         sync.Mutex

	// Command callbacks
	callbacks map[string]*cCommandInfo

	// Foreign type callbacks
	foreignTypes map[string]*cForeignTypeInfo
}

// cCommandInfo stores C callback info
type cCommandInfo struct {
	callback C.FeatherCommandCallback
	userData unsafe.Pointer
}

// cForeignTypeInfo stores C foreign type callbacks
type cForeignTypeInfo struct {
	newFn     C.FeatherForeignNewFunc
	invokeFn  C.FeatherForeignInvokeFunc
	destroyFn C.FeatherForeignDestroyFunc
	userData  unsafe.Pointer
	counter   int
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

// storeObj stores an object and returns its handle
func (s *exportState) storeObj(obj *feather.Obj) uint64 {
	if obj == nil {
		return 0
	}
	s.mu.Lock()
	defer s.mu.Unlock()

	s.nextObjID++
	handle := s.nextObjID
	s.objects[handle] = obj
	s.objRefCount[handle] = 1
	return handle
}

// getObj retrieves an object by handle
func (s *exportState) getObj(handle uint64) *feather.Obj {
	if handle == 0 {
		return nil
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.objects[handle]
}

// retainObj increments reference count
func (s *exportState) retainObj(handle uint64) {
	if handle == 0 {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.objRefCount[handle]; exists {
		s.objRefCount[handle]++
	}
}

// releaseObj decrements reference count and removes if zero
func (s *exportState) releaseObj(handle uint64) {
	if handle == 0 {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if count, exists := s.objRefCount[handle]; exists {
		count--
		if count <= 0 {
			delete(s.objects, handle)
			delete(s.objRefCount, handle)
		} else {
			s.objRefCount[handle] = count
		}
	}
}

// =============================================================================
// Lifecycle (2 functions)
// =============================================================================

//export FeatherNew
func FeatherNew() C.size_t {
	interp := feather.New()

	state := &exportState{
		interp:       interp,
		objects:      make(map[uint64]*feather.Obj),
		nextObjID:    0,
		objRefCount:  make(map[uint64]int),
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
// Evaluation (2 functions)
// =============================================================================

//export FeatherEval
func FeatherEval(interp C.size_t, script *C.char, length C.size_t, result *C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1 // FEATHER_ERROR
	}

	goScript := C.GoStringN(script, C.int(length))
	obj, err := state.interp.Eval(goScript)
	if err != nil {
		// On error, store error message as result
		errObj := state.interp.String(err.Error())
		handle := state.storeObj(errObj)
		if result != nil {
			*result = C.size_t(handle)
		}
		return 1
	}

	handle := state.storeObj(obj)
	if result != nil {
		*result = C.size_t(handle)
	}
	return 0 // FEATHER_OK
}

//export FeatherCall
func FeatherCall(interp C.size_t, cmd *C.char, argc C.int, argv *C.size_t, result *C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	cmdName := C.GoString(cmd)

	// Convert argv handles to Go args
	args := make([]any, int(argc))
	if argc > 0 && argv != nil {
		argHandles := unsafe.Slice(argv, int(argc))
		for i := 0; i < int(argc); i++ {
			obj := state.getObj(uint64(argHandles[i]))
			args[i] = obj
		}
	}

	obj, err := state.interp.Call(cmdName, args...)
	if err != nil {
		errObj := state.interp.String(err.Error())
		handle := state.storeObj(errObj)
		if result != nil {
			*result = C.size_t(handle)
		}
		return 1
	}

	handle := state.storeObj(obj)
	if result != nil {
		*result = C.size_t(handle)
	}
	return 0
}

// =============================================================================
// Object Creation (4 functions)
// =============================================================================

//export FeatherString
func FeatherString(interp C.size_t, s *C.char, length C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	goStr := C.GoStringN(s, C.int(length))
	obj := state.interp.String(goStr)
	return C.size_t(state.storeObj(obj))
}

//export FeatherInt
func FeatherInt(interp C.size_t, val C.int64_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	obj := state.interp.Int(int64(val))
	return C.size_t(state.storeObj(obj))
}

//export FeatherDouble
func FeatherDouble(interp C.size_t, val C.double) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	obj := state.interp.Double(float64(val))
	return C.size_t(state.storeObj(obj))
}

//export FeatherList
func FeatherList(interp C.size_t, argc C.int, items *C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	var objs []*feather.Obj
	if argc > 0 && items != nil {
		itemHandles := unsafe.Slice(items, int(argc))
		objs = make([]*feather.Obj, int(argc))
		for i := 0; i < int(argc); i++ {
			objs[i] = state.getObj(uint64(itemHandles[i]))
		}
	}

	obj := state.interp.List(objs...)
	return C.size_t(state.storeObj(obj))
}

// =============================================================================
// Object Access (4 functions)
// =============================================================================

//export FeatherGetString
func FeatherGetString(obj C.size_t, interp C.size_t, length *C.size_t) *C.char {
	state := getExportState(uint64(interp))
	if state == nil {
		return nil
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return nil
	}

	str := o.String()
	if length != nil {
		*length = C.size_t(len(str))
	}
	return C.CString(str)
}

//export FeatherGetInt
func FeatherGetInt(obj C.size_t, interp C.size_t, out *C.int64_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return 1
	}

	val, err := o.Int()
	if err != nil {
		return 1
	}
	if out != nil {
		*out = C.int64_t(val)
	}
	return 0
}

//export FeatherGetDouble
func FeatherGetDouble(obj C.size_t, interp C.size_t, out *C.double) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return 1
	}

	val, err := o.Double()
	if err != nil {
		return 1
	}
	if out != nil {
		*out = C.double(val)
	}
	return 0
}

//export FeatherGetList
func FeatherGetList(interp C.size_t, obj C.size_t, argc *C.int, items **C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return 1
	}

	list, err := o.List()
	if err != nil {
		return 1
	}

	n := len(list)
	if argc != nil {
		*argc = C.int(n)
	}
	if items != nil && n > 0 {
		handles := (*C.size_t)(C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof(C.size_t(0)))))
		handleSlice := unsafe.Slice(handles, n)
		for i, item := range list {
			handleSlice[i] = C.size_t(state.storeObj(item))
		}
		*items = handles
	}
	return 0
}

// =============================================================================
// Memory Management (2 functions)
// =============================================================================

//export FeatherRetain
func FeatherRetain(interp C.size_t, obj C.size_t) {
	state := getExportState(uint64(interp))
	if state == nil {
		return
	}
	state.retainObj(uint64(obj))
}

//export FeatherRelease
func FeatherRelease(interp C.size_t, obj C.size_t) {
	state := getExportState(uint64(interp))
	if state == nil {
		return
	}
	state.releaseObj(uint64(obj))
}

//export FeatherFreeString
func FeatherFreeString(s *C.char) {
	C.free(unsafe.Pointer(s))
}

// =============================================================================
// String Operations (6 functions)
// =============================================================================

//export FeatherStringLength
func FeatherStringLength(obj C.size_t, interp C.size_t) C.size_t {
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

//export FeatherStringByteAt
func FeatherStringByteAt(obj C.size_t, interp C.size_t, index C.size_t) C.int {
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

//export FeatherStringSlice
func FeatherStringSlice(interp C.size_t, str C.size_t, start C.size_t, end C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(str))
	if o == nil {
		return 0
	}

	s := o.String()
	if int(start) > len(s) {
		start = C.size_t(len(s))
	}
	if int(end) > len(s) {
		end = C.size_t(len(s))
	}
	if start > end {
		start = end
	}

	result := state.interp.String(s[int(start):int(end)])
	return C.size_t(state.storeObj(result))
}

//export FeatherStringConcat
func FeatherStringConcat(interp C.size_t, a C.size_t, b C.size_t) C.size_t {
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
	result := state.interp.String(strA + strB)
	return C.size_t(state.storeObj(result))
}

//export FeatherStringCompare
func FeatherStringCompare(a C.size_t, b C.size_t, interp C.size_t) C.int {
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

//export FeatherStringMatch
func FeatherStringMatch(pattern C.size_t, str C.size_t, interp C.size_t) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	objP := state.getObj(uint64(pattern))
	objS := state.getObj(uint64(str))
	if objP == nil || objS == nil {
		return 0
	}

	// Simple glob matching for string match
	if simpleGlobMatch(objP.String(), objS.String()) {
		return 1
	}
	return 0
}

// simpleGlobMatch implements basic glob matching (* and ?)
func simpleGlobMatch(pattern, str string) bool {
	return simpleGlobMatchHelper(pattern, str, 0, 0)
}

func simpleGlobMatchHelper(pattern, str string, pi, si int) bool {
	for pi < len(pattern) {
		if si >= len(str) {
			// Check if remaining pattern is all *
			for pi < len(pattern) {
				if pattern[pi] != '*' {
					return false
				}
				pi++
			}
			return true
		}
		switch pattern[pi] {
		case '*':
			// Try to match zero or more characters
			for i := si; i <= len(str); i++ {
				if simpleGlobMatchHelper(pattern, str, pi+1, i) {
					return true
				}
			}
			return false
		case '?':
			// Match exactly one character
			pi++
			si++
		default:
			if pattern[pi] != str[si] {
				return false
			}
			pi++
			si++
		}
	}
	return si == len(str)
}

// =============================================================================
// List Operations (5 functions)
// =============================================================================

//export FeatherListLength
func FeatherListLength(obj C.size_t, interp C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(obj))
	if o == nil {
		return 0
	}

	list, err := o.List()
	if err != nil {
		return 0
	}
	return C.size_t(len(list))
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

	return C.size_t(state.storeObj(l[int(index)]))
}

//export FeatherListSlice
func FeatherListSlice(interp C.size_t, list C.size_t, first C.size_t, last C.size_t) C.size_t {
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

	if int(first) > len(l) {
		first = C.size_t(len(l))
	}
	if int(last) > len(l) {
		last = C.size_t(len(l))
	}
	if first > last {
		first = last
	}

	result := state.interp.List(l[int(first):int(last)]...)
	return C.size_t(state.storeObj(result))
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
	return C.size_t(state.storeObj(result))
}

//export FeatherListSet
func FeatherListSet(interp C.size_t, list C.size_t, index C.size_t, value C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	listObj := state.getObj(uint64(list))
	valueObj := state.getObj(uint64(value))
	if listObj == nil {
		return 0
	}

	l, err := listObj.List()
	if err != nil || int(index) >= len(l) {
		return 0
	}

	newList := make([]*feather.Obj, len(l))
	copy(newList, l)
	newList[int(index)] = valueObj

	result := state.interp.List(newList...)
	return C.size_t(state.storeObj(result))
}

// =============================================================================
// Dict Operations (6 functions)
// =============================================================================

//export FeatherDict
func FeatherDict(interp C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	obj := state.interp.DictKV()
	return C.size_t(state.storeObj(obj))
}

//export FeatherDictSize
func FeatherDictSize(obj C.size_t, interp C.size_t) C.size_t {
	state := getExportState(uint64(interp))
	if state == nil {
		return 0
	}

	o := state.getObj(uint64(obj))
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
		return C.size_t(state.storeObj(val))
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
	if d != nil {
		for k, v := range d.Items {
			items[k] = v
		}
	}

	keyStr := keyObj.String()
	items[keyStr] = valueObj

	// Build KV pairs for DictKV
	kvPairs := make([]any, 0, len(items)*2)
	for k, v := range items {
		kvPairs = append(kvPairs, k, v)
	}

	result := state.interp.DictKV(kvPairs...)
	return C.size_t(state.storeObj(result))
}

//export FeatherDictExists
func FeatherDictExists(dict C.size_t, key C.size_t, interp C.size_t) C.int {
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
	return C.size_t(state.storeObj(result))
}

// =============================================================================
// Variables (2 functions)
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
		state.interp.SetVar(goName, obj)
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
	return C.size_t(state.storeObj(obj))
}

// =============================================================================
// Command Registration (1 function)
// =============================================================================

//export FeatherRegisterCommand
func FeatherRegisterCommand(interp C.size_t, name *C.char, callback C.FeatherCommandCallback, userData unsafe.Pointer) C.int {
	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	goName := C.GoString(name)

	// Store callback info
	info := &cCommandInfo{
		callback: callback,
		userData: userData,
	}
	state.mu.Lock()
	state.callbacks[goName] = info
	state.mu.Unlock()

	// Register Go wrapper that calls C callback
	state.interp.RegisterCommand(goName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		// Convert args to C strings
		argc := len(args)
		var cArgs **C.char
		if argc > 0 {
			cArgs = (**C.char)(C.malloc(C.size_t(argc) * C.size_t(unsafe.Sizeof((*C.char)(nil)))))
			argSlice := unsafe.Slice(cArgs, argc)
			for j, arg := range args {
				argSlice[j] = C.CString(arg.String())
			}
		}

		var result *C.char
		var errMsg *C.char

		// Call C callback
		ret := C.callCCallback(info.callback, info.userData, C.int(argc), cArgs, &result, &errMsg)

		// Free C arg strings
		if cArgs != nil {
			argSlice := unsafe.Slice(cArgs, argc)
			for j := 0; j < argc; j++ {
				C.free(unsafe.Pointer(argSlice[j]))
			}
			C.free(unsafe.Pointer(cArgs))
		}

		if ret != 0 {
			var errStr string
			if errMsg != nil {
				errStr = C.GoString(errMsg)
				C.free(unsafe.Pointer(errMsg))
			} else {
				errStr = "command failed"
			}
			return feather.Error(errStr)
		}

		var resultStr string
		if result != nil {
			resultStr = C.GoString(result)
			C.free(unsafe.Pointer(result))
		}
		return feather.OK(i.String(resultStr))
	})

	return 0
}

// =============================================================================
// Foreign Type Registration (1 function)
// =============================================================================

//export FeatherRegisterForeign
func FeatherRegisterForeign(interp C.size_t, typeName *C.char,
	newFn C.FeatherForeignNewFunc, invokeFn C.FeatherForeignInvokeFunc,
	destroyFn C.FeatherForeignDestroyFunc, userData unsafe.Pointer) C.int {

	state := getExportState(uint64(interp))
	if state == nil {
		return 1
	}

	goTypeName := C.GoString(typeName)

	// Store foreign type info
	info := &cForeignTypeInfo{
		newFn:     newFn,
		invokeFn:  invokeFn,
		destroyFn: destroyFn,
		userData:  userData,
		counter:   0,
	}
	state.mu.Lock()
	state.foreignTypes[goTypeName] = info
	state.mu.Unlock()

	// Register constructor command: TypeName new -> creates instance
	state.interp.RegisterCommand(goTypeName, func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		if len(args) < 1 {
			return feather.Error("wrong # args: should be \"" + goTypeName + " new\"")
		}

		method := args[0].String()
		if method != "new" {
			return feather.Error("unknown method \"" + method + "\": should be \"new\"")
		}

		// Create new instance via C callback
		instance := C.callForeignNew(info.newFn, info.userData)
		if instance == nil {
			return feather.Error("failed to create " + goTypeName + " instance")
		}

		// Generate unique instance ID
		state.mu.Lock()
		info.counter++
		instanceID := info.counter
		state.mu.Unlock()

		// Create instance command
		instanceName := goTypeName + "_" + itoa(instanceID)

		// Store instance info
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
				// Note: we don't unregister the command in Feather (would need API support)
				return feather.OK(i.String(""))
			}

			// Convert remaining args to C strings
			methodArgs := args[1:]
			argc := len(methodArgs)
			var cArgs **C.char
			if argc > 0 {
				cArgs = (**C.char)(C.malloc(C.size_t(argc) * C.size_t(unsafe.Sizeof((*C.char)(nil)))))
				argSlice := unsafe.Slice(cArgs, argc)
				for j, arg := range methodArgs {
					argSlice[j] = C.CString(arg.String())
				}
			}

			var result *C.char
			var errMsg *C.char

			// Call C invoke callback
			ret := C.callForeignInvoke(info.invokeFn, instanceInfo.instance,
				C.CString(methodName), C.int(argc), cArgs, &result, &errMsg)

			// Free C arg strings
			if cArgs != nil {
				argSlice := unsafe.Slice(cArgs, argc)
				for j := 0; j < argc; j++ {
					C.free(unsafe.Pointer(argSlice[j]))
				}
				C.free(unsafe.Pointer(cArgs))
			}

			if ret != 0 {
				var errStr string
				if errMsg != nil {
					errStr = C.GoString(errMsg)
					C.free(unsafe.Pointer(errMsg))
				} else {
					errStr = "method failed"
				}
				return feather.Error(errStr)
			}

			var resultStr string
			if result != nil {
				resultStr = C.GoString(result)
				C.free(unsafe.Pointer(result))
			}
			return feather.OK(i.String(resultStr))
		})

		return feather.OK(i.String(instanceName))
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
