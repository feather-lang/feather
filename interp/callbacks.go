package interp

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#cgo LDFLAGS: -L${SRCDIR}/../build -ltclc -Wl,-rpath,${SRCDIR}/../build
#include "tclc.h"

// Implemented in callbacks.c
extern TclResult call_tcl_eval_obj(TclInterp interp, TclObj script, TclEvalFlags flags);
*/
import "C"

// Go callback implementations - these are called from C via the wrappers in callbacks.c

//export goBindUnknown
func goBindUnknown(interp C.TclInterp, cmd C.TclObj, args C.TclObj, value *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}

	if i.UnknownHandler != nil {
		// TODO: convert args TclObj (list) to []TclObj slice
		result := i.UnknownHandler(i, TclObj(cmd), nil)
		*value = C.TclObj(i.result)
		return C.TclResult(result)
	}

	return C.TCL_ERROR
}

//export goStringIntern
func goStringIntern(interp C.TclInterp, s *C.char, length C.size_t) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	goStr := C.GoStringN(s, C.int(length))
	return C.TclObj(i.internString(goStr))
}

//export goStringGet
func goStringGet(interp C.TclInterp, obj C.TclObj, length *C.size_t) *C.char {
	i := getInterp(interp)
	if i == nil {
		*length = 0
		return nil
	}
	o := i.getObject(TclObj(obj))
	if o == nil {
		*length = 0
		return nil
	}
	// Cache the C string in the object to avoid memory issues
	if o.cstr == nil {
		o.cstr = C.CString(o.stringVal)
	}
	*length = C.size_t(len(o.stringVal))
	return o.cstr
}

//export goStringConcat
func goStringConcat(interp C.TclInterp, a C.TclObj, b C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	objA := i.getObject(TclObj(a))
	objB := i.getObject(TclObj(b))
	if objA == nil || objB == nil {
		return 0
	}
	return C.TclObj(i.internString(objA.stringVal + objB.stringVal))
}

//export goInterpSetResult
func goInterpSetResult(interp C.TclInterp, result C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.result = TclObj(result)
	return C.TCL_OK
}

//export goInterpGetResult
func goInterpGetResult(interp C.TclInterp) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.TclObj(i.result)
}

//export goInterpResetResult
func goInterpResetResult(interp C.TclInterp, result C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.result = 0
	return C.TCL_OK
}

//export goInterpSetReturnOptions
func goInterpSetReturnOptions(interp C.TclInterp, options C.TclObj) C.TclResult {
	return C.TCL_OK
}

//export goInterpGetReturnOptions
func goInterpGetReturnOptions(interp C.TclInterp, code C.TclResult) C.TclObj {
	return 0
}

//export goListCreate
func goListCreate(interp C.TclInterp) C.TclObj {
	return 0
}

//export goListIsNil
func goListIsNil(interp C.TclInterp, obj C.TclObj) C.int {
	if obj == 0 {
		return 1
	}
	return 0
}

//export goListFrom
func goListFrom(interp C.TclInterp, obj C.TclObj) C.TclObj {
	return 0
}

//export goListPush
func goListPush(interp C.TclInterp, list C.TclObj, item C.TclObj) C.TclObj {
	return list
}

//export goListPop
func goListPop(interp C.TclInterp, list C.TclObj) C.TclObj {
	return 0
}

//export goListUnshift
func goListUnshift(interp C.TclInterp, list C.TclObj, item C.TclObj) C.TclObj {
	return list
}

//export goListShift
func goListShift(interp C.TclInterp, list C.TclObj) C.TclObj {
	return 0
}

//export goListLength
func goListLength(interp C.TclInterp, list C.TclObj) C.size_t {
	return 0
}

//export goListAt
func goListAt(interp C.TclInterp, list C.TclObj) C.TclObj {
	return 0
}

//export goIntCreate
func goIntCreate(interp C.TclInterp, val C.int64_t) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	i.mu.Lock()
	defer i.mu.Unlock()
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{intVal: int64(val), isInt: true}
	return C.TclObj(id)
}

//export goIntGet
func goIntGet(interp C.TclInterp, obj C.TclObj, out *C.int64_t) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	o := i.getObject(TclObj(obj))
	if o == nil || !o.isInt {
		return C.TCL_ERROR
	}
	*out = C.int64_t(o.intVal)
	return C.TCL_OK
}

//export goFramePush
func goFramePush(interp C.TclInterp, cmd C.TclObj, args C.TclObj) C.TclResult {
	return C.TCL_OK
}

//export goFramePop
func goFramePop(interp C.TclInterp) C.TclResult {
	return C.TCL_OK
}

//export goFrameLevel
func goFrameLevel(interp C.TclInterp) C.size_t {
	return 0
}

//export goFrameSetActive
func goFrameSetActive(interp C.TclInterp, level C.size_t) C.TclResult {
	return C.TCL_OK
}

//export goVarGet
func goVarGet(interp C.TclInterp, name C.TclObj) C.TclObj {
	return 0
}

//export goVarSet
func goVarSet(interp C.TclInterp, name C.TclObj, value C.TclObj) {
}

//export goVarUnset
func goVarUnset(interp C.TclInterp, name C.TclObj) {
}

//export goVarExists
func goVarExists(interp C.TclInterp, name C.TclObj) C.TclResult {
	return C.TCL_ERROR
}

//export goVarLink
func goVarLink(interp C.TclInterp, local C.TclObj, target_level C.size_t, target C.TclObj) {
}

//export goProcDefine
func goProcDefine(interp C.TclInterp, name C.TclObj, params C.TclObj, body C.TclObj) {
}

//export goProcExists
func goProcExists(interp C.TclInterp, name C.TclObj) C.int {
	return 0
}

//export goProcParams
func goProcParams(interp C.TclInterp, name C.TclObj, result *C.TclObj) C.TclResult {
	return C.TCL_ERROR
}

//export goProcBody
func goProcBody(interp C.TclInterp, name C.TclObj, result *C.TclObj) C.TclResult {
	return C.TCL_ERROR
}

// callCEval invokes the C interpreter
func callCEval(interpHandle TclInterp, scriptHandle TclObj) C.TclResult {
	return C.call_tcl_eval_obj(C.TclInterp(interpHandle), C.TclObj(scriptHandle), C.TCL_EVAL_LOCAL)
}
