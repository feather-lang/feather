// Package wasm provides the wazero runtime for tclc WASM module.
package wasm

import (
	"context"
	_ "embed"
	"errors"
	"fmt"
	"sync"

	"github.com/tetratelabs/wazero"
	"github.com/tetratelabs/wazero/api"
)

//go:embed tclc.wasm
var tclcWASM []byte

// TclResult codes matching the C enum
type TclResult uint32

const (
	ResultOK       TclResult = 0
	ResultError    TclResult = 1
	ResultReturn   TclResult = 2
	ResultBreak    TclResult = 3
	ResultContinue TclResult = 4
)

// CmdType indicates the type of a command
type CmdType int

const (
	CmdNone    CmdType = 0
	CmdBuiltin CmdType = 1
	CmdProc    CmdType = 2
)

// TclObj is a handle to a TCL object (matches TclHandle in C)
type TclObj = uint32

// TclInterp is a handle to an interpreter instance
type TclInterp = uint32

// Runtime wraps the wazero runtime and compiled tclc module.
type Runtime struct {
	ctx      context.Context
	runtime  wazero.Runtime
	compiled wazero.CompiledModule

	mu        sync.Mutex
	instances map[TclInterp]*Instance
	nextID    TclInterp
}

// Instance represents an instantiated tclc module with its state.
type Instance struct {
	id       TclInterp
	module   api.Module
	memory   api.Memory
	runtime  *Runtime
	hostData any // opaque data for host callbacks

	// Exported functions from tclc.wasm
	scriptEval    api.Function
	interpInit    api.Function
	parseInit     api.Function
	parseCommand  api.Function
	commandExec   api.Function
	scriptEvalObj api.Function
	subst         api.Function
}

// NewRuntime creates a new WASM runtime with the tclc module compiled.
func NewRuntime(ctx context.Context) (*Runtime, error) {
	r := wazero.NewRuntime(ctx)

	compiled, err := r.CompileModule(ctx, tclcWASM)
	if err != nil {
		r.Close(ctx)
		return nil, fmt.Errorf("compile tclc.wasm: %w", err)
	}

	return &Runtime{
		ctx:       ctx,
		runtime:   r,
		compiled:  compiled,
		instances: make(map[TclInterp]*Instance),
		nextID:    1,
	}, nil
}

// Close releases all resources associated with the runtime.
func (r *Runtime) Close() error {
	r.mu.Lock()
	defer r.mu.Unlock()

	var errs []error
	for _, inst := range r.instances {
		if err := inst.module.Close(r.ctx); err != nil {
			errs = append(errs, err)
		}
	}
	r.instances = nil

	if err := r.runtime.Close(r.ctx); err != nil {
		errs = append(errs, err)
	}

	return errors.Join(errs...)
}

// NewInstance creates a new interpreter instance.
// The hostData parameter is passed to host callbacks.
func (r *Runtime) NewInstance(hostData any) (*Instance, error) {
	r.mu.Lock()
	id := r.nextID
	r.nextID++
	r.mu.Unlock()

	// Create a unique module name for this instance
	moduleName := fmt.Sprintf("tclc_%d", id)

	// Build host module with TclHostOps imports
	hostMod, err := r.buildHostModule(id)
	if err != nil {
		return nil, fmt.Errorf("build host module: %w", err)
	}

	// Instantiate host module first (provides imports)
	_, err = r.runtime.InstantiateModule(r.ctx, hostMod, wazero.NewModuleConfig().WithName("env"))
	if err != nil {
		return nil, fmt.Errorf("instantiate host module: %w", err)
	}

	// Instantiate tclc module
	mod, err := r.runtime.InstantiateModule(r.ctx, r.compiled, wazero.NewModuleConfig().WithName(moduleName))
	if err != nil {
		return nil, fmt.Errorf("instantiate tclc: %w", err)
	}

	inst := &Instance{
		id:            id,
		module:        mod,
		memory:        mod.Memory(),
		runtime:       r,
		hostData:      hostData,
		scriptEval:    mod.ExportedFunction("wasm_script_eval"),
		interpInit:    mod.ExportedFunction("wasm_interp_init"),
		parseInit:     mod.ExportedFunction("tcl_parse_init"),
		parseCommand:  mod.ExportedFunction("tcl_parse_command"),
		commandExec:   mod.ExportedFunction("wasm_command_exec"),
		scriptEvalObj: mod.ExportedFunction("wasm_script_eval_obj"),
		subst:         mod.ExportedFunction("tcl_subst"),
	}

	r.mu.Lock()
	r.instances[id] = inst
	r.mu.Unlock()

	return inst, nil
}

// buildHostModule creates the host module with TclHostOps function imports.
func (r *Runtime) buildHostModule(interpID TclInterp) (wazero.CompiledModule, error) {
	return BuildHostModule(r.ctx, r.runtime)
}

// ID returns the instance's unique identifier.
func (i *Instance) ID() TclInterp {
	return i.id
}

// HostData returns the opaque host data associated with this instance.
func (i *Instance) HostData() any {
	return i.hostData
}

// Close releases resources for this instance.
func (i *Instance) Close() error {
	i.runtime.mu.Lock()
	delete(i.runtime.instances, i.id)
	i.runtime.mu.Unlock()
	return i.module.Close(i.runtime.ctx)
}

// Memory helpers for string passing

// WriteString writes a Go string to WASM memory at the given offset.
// Returns the number of bytes written.
func (i *Instance) WriteString(offset uint32, s string) uint32 {
	data := []byte(s)
	if !i.memory.Write(offset, data) {
		return 0
	}
	return uint32(len(data))
}

// WriteStringNullTerminated writes a null-terminated string to WASM memory.
// Returns the number of bytes written including the null terminator.
func (i *Instance) WriteStringNullTerminated(offset uint32, s string) uint32 {
	data := append([]byte(s), 0)
	if !i.memory.Write(offset, data) {
		return 0
	}
	return uint32(len(data))
}

// ReadString reads a string from WASM memory given offset and length.
func (i *Instance) ReadString(offset, length uint32) (string, bool) {
	data, ok := i.memory.Read(offset, length)
	if !ok {
		return "", false
	}
	return string(data), true
}

// ReadStringNullTerminated reads a null-terminated string from WASM memory.
func (i *Instance) ReadStringNullTerminated(offset uint32) (string, bool) {
	// Read in chunks until we find a null byte
	var result []byte
	const chunkSize = 256
	for {
		chunk, ok := i.memory.Read(offset+uint32(len(result)), chunkSize)
		if !ok {
			return "", false
		}
		for idx, b := range chunk {
			if b == 0 {
				return string(result) + string(chunk[:idx]), true
			}
		}
		result = append(result, chunk...)
		if len(result) > 1<<20 { // 1MB limit
			return "", false
		}
	}
}

// heapBase is the starting address for heap allocation.
// This is set when the instance is created from the __heap_base export.
var heapBaseAddr uint32

// allocator is a simple bump allocator for WASM memory.
type allocator struct {
	mu      sync.Mutex
	nextPtr uint32
	memory  api.Memory
}

var globalAllocator *allocator

// initAllocator initializes the bump allocator from __heap_base.
func (i *Instance) initAllocator() error {
	heapBase := i.module.ExportedGlobal("__heap_base")
	if heapBase == nil {
		return fmt.Errorf("__heap_base not exported")
	}
	heapBaseAddr = uint32(heapBase.Get())
	globalAllocator = &allocator{
		nextPtr: heapBaseAddr,
		memory:  i.memory,
	}
	return nil
}

// Allocate allocates memory in the WASM module using a bump allocator.
// Returns the offset in WASM linear memory where the data can be written.
func (i *Instance) Allocate(size uint32) (uint32, error) {
	if globalAllocator == nil {
		if err := i.initAllocator(); err != nil {
			return 0, err
		}
	}

	globalAllocator.mu.Lock()
	defer globalAllocator.mu.Unlock()

	// Align to 8 bytes
	aligned := (globalAllocator.nextPtr + 7) &^ 7
	newPtr := aligned + size

	// Check if we need to grow memory
	memSize := i.memory.Size()
	if newPtr > memSize {
		// Calculate pages needed (WASM page = 64KB)
		pagesNeeded := (newPtr - memSize + 65535) / 65536
		if _, ok := i.memory.Grow(pagesNeeded); !ok {
			return 0, fmt.Errorf("failed to grow memory by %d pages", pagesNeeded)
		}
	}

	globalAllocator.nextPtr = newPtr
	return aligned, nil
}

// MemorySize returns the current memory size in bytes.
func (i *Instance) MemorySize() uint32 {
	return i.memory.Size()
}

// EvalFlags matching TclEvalFlags enum
const (
	EvalLocal  uint32 = 0
	EvalGlobal uint32 = 1
)

// InterpInit calls wasm_interp_init to register builtin commands.
func (i *Instance) InterpInit(interpID TclInterp) error {
	if i.interpInit == nil {
		return fmt.Errorf("wasm_interp_init not exported")
	}
	_, err := i.interpInit.Call(i.runtime.ctx, uint64(interpID))
	return err
}

// ScriptEval calls wasm_script_eval to evaluate a script.
// Returns the result code.
func (i *Instance) ScriptEval(interpID TclInterp, scriptPtr, scriptLen, flags uint32) (TclResult, error) {
	if i.scriptEval == nil {
		return ResultError, fmt.Errorf("wasm_script_eval not exported")
	}
	results, err := i.scriptEval.Call(i.runtime.ctx, uint64(interpID), uint64(scriptPtr), uint64(scriptLen), uint64(flags))
	if err != nil {
		return ResultError, err
	}
	if len(results) == 0 {
		return ResultError, fmt.Errorf("wasm_script_eval returned no results")
	}
	return TclResult(results[0]), nil
}

// ScriptEvalObj calls wasm_script_eval_obj to evaluate a script object.
func (i *Instance) ScriptEvalObj(interpID TclInterp, scriptObj TclObj, flags uint32) (TclResult, error) {
	if i.scriptEvalObj == nil {
		return ResultError, fmt.Errorf("wasm_script_eval_obj not exported")
	}
	results, err := i.scriptEvalObj.Call(i.runtime.ctx, uint64(interpID), uint64(scriptObj), uint64(flags))
	if err != nil {
		return ResultError, err
	}
	if len(results) == 0 {
		return ResultError, fmt.Errorf("wasm_script_eval_obj returned no results")
	}
	return TclResult(results[0]), nil
}

// CommandExec calls wasm_command_exec to execute a parsed command.
func (i *Instance) CommandExec(interpID TclInterp, commandObj TclObj, flags uint32) (TclResult, error) {
	if i.commandExec == nil {
		return ResultError, fmt.Errorf("wasm_command_exec not exported")
	}
	results, err := i.commandExec.Call(i.runtime.ctx, uint64(interpID), uint64(commandObj), uint64(flags))
	if err != nil {
		return ResultError, err
	}
	if len(results) == 0 {
		return ResultError, fmt.Errorf("wasm_command_exec returned no results")
	}
	return TclResult(results[0]), nil
}
