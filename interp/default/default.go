// Package default provides a pre-configured interpreter with standard host commands.
package defaults

import (
	"fmt"
	"strings"

	"github.com/dhamidi/tclc/interp"
)

// CommandFunc is the signature for host command implementations
type CommandFunc func(i *interp.Interp, args []string) (string, error)

// Host contains the interpreter and registered commands
type Host struct {
	Interp   *interp.Interp
	Commands map[string]CommandFunc
}

// NewHost creates a new Host with an interpreter and default commands
func NewHost() *Host {
	h := &Host{
		Interp:   interp.NewInterp(),
		Commands: make(map[string]CommandFunc),
	}

	// Register default commands
	h.Register("say-hello", cmdSayHello)

	// Set up the unknown handler to dispatch to registered commands
	h.Interp.UnknownHandler = h.dispatch

	return h
}

// Register adds a command to the host
func (h *Host) Register(name string, fn CommandFunc) {
	h.Commands[name] = fn
}

// Eval evaluates a script
func (h *Host) Eval(script string) (string, error) {
	return h.Interp.Eval(script)
}

// dispatch handles command lookup and execution
func (h *Host) dispatch(i *interp.Interp, cmd string, args []string) (string, error) {
	cmd = strings.TrimSpace(cmd)
	if fn, ok := h.Commands[cmd]; ok {
		return fn(i, args)
	}
	return "", fmt.Errorf("unknown command: %s", cmd)
}

// Default commands

func cmdSayHello(i *interp.Interp, args []string) (string, error) {
	return "hello", nil
}
