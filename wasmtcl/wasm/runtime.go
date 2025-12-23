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
		scriptEval:    mod.ExportedFunction("tcl_script_eval"),
		interpInit:    mod.ExportedFunction("tcl_interp_init"),
		parseInit:     mod.ExportedFunction("tcl_parse_init"),
		parseCommand:  mod.ExportedFunction("tcl_parse_command"),
		commandExec:   mod.ExportedFunction("tcl_command_exec"),
		scriptEvalObj: mod.ExportedFunction("tcl_script_eval_obj"),
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

// Allocate allocates memory in the WASM module.
// For now this is a stub - proper allocation requires heap management.
func (i *Instance) Allocate(size uint32) (uint32, error) {
	// This is a placeholder - in Phase 2 we'll implement proper allocation
	// using __heap_base and a simple bump allocator or calling malloc
	return 0, fmt.Errorf("allocation not yet implemented")
}

// MemorySize returns the current memory size in bytes.
func (i *Instance) MemorySize() uint32 {
	return i.memory.Size()
}
