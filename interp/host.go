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
	// Also register in interpreter's namespace storage for enumeration.
	// These are Go commands dispatched via bind.unknown, not C builtins.
	// We set builtin to nil so the C code falls through to unknown handler.
	h.Interp.globalNamespace.commands[name] = &Command{
		cmdType: CmdBuiltin,
		builtin: nil, // nil means dispatch via bind.unknown
	}
}

// Parse parses a script and returns the parse result.
func (h *Host) Parse(script string) ParseResult {
	return h.Interp.Parse(script)
}

// Eval evaluates a script and returns the result as a string.
func (h *Host) Eval(script string) (string, error) {
	return h.Interp.Eval(script)
}

// ResultInfo contains type information about a TCL value.
type ResultInfo struct {
	String      string
	IsInt       bool
	IntVal      int64
	IsDouble    bool
	DoubleVal   float64
	IsList      bool
	ListItems   []ResultInfo
	IsDict      bool
	DictKeys    []string
	DictValues  map[string]ResultInfo
	IsForeign   bool
	ForeignType string
}

// EvalTyped evaluates a script and returns typed result info.
func (h *Host) EvalTyped(script string) (ResultInfo, error) {
	_, err := h.Interp.Eval(script)
	if err != nil {
		return ResultInfo{}, err
	}
	return h.Interp.getResultInfo(h.Interp.result), nil
}

// getResultInfo extracts type information from a FeatherObj.
func (i *Interp) getResultInfo(h FeatherObj) ResultInfo {
	obj := i.objects[h]
	if obj == nil {
		return ResultInfo{String: ""}
	}

	info := ResultInfo{
		String:      i.GetString(h),
		IsInt:       obj.isInt,
		IsDouble:    obj.isDouble,
		IsList:      obj.isList,
		IsDict:      obj.isDict,
		IsForeign:   obj.isForeign,
		ForeignType: obj.foreignType,
	}

	if obj.isInt {
		info.IntVal = obj.intVal
	}
	if obj.isDouble {
		info.DoubleVal = obj.dblVal
	}
	if obj.isList {
		info.ListItems = make([]ResultInfo, len(obj.listItems))
		for idx, item := range obj.listItems {
			info.ListItems[idx] = i.getResultInfo(item)
		}
	}
	if obj.isDict {
		info.DictKeys = obj.dictOrder
		info.DictValues = make(map[string]ResultInfo, len(obj.dictItems))
		for k, v := range obj.dictItems {
			info.DictValues[k] = i.getResultInfo(v)
		}
	}

	return info
}

// Close releases resources associated with the host.
func (h *Host) Close() {
	h.Interp.Close()
}

// dispatch handles command lookup and execution.
func (h *Host) dispatch(i *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
	cmdStr := i.GetString(cmd)
	if fn, ok := h.Commands[cmdStr]; ok {
		return fn(i, cmd, args)
	}
	i.SetErrorString("invalid command name \"" + cmdStr + "\"")
	return ResultError
}
