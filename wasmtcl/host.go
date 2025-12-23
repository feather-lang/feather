package wasmtcl

import (
	"context"
	"fmt"

	"github.com/dhamidi/tclc/wasmtcl/wasm"
)

// Host contains the interpreter and registered commands.
type Host struct {
	runtime  *wasm.Runtime
	instance *wasm.Instance
	Interp   *Interp
	Commands map[string]CommandFunc
}

// NewHost creates a new Host with a WASM-based interpreter.
func NewHost(ctx context.Context) (*Host, error) {
	runtime, err := wasm.NewRuntime(ctx)
	if err != nil {
		return nil, fmt.Errorf("create runtime: %w", err)
	}

	h := &Host{
		runtime:  runtime,
		Commands: make(map[string]CommandFunc),
	}

	// Create the WASM instance
	instance, err := runtime.NewInstance(nil)
	if err != nil {
		runtime.Close()
		return nil, fmt.Errorf("create instance: %w", err)
	}
	h.instance = instance

	// Create the interpreter using the instance
	h.Interp = NewInterp(instance)

	// Register the interpreter with the callback system
	wasm.RegisterInterp(instance.ID(), h.Interp)

	// Set up the unknown handler to dispatch to registered commands
	h.Interp.UnknownHandler = h.dispatch

	// Initialize the interpreter (registers builtins)
	if err := h.Interp.Initialize(); err != nil {
		h.Close()
		return nil, fmt.Errorf("initialize interpreter: %w", err)
	}

	return h, nil
}

// Register adds a command to the host.
func (h *Host) Register(name string, fn CommandFunc) {
	h.Commands[name] = fn
	// Also register in interpreter's commands map for enumeration
	h.Interp.mu.Lock()
	h.Interp.commands[name] = &Command{
		cmdType:       CmdBuiltin,
		canonicalName: name,
	}
	h.Interp.mu.Unlock()
}

// Parse parses a script and returns the parse result.
func (h *Host) Parse(script string) ParseResult {
	return h.Interp.Parse(script)
}

// Eval evaluates a script and returns the result as a string.
func (h *Host) Eval(script string) (string, error) {
	return h.Interp.Eval(script)
}

// Close releases resources associated with the host.
func (h *Host) Close() {
	if h.instance != nil {
		wasm.UnregisterInterp(h.instance.ID())
		h.instance.Close()
	}
	if h.runtime != nil {
		h.runtime.Close()
	}
}

// dispatch handles command lookup and execution.
func (h *Host) dispatch(i *Interp, cmd TclObj, args []TclObj) TclResult {
	cmdStr := i.GetString(cmd)
	if fn, ok := h.Commands[cmdStr]; ok {
		return fn(i, cmd, args)
	}
	i.SetErrorString("invalid command name \"" + cmdStr + "\"")
	return ResultError
}
