package wasm

import (
	"context"

	"github.com/tetratelabs/wazero"
	"github.com/tetratelabs/wazero/api"
)

// InterpProvider is an interface for accessing interpreter state from WASM callbacks.
// This breaks the circular dependency between wasm and wasmtcl packages.
type InterpProvider interface {
	// String operations
	InternString(s string) TclObj
	GetString(h TclObj) string
	StringConcat(a, b TclObj) TclObj
	StringCompare(a, b TclObj) int

	// Integer operations
	CreateInt(val int64) TclObj
	GetInt(h TclObj) (int64, error)

	// Double operations
	CreateDouble(val float64) TclObj
	GetDouble(h TclObj) (float64, error)

	// List operations
	IsNil(obj TclObj) bool
	CreateList() TclObj
	ListFrom(obj TclObj) TclObj
	ListPush(list, item TclObj) TclObj
	ListPop(list TclObj) TclObj
	ListUnshift(list, item TclObj) TclObj
	ListShift(list TclObj) TclObj
	ListLength(list TclObj) int
	ListAt(list TclObj, index int) TclObj

	// Frame operations
	FramePush(cmd, args TclObj) TclResult
	FramePop() TclResult
	FrameLevel() int
	FrameSetActive(level int) TclResult
	FrameSize() int
	FrameInfo(level int) (cmd, args TclObj, ok bool)

	// Variable operations
	VarGet(name TclObj) TclObj
	VarSet(name, value TclObj)
	VarUnset(name TclObj)
	VarExists(name TclObj) bool
	VarLink(local TclObj, targetLevel int, target TclObj)

	// Procedure operations
	ProcDefine(name, params, body TclObj)
	ProcExists(name TclObj) bool
	ProcParams(name TclObj) (TclObj, bool)
	ProcBody(name TclObj) (TclObj, bool)
	ProcNames(namespace TclObj) TclObj
	RegisterCommand(name TclObj)
	CommandLookup(name TclObj) (cmdType int, canonicalName string)
	CommandRename(oldName, newName TclObj) TclResult
	ResolveNamespace(path TclObj) (TclObj, bool)

	// Interpreter operations
	SetResult(obj TclObj)
	GetResult() TclObj
	ResetResult()
	SetReturnOptions(options TclObj) TclResult
	GetReturnOptions(code TclResult) TclObj

	// Instance access for memory operations
	Instance() *Instance
}

// interpRegistry maps interpreter IDs to InterpProvider instances.
// This is used by WASM callbacks to find the correct interpreter.
var interpRegistry = make(map[TclInterp]InterpProvider)

// RegisterInterp registers an interpreter with the callback system.
func RegisterInterp(id TclInterp, interp InterpProvider) {
	interpRegistry[id] = interp
}

// UnregisterInterp removes an interpreter from the callback system.
func UnregisterInterp(id TclInterp) {
	delete(interpRegistry, id)
}

// getInterp retrieves an interpreter by ID.
func getInterp(id TclInterp) InterpProvider {
	return interpRegistry[id]
}

// BuildHostModule creates the host module with all TclHostOps callbacks.
// This replaces the stub implementation in runtime.go.
func BuildHostModule(ctx context.Context, r wazero.Runtime) (wazero.CompiledModule, error) {
	builder := r.NewHostModuleBuilder("env")

	// String operations
	builder.NewFunctionBuilder().
		WithFunc(stringIntern).
		Export("string_intern")

	builder.NewFunctionBuilder().
		WithFunc(stringGet).
		Export("string_get")

	builder.NewFunctionBuilder().
		WithFunc(stringConcat).
		Export("string_concat")

	builder.NewFunctionBuilder().
		WithFunc(stringCompare).
		Export("string_compare")

	// Integer operations
	builder.NewFunctionBuilder().
		WithFunc(integerCreate).
		Export("integer_create")

	builder.NewFunctionBuilder().
		WithFunc(integerGet).
		Export("integer_get")

	// Double operations
	builder.NewFunctionBuilder().
		WithFunc(doubleCreate).
		Export("dbl_create")

	builder.NewFunctionBuilder().
		WithFunc(doubleGet).
		Export("dbl_get")

	// List operations
	builder.NewFunctionBuilder().
		WithFunc(listIsNil).
		Export("list_is_nil")

	builder.NewFunctionBuilder().
		WithFunc(listCreate).
		Export("list_create")

	builder.NewFunctionBuilder().
		WithFunc(listFrom).
		Export("list_from")

	builder.NewFunctionBuilder().
		WithFunc(listPush).
		Export("list_push")

	builder.NewFunctionBuilder().
		WithFunc(listPop).
		Export("list_pop")

	builder.NewFunctionBuilder().
		WithFunc(listUnshift).
		Export("list_unshift")

	builder.NewFunctionBuilder().
		WithFunc(listShift).
		Export("list_shift")

	builder.NewFunctionBuilder().
		WithFunc(listLength).
		Export("list_length")

	builder.NewFunctionBuilder().
		WithFunc(listAt).
		Export("list_at")

	// Frame operations
	builder.NewFunctionBuilder().
		WithFunc(framePush).
		Export("frame_push")

	builder.NewFunctionBuilder().
		WithFunc(framePop).
		Export("frame_pop")

	builder.NewFunctionBuilder().
		WithFunc(frameLevel).
		Export("frame_level")

	builder.NewFunctionBuilder().
		WithFunc(frameSetActive).
		Export("frame_set_active")

	builder.NewFunctionBuilder().
		WithFunc(frameSize).
		Export("frame_size")

	builder.NewFunctionBuilder().
		WithFunc(frameInfo).
		Export("frame_info")

	// Variable operations
	builder.NewFunctionBuilder().
		WithFunc(varGet).
		Export("var_get")

	builder.NewFunctionBuilder().
		WithFunc(varSet).
		Export("var_set")

	builder.NewFunctionBuilder().
		WithFunc(varUnset).
		Export("var_unset")

	builder.NewFunctionBuilder().
		WithFunc(varExists).
		Export("var_exists")

	builder.NewFunctionBuilder().
		WithFunc(varLink).
		Export("var_link")

	// Procedure operations
	builder.NewFunctionBuilder().
		WithFunc(procDefine).
		Export("proc_define")

	builder.NewFunctionBuilder().
		WithFunc(procExists).
		Export("proc_exists")

	builder.NewFunctionBuilder().
		WithFunc(procParams).
		Export("proc_params")

	builder.NewFunctionBuilder().
		WithFunc(procBody).
		Export("proc_body")

	builder.NewFunctionBuilder().
		WithFunc(procNames).
		Export("proc_names")

	builder.NewFunctionBuilder().
		WithFunc(procRegisterCommand).
		Export("proc_register_command")

	builder.NewFunctionBuilder().
		WithFunc(procLookup).
		Export("proc_lookup")

	builder.NewFunctionBuilder().
		WithFunc(procRename).
		Export("proc_rename")

	builder.NewFunctionBuilder().
		WithFunc(procResolveNamespace).
		Export("proc_resolve_namespace")

	// Interpreter operations
	builder.NewFunctionBuilder().
		WithFunc(interpSetResult).
		Export("interp_set_result")

	builder.NewFunctionBuilder().
		WithFunc(interpGetResult).
		Export("interp_get_result")

	builder.NewFunctionBuilder().
		WithFunc(interpResetResult).
		Export("interp_reset_result")

	builder.NewFunctionBuilder().
		WithFunc(interpSetReturnOptions).
		Export("interp_set_return_options")

	builder.NewFunctionBuilder().
		WithFunc(interpGetReturnOptions).
		Export("interp_get_return_options")

	// Bind operations
	builder.NewFunctionBuilder().
		WithFunc(bindUnknown).
		Export("bind_unknown")

	// Memory allocation (required for C code)
	builder.NewFunctionBuilder().
		WithFunc(func(ctx context.Context, size uint32) uint32 {
			return 0
		}).
		Export("malloc")

	builder.NewFunctionBuilder().
		WithFunc(func(ctx context.Context, ptr uint32) {
		}).
		Export("free")

	return builder.Compile(ctx)
}

// String callbacks

func stringIntern(ctx context.Context, m api.Module, interp TclInterp, strPtr, strLen uint32) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	inst := i.Instance()
	if inst == nil {
		return 0
	}
	str, ok := inst.ReadString(strPtr, strLen)
	if !ok {
		return 0
	}
	return i.InternString(str)
}

func stringGet(ctx context.Context, m api.Module, interp TclInterp, obj TclObj, lenPtr uint32) uint32 {
	// Returns pointer to string in WASM memory, sets length at lenPtr
	// This is complex because we need to copy the string into WASM memory
	// For now, return 0 to indicate we need a different approach
	return 0
}

func stringConcat(ctx context.Context, m api.Module, interp TclInterp, a, b TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.StringConcat(a, b)
}

func stringCompare(ctx context.Context, m api.Module, interp TclInterp, a, b TclObj) int32 {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return int32(i.StringCompare(a, b))
}

// Integer callbacks

func integerCreate(ctx context.Context, m api.Module, interp TclInterp, val int64) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.CreateInt(val)
}

func integerGet(ctx context.Context, m api.Module, interp TclInterp, obj TclObj, outPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	val, err := i.GetInt(obj)
	if err != nil {
		return uint32(ResultError)
	}
	inst := i.Instance()
	if inst != nil && outPtr != 0 {
		// Write int64 to memory at outPtr
		inst.memory.WriteUint64Le(outPtr, uint64(val))
	}
	return uint32(ResultOK)
}

// Double callbacks

func doubleCreate(ctx context.Context, m api.Module, interp TclInterp, val float64) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.CreateDouble(val)
}

func doubleGet(ctx context.Context, m api.Module, interp TclInterp, obj TclObj, outPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	val, err := i.GetDouble(obj)
	if err != nil {
		return uint32(ResultError)
	}
	inst := i.Instance()
	if inst != nil && outPtr != 0 {
		inst.memory.WriteFloat64Le(outPtr, val)
	}
	return uint32(ResultOK)
}

// List callbacks

func listIsNil(ctx context.Context, m api.Module, interp TclInterp, obj TclObj) int32 {
	i := getInterp(interp)
	if i == nil {
		return 1
	}
	if i.IsNil(obj) {
		return 1
	}
	return 0
}

func listCreate(ctx context.Context, m api.Module, interp TclInterp) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.CreateList()
}

func listFrom(ctx context.Context, m api.Module, interp TclInterp, obj TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.ListFrom(obj)
}

func listPush(ctx context.Context, m api.Module, interp TclInterp, list, item TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return list
	}
	return i.ListPush(list, item)
}

func listPop(ctx context.Context, m api.Module, interp TclInterp, list TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.ListPop(list)
}

func listUnshift(ctx context.Context, m api.Module, interp TclInterp, list, item TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return list
	}
	return i.ListUnshift(list, item)
}

func listShift(ctx context.Context, m api.Module, interp TclInterp, list TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.ListShift(list)
}

func listLength(ctx context.Context, m api.Module, interp TclInterp, list TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return uint32(i.ListLength(list))
}

func listAt(ctx context.Context, m api.Module, interp TclInterp, list TclObj, index uint32) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.ListAt(list, int(index))
}

// Frame callbacks

func framePush(ctx context.Context, m api.Module, interp TclInterp, cmd, args TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	return uint32(i.FramePush(cmd, args))
}

func framePop(ctx context.Context, m api.Module, interp TclInterp) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	return uint32(i.FramePop())
}

func frameLevel(ctx context.Context, m api.Module, interp TclInterp) uint32 {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return uint32(i.FrameLevel())
}

func frameSetActive(ctx context.Context, m api.Module, interp TclInterp, level uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	return uint32(i.FrameSetActive(int(level)))
}

func frameSize(ctx context.Context, m api.Module, interp TclInterp) uint32 {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return uint32(i.FrameSize())
}

func frameInfo(ctx context.Context, m api.Module, interp TclInterp, level uint32, cmdPtr, argsPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	cmd, args, ok := i.FrameInfo(int(level))
	if !ok {
		return uint32(ResultError)
	}
	inst := i.Instance()
	if inst != nil {
		if cmdPtr != 0 {
			inst.memory.WriteUint32Le(cmdPtr, cmd)
		}
		if argsPtr != 0 {
			inst.memory.WriteUint32Le(argsPtr, args)
		}
	}
	return uint32(ResultOK)
}

// Variable callbacks

func varGet(ctx context.Context, m api.Module, interp TclInterp, name TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.VarGet(name)
}

func varSet(ctx context.Context, m api.Module, interp TclInterp, name, value TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.VarSet(name, value)
}

func varUnset(ctx context.Context, m api.Module, interp TclInterp, name TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.VarUnset(name)
}

func varExists(ctx context.Context, m api.Module, interp TclInterp, name TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	if i.VarExists(name) {
		return uint32(ResultOK)
	}
	return uint32(ResultError)
}

func varLink(ctx context.Context, m api.Module, interp TclInterp, local TclObj, targetLevel uint32, target TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.VarLink(local, int(targetLevel), target)
}

// Procedure callbacks

func procDefine(ctx context.Context, m api.Module, interp TclInterp, name, params, body TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.ProcDefine(name, params, body)
}

func procExists(ctx context.Context, m api.Module, interp TclInterp, name TclObj) int32 {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	if i.ProcExists(name) {
		return 1
	}
	return 0
}

func procParams(ctx context.Context, m api.Module, interp TclInterp, name TclObj, resultPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	params, ok := i.ProcParams(name)
	if !ok {
		return uint32(ResultError)
	}
	inst := i.Instance()
	if inst != nil && resultPtr != 0 {
		inst.memory.WriteUint32Le(resultPtr, params)
	}
	return uint32(ResultOK)
}

func procBody(ctx context.Context, m api.Module, interp TclInterp, name TclObj, resultPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	body, ok := i.ProcBody(name)
	if !ok {
		return uint32(ResultError)
	}
	inst := i.Instance()
	if inst != nil && resultPtr != 0 {
		inst.memory.WriteUint32Le(resultPtr, body)
	}
	return uint32(ResultOK)
}

func procNames(ctx context.Context, m api.Module, interp TclInterp, namespace TclObj) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.ProcNames(namespace)
}

func procRegisterCommand(ctx context.Context, m api.Module, interp TclInterp, name TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.RegisterCommand(name)
}

func procLookup(ctx context.Context, m api.Module, interp TclInterp, name TclObj, canonicalPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return 0 // CmdNone
	}
	cmdType, canonical := i.CommandLookup(name)
	if canonicalPtr != 0 {
		canonicalObj := i.InternString(canonical)
		inst := i.Instance()
		if inst != nil {
			inst.memory.WriteUint32Le(canonicalPtr, canonicalObj)
		}
	}
	return uint32(cmdType)
}

func procRename(ctx context.Context, m api.Module, interp TclInterp, oldName, newName TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	return uint32(i.CommandRename(oldName, newName))
}

func procResolveNamespace(ctx context.Context, m api.Module, interp TclInterp, path TclObj, resultPtr uint32) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	ns, ok := i.ResolveNamespace(path)
	if !ok {
		return uint32(ResultError)
	}
	inst := i.Instance()
	if inst != nil && resultPtr != 0 {
		inst.memory.WriteUint32Le(resultPtr, ns)
	}
	return uint32(ResultOK)
}

// Interpreter callbacks

func interpSetResult(ctx context.Context, m api.Module, interp TclInterp, result TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	i.SetResult(result)
	return uint32(ResultOK)
}

func interpGetResult(ctx context.Context, m api.Module, interp TclInterp) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.GetResult()
}

func interpResetResult(ctx context.Context, m api.Module, interp TclInterp, result TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	i.ResetResult()
	return uint32(ResultOK)
}

func interpSetReturnOptions(ctx context.Context, m api.Module, interp TclInterp, options TclObj) uint32 {
	i := getInterp(interp)
	if i == nil {
		return uint32(ResultError)
	}
	return uint32(i.SetReturnOptions(options))
}

func interpGetReturnOptions(ctx context.Context, m api.Module, interp TclInterp, code uint32) TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return i.GetReturnOptions(TclResult(code))
}

// Bind callbacks

func bindUnknown(ctx context.Context, m api.Module, interp TclInterp, cmd, args TclObj, valuePtr uint32) uint32 {
	// For now, always return error (command not found)
	// The interpreter will handle the unknown command fallback
	return uint32(ResultError)
}
