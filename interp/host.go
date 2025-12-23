package interp

// Host contains the interpreter and registered commands.
type Host struct {
	Interp   *Interp
	Commands map[string]CommandFunc
}

// NewHost creates a new Host with an interpreter.
func NewHost() *Host {
	h := &Host{
		Interp:   NewInterp(),
		Commands: make(map[string]CommandFunc),
	}

	// Set up the unknown handler to dispatch to registered commands
	h.Interp.UnknownHandler = h.dispatch

	return h
}

// Register adds a command to the host.
func (h *Host) Register(name string, fn CommandFunc) {
	h.Commands[name] = fn
	// Also register in interpreter's commands map for enumeration
	h.Interp.mu.Lock()
	h.Interp.commands[name] = struct{}{}
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
	h.Interp.Close()
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
