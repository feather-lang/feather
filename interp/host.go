package interp

// Host is a deprecated alias for Interp.
// Use NewInterp() instead of NewHost().
//
// Deprecated: Host exists only for backward compatibility.
// All functionality has been merged into Interp.
type Host struct {
	Interp   *Interp
	Commands map[string]CommandFunc
}

// NewHost creates a new interpreter.
//
// Deprecated: Use NewInterp() instead.
func NewHost() *Host {
	i := NewInterp()
	return &Host{
		Interp:   i,
		Commands: i.Commands,
	}
}

// Register adds a command to the interpreter.
//
// Deprecated: Use Interp.Register() instead.
func (h *Host) Register(name string, fn CommandFunc) {
	h.Interp.Register(name, fn)
}

// Parse parses a script and returns the parse result.
//
// Deprecated: Use Interp.Parse() instead.
func (h *Host) Parse(script string) ParseResult {
	return h.Interp.Parse(script)
}

// Eval evaluates a script and returns the result as a string.
//
// Deprecated: Use Interp.Eval() instead.
func (h *Host) Eval(script string) (string, error) {
	return h.Interp.Eval(script)
}

// EvalTyped evaluates a script and returns typed result info.
//
// Deprecated: Use Interp.EvalTyped() instead.
func (h *Host) EvalTyped(script string) (ResultInfo, error) {
	return h.Interp.EvalTyped(script)
}

// Close releases resources associated with the interpreter.
//
// Deprecated: Use Interp.Close() instead.
func (h *Host) Close() {
	h.Interp.Close()
}

// GetForeignMethods returns the method names for a foreign type.
func (h *Host) GetForeignMethods(typeName string) []string {
	return h.Interp.GetForeignMethods(typeName)
}

// GetForeignStringRep returns a custom string representation for a foreign object.
func (h *Host) GetForeignStringRep(obj FeatherObj) string {
	return h.Interp.GetForeignStringRep(obj)
}

// DefineTypeHost registers a foreign type using the deprecated Host.
// Deprecated: Use DefineType with *Interp instead.
func DefineTypeHost[T any](host *Host, typeName string, def ForeignTypeDef[T]) error {
	return DefineType[T](host.Interp, typeName, def)
}
