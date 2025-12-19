package main

/*
#cgo CFLAGS: -I../../core
#include "../../core/tclc.h"
#include <stdlib.h>

extern const TclHost* tclGetGoHost(void);
*/
import "C"
import (
	"runtime/cgo"
	"unsafe"
)

// TclCmdFunc is the signature for extension command functions
type TclCmdFunc func(interp *TclInterp, args []*TclObj) (*TclObj, error)

// ExtCmd represents an extension command
type ExtCmd struct {
	handle cgo.Handle
	name   string
	fn     TclCmdFunc
}

// TclInterp wraps the C interpreter for extension commands
type TclInterp struct {
	cPtr *C.TclInterp
}

// HostContext holds interpreter-level state
type HostContext struct {
	handle     cgo.Handle
	globalVars *VarTable
	procs      map[string]*ProcDef
	commands   map[string]*ExtCmd // Extension commands
}

// ProcDef represents a procedure definition
type ProcDef struct {
	handle  cgo.Handle
	name    string
	argList *TclObj
	body    *TclObj
}

// Handle returns the stable handle for this HostContext
func (ctx *HostContext) Handle() uintptr {
	if ctx == nil {
		return 0
	}
	return uintptr(ctx.handle)
}

// getCtx retrieves a HostContext by handle using cgo.Handle
func getCtx(h uintptr) *HostContext {
	if h == 0 {
		return nil
	}
	return cgo.Handle(h).Value().(*HostContext)
}

// freeCtx frees the HostContext's handle
func freeCtx(ctx *HostContext) {
	if ctx == nil {
		return
	}
	if ctx.handle != 0 {
		ctx.handle.Delete()
		ctx.handle = 0
	}
}

// Handle returns the stable handle for this ProcDef
func (p *ProcDef) Handle() uintptr {
	if p == nil {
		return 0
	}
	return uintptr(p.handle)
}

// getProc retrieves a ProcDef by handle using cgo.Handle
func getProc(h uintptr) *ProcDef {
	if h == 0 {
		return nil
	}
	return cgo.Handle(h).Value().(*ProcDef)
}

// Handle returns the stable handle for this ExtCmd
func (e *ExtCmd) Handle() uintptr {
	if e == nil {
		return 0
	}
	return uintptr(e.handle)
}

// getExtCmd retrieves an ExtCmd by handle using cgo.Handle
func getExtCmd(h uintptr) *ExtCmd {
	if h == 0 {
		return nil
	}
	return cgo.Handle(h).Value().(*ExtCmd)
}

// RegisterCommand registers an extension command
func (ctx *HostContext) RegisterCommand(name string, fn TclCmdFunc) {
	if ctx.commands == nil {
		ctx.commands = make(map[string]*ExtCmd)
	}
	cmd := &ExtCmd{name: name, fn: fn}
	cmd.handle = cgo.NewHandle(cmd)
	ctx.commands[name] = cmd
}

//export goInterpContextNew
func goInterpContextNew(parentCtx unsafe.Pointer, safe C.int) C.uintptr_t {
	ctx := &HostContext{
		globalVars: NewVarTable(),
		procs:      make(map[string]*ProcDef),
		commands:   make(map[string]*ExtCmd),
	}
	ctx.handle = cgo.NewHandle(ctx)
	return C.uintptr_t(ctx.Handle())
}

//export goInterpContextFree
func goInterpContextFree(ctxHandle C.uintptr_t) {
	ctx := getCtx(uintptr(ctxHandle))
	freeCtx(ctx)
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
	// Store handle as uintptr cast to void* - this is just a number, not a Go pointer
	frame.varsHandle = unsafe.Pointer(varsTable.Handle())

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
		varsTable := getVars(varsHandle)
		freeVars(varsTable)
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

	// Check extension commands first
	if cmd, ok := ctx.commands[goName]; ok {
		out._type = C.TCL_CMD_EXTENSION
		*(*uintptr)(unsafe.Pointer(&out.u)) = cmd.Handle()
		return 0
	}

	// Then check procs
	if proc, ok := ctx.procs[goName]; ok {
		out._type = C.TCL_CMD_PROC
		*(*uintptr)(unsafe.Pointer(&out.u)) = proc.Handle()
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
	proc.handle = cgo.NewHandle(proc)

	ctx.procs[goName] = proc
	return C.uintptr_t(proc.Handle())
}

//export goProcGetDef
func goProcGetDef(handle C.uintptr_t, argListOut, bodyOut *C.uintptr_t) C.int {
	proc := getProc(uintptr(handle))
	if proc == nil {
		return -1
	}

	// Return existing handles instead of allocating new ones
	*argListOut = C.uintptr_t(proc.argList.Handle())
	*bodyOut = C.uintptr_t(proc.body.Handle())
	return 0
}

//export goExtInvoke
func goExtInvoke(interpPtr *C.TclInterp, handle C.uintptr_t,
	objc C.int, objv **C.uintptr_t) C.TclResult {
	cmd := getExtCmd(uintptr(handle))
	if cmd == nil {
		return C.TCL_ERROR
	}

	// Convert arguments
	args := make([]*TclObj, int(objc))
	if objc > 0 {
		objvSlice := unsafe.Slice(objv, int(objc))
		for i := range args {
			args[i] = getObj(uintptr(*objvSlice[i]))
		}
	}

	// Create Interp wrapper
	interp := &TclInterp{cPtr: interpPtr}

	// Call Go function
	result, err := cmd.fn(interp, args)
	if err != nil {
		// TODO: Set error message in interpreter
		return C.TCL_ERROR
	}

	if result != nil {
		// TODO: Set result in interpreter
		_ = result
	}
	return C.TCL_OK
}

// GetGoHost returns the Go host callback table for C code
func GetGoHost() *C.TclHost {
	return C.tclGetGoHost()
}
