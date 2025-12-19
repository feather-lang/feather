package main

/*
#cgo CFLAGS: -I../../core
#include "../../core/tclc.h"
#include <stdlib.h>

extern const TclHost* tclGetGoHost(void);
*/
import "C"
import (
	"sync"
	"unsafe"
)

// HostContext holds interpreter-level state
type HostContext struct {
	globalVars *VarTable
	procs      map[string]*ProcDef
}

// ProcDef represents a procedure definition
type ProcDef struct {
	name    string
	argList *TclObj
	body    *TclObj
}

// Context handle management
var (
	ctxMu      sync.RWMutex
	ctxHandles = make(map[uintptr]*HostContext)
	nextCtxID  uintptr = 1
)

func allocCtxHandle(ctx *HostContext) uintptr {
	ctxMu.Lock()
	defer ctxMu.Unlock()
	id := nextCtxID
	nextCtxID++
	ctxHandles[id] = ctx
	return id
}

func getCtx(h uintptr) *HostContext {
	ctxMu.RLock()
	defer ctxMu.RUnlock()
	return ctxHandles[h]
}

func freeCtxHandle(h uintptr) {
	ctxMu.Lock()
	defer ctxMu.Unlock()
	delete(ctxHandles, h)
}

// Proc handle management
var (
	procMu      sync.RWMutex
	procHandles = make(map[uintptr]*ProcDef)
	nextProcID  uintptr = 1
)

func allocProcHandle(proc *ProcDef) uintptr {
	procMu.Lock()
	defer procMu.Unlock()
	id := nextProcID
	nextProcID++
	procHandles[id] = proc
	return id
}

func getProc(h uintptr) *ProcDef {
	procMu.RLock()
	defer procMu.RUnlock()
	return procHandles[h]
}

//export goInterpContextNew
func goInterpContextNew(parentCtx unsafe.Pointer, safe C.int) C.uintptr_t {
	ctx := &HostContext{
		globalVars: NewVarTable(),
		procs:      make(map[string]*ProcDef),
	}
	h := allocCtxHandle(ctx)
	return C.uintptr_t(h)
}

//export goInterpContextFree
func goInterpContextFree(ctxHandle C.uintptr_t) {
	freeCtxHandle(uintptr(ctxHandle))
}

//export goFrameAlloc
func goFrameAlloc(ctxHandle C.uintptr_t) C.uintptr_t {
	_ = getCtx(uintptr(ctxHandle)) // Get context (for future use)

	// Allocate frame using C malloc (since C code will manage it)
	frame := (*C.TclFrame)(C.calloc(1, C.sizeof_TclFrame))
	if frame == nil {
		return 0
	}

	// Create variable table for this frame
	varsTable := NewVarTable()
	varsHandle := allocVarsHandle(varsTable)
	// Store handle as uintptr cast to void* - this is just a number, not a Go pointer
	frame.varsHandle = unsafe.Pointer(varsHandle)

	return C.uintptr_t(uintptr(unsafe.Pointer(frame)))
}

//export goFrameFree
func goFrameFree(ctxHandle C.uintptr_t, framePtr C.uintptr_t) {
	if framePtr == 0 {
		return
	}

	frame := (*C.TclFrame)(unsafe.Pointer(uintptr(framePtr)))

	// Free variable table
	if frame.varsHandle != nil {
		varsHandle := uintptr(frame.varsHandle)
		freeVarsHandle(varsHandle)
	}

	C.free(unsafe.Pointer(frame))
}

//export goCmdLookup
func goCmdLookup(ctxHandle C.uintptr_t, name *C.char, nameLen C.size_t, out *C.TclCmdInfo) C.int {
	ctx := getCtx(uintptr(ctxHandle))
	if ctx == nil {
		out._type = C.TCL_CMD_NOT_FOUND
		return 0
	}

	goName := C.GoStringN(name, C.int(nameLen))

	// Look for a proc with this name
	proc, ok := ctx.procs[goName]
	if ok {
		out._type = C.TCL_CMD_PROC
		// Store proc handle in the union
		*(*uintptr)(unsafe.Pointer(&out.u)) = allocProcHandle(proc)
		return 0
	}

	out._type = C.TCL_CMD_NOT_FOUND
	return 0
}

//export goProcRegister
func goProcRegister(ctxHandle C.uintptr_t, name *C.char, nameLen C.size_t,
	argListHandle, bodyHandle C.uintptr_t) C.uintptr_t {
	ctx := getCtx(uintptr(ctxHandle))
	if ctx == nil {
		return 0
	}

	goName := C.GoStringN(name, C.int(nameLen))
	argList := getObj(uintptr(argListHandle))
	body := getObj(uintptr(bodyHandle))

	proc := &ProcDef{
		name:    goName,
		argList: argList.Dup(),
		body:    body.Dup(),
	}

	ctx.procs[goName] = proc
	return C.uintptr_t(allocProcHandle(proc))
}

//export goProcGetDef
func goProcGetDef(handle C.uintptr_t, argListOut, bodyOut *C.uintptr_t) C.int {
	proc := getProc(uintptr(handle))
	if proc == nil {
		return -1
	}

	*argListOut = C.uintptr_t(allocObjHandle(proc.argList))
	*bodyOut = C.uintptr_t(allocObjHandle(proc.body))
	return 0
}

// GetGoHost returns the Go host callback table for C code
func GetGoHost() *C.TclHost {
	return C.tclGetGoHost()
}
