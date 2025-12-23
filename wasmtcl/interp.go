// Package wasmtcl provides a pure-Go TCL interpreter using WebAssembly.
package wasmtcl

import (
	"fmt"
	"strconv"
	"strings"
	"sync"

	"github.com/dhamidi/tclc/wasmtcl/wasm"
)

// TclResult codes matching the C enum
type TclResult = wasm.TclResult

const (
	ResultOK       = wasm.ResultOK
	ResultError    = wasm.ResultError
	ResultReturn   = wasm.ResultReturn
	ResultBreak    = wasm.ResultBreak
	ResultContinue = wasm.ResultContinue
)

// TclObj is a handle to a TCL object
type TclObj = wasm.TclObj

// CommandFunc is the signature for host command implementations.
type CommandFunc func(i *Interp, cmd TclObj, args []TclObj) TclResult

// varLink represents a link to a variable in another frame (for upvar)
type varLink struct {
	targetLevel int
	targetName  string
}

// CallFrame represents an execution frame on the call stack.
type CallFrame struct {
	cmd   TclObj
	args  TclObj
	vars  map[string]TclObj
	links map[string]varLink
	level int
}

// Procedure represents a user-defined procedure
type Procedure struct {
	name   TclObj
	params TclObj
	body   TclObj
}

// CommandType indicates the type of a command
type CommandType int

const (
	CmdNone    CommandType = 0
	CmdBuiltin CommandType = 1
	CmdProc    CommandType = 2
)

// Command represents an entry in the unified command table
type Command struct {
	cmdType       CommandType
	canonicalName string
	proc          *Procedure
}

// Object represents a TCL object with shimmering support
type Object struct {
	stringVal string
	intVal    int64
	isInt     bool
	dblVal    float64
	isDouble  bool
	listItems []TclObj
	isList    bool
}

// Interp represents a TCL interpreter instance
type Interp struct {
	instance       *wasm.Instance
	objects        map[TclObj]*Object
	commands       map[string]*Command
	globalNS       TclObj
	nextID         TclObj
	result         TclObj
	returnOptions  TclObj
	frames         []*CallFrame
	active         int
	recursionLimit int
	mu             sync.Mutex

	UnknownHandler CommandFunc
}

// DefaultRecursionLimit is the default maximum call stack depth.
const DefaultRecursionLimit = 1000

// NewInterp creates a new interpreter from a WASM instance.
func NewInterp(instance *wasm.Instance) *Interp {
	interp := &Interp{
		instance: instance,
		objects:  make(map[TclObj]*Object),
		commands: make(map[string]*Command),
		nextID:   1,
	}

	globalFrame := &CallFrame{
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: 0,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0

	interp.globalNS = interp.internString("::")

	return interp
}

// Instance returns the underlying WASM instance.
func (i *Interp) Instance() *wasm.Instance {
	return i.instance
}

// SetRecursionLimit sets the maximum call stack depth.
func (i *Interp) SetRecursionLimit(limit int) {
	i.mu.Lock()
	defer i.mu.Unlock()
	if limit <= 0 {
		i.recursionLimit = DefaultRecursionLimit
	} else {
		i.recursionLimit = limit
	}
}

func (i *Interp) getRecursionLimit() int {
	if i.recursionLimit <= 0 {
		return DefaultRecursionLimit
	}
	return i.recursionLimit
}

// internString stores a string and returns its handle
func (i *Interp) internString(s string) TclObj {
	i.mu.Lock()
	defer i.mu.Unlock()

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{stringVal: s}
	return id
}

// InternString stores a string and returns its handle (public).
func (i *Interp) InternString(s string) TclObj {
	return i.internString(s)
}

// getObject retrieves an object by handle
func (i *Interp) getObject(h TclObj) *Object {
	i.mu.Lock()
	defer i.mu.Unlock()
	return i.objects[h]
}

// GetString returns the string representation of an object.
// Performs shimmering: converts int/double/list representations to string as needed.
func (i *Interp) GetString(h TclObj) string {
	if obj := i.getObject(h); obj != nil {
		if obj.isInt && obj.stringVal == "" {
			obj.stringVal = fmt.Sprintf("%d", obj.intVal)
		}
		if obj.isDouble && obj.stringVal == "" {
			obj.stringVal = strconv.FormatFloat(obj.dblVal, 'g', -1, 64)
		}
		if obj.isList && obj.stringVal == "" {
			obj.stringVal = i.listToValue(obj)
		}
		return obj.stringVal
	}
	return ""
}

// GetInt returns the integer representation of an object.
// Performs shimmering: parses string representation as integer if needed.
func (i *Interp) GetInt(h TclObj) (int64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	if obj.isInt {
		return obj.intVal, nil
	}
	if obj.isDouble {
		obj.intVal = int64(obj.dblVal)
		obj.isInt = true
		return obj.intVal, nil
	}
	val, err := strconv.ParseInt(obj.stringVal, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("expected integer but got %q", obj.stringVal)
	}
	obj.intVal = val
	obj.isInt = true
	return val, nil
}

// GetDouble returns the floating-point representation of an object.
func (i *Interp) GetDouble(h TclObj) (float64, error) {
	obj := i.getObject(h)
	if obj == nil {
		return 0, fmt.Errorf("nil object")
	}
	if obj.isDouble {
		return obj.dblVal, nil
	}
	if obj.isInt {
		obj.dblVal = float64(obj.intVal)
		obj.isDouble = true
		return obj.dblVal, nil
	}
	val, err := strconv.ParseFloat(obj.stringVal, 64)
	if err != nil {
		return 0, fmt.Errorf("expected floating-point number but got %q", obj.stringVal)
	}
	obj.dblVal = val
	obj.isDouble = true
	return val, nil
}

// GetList returns the list representation of an object.
func (i *Interp) GetList(h TclObj) ([]TclObj, error) {
	obj := i.getObject(h)
	if obj == nil {
		return nil, fmt.Errorf("nil object")
	}
	if obj.isList {
		return obj.listItems, nil
	}
	items, err := i.parseList(obj.stringVal)
	if err != nil {
		return nil, err
	}
	obj.listItems = items
	obj.isList = true
	return items, nil
}

// parseList parses a TCL list string into a slice of object handles.
func (i *Interp) parseList(s string) ([]TclObj, error) {
	var items []TclObj
	s = strings.TrimSpace(s)
	if s == "" {
		return items, nil
	}

	pos := 0
	for pos < len(s) {
		for pos < len(s) && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n') {
			pos++
		}
		if pos >= len(s) {
			break
		}

		var elem string
		if s[pos] == '{' {
			depth := 1
			start := pos + 1
			pos++
			for pos < len(s) && depth > 0 {
				if s[pos] == '{' {
					depth++
				} else if s[pos] == '}' {
					depth--
				}
				pos++
			}
			if depth != 0 {
				return nil, fmt.Errorf("unmatched brace in list")
			}
			elem = s[start : pos-1]
		} else if s[pos] == '"' {
			start := pos + 1
			pos++
			for pos < len(s) && s[pos] != '"' {
				if s[pos] == '\\' && pos+1 < len(s) {
					pos++
				}
				pos++
			}
			if pos >= len(s) {
				return nil, fmt.Errorf("unmatched quote in list")
			}
			elem = s[start:pos]
			pos++
		} else {
			start := pos
			for pos < len(s) && s[pos] != ' ' && s[pos] != '\t' && s[pos] != '\n' {
				pos++
			}
			elem = s[start:pos]
		}
		items = append(items, i.internString(elem))
	}
	return items, nil
}

// listToValue converts a list object to its TCL value string representation.
func (i *Interp) listToValue(obj *Object) string {
	if obj == nil {
		return ""
	}
	if !obj.isList {
		if obj.isInt {
			return fmt.Sprintf("%d", obj.intVal)
		}
		return obj.stringVal
	}
	var result string
	for idx, itemHandle := range obj.listItems {
		itemObj := i.getObject(itemHandle)
		if itemObj == nil {
			continue
		}
		if idx > 0 {
			result += " "
		}
		if itemObj.isInt {
			result += fmt.Sprintf("%d", itemObj.intVal)
		} else if itemObj.isList {
			nested := i.listToValue(itemObj)
			if len(itemObj.listItems) > 0 || strings.ContainsAny(nested, " \t\n") {
				result += "{" + nested + "}"
			} else {
				result += nested
			}
		} else {
			if strings.ContainsAny(itemObj.stringVal, " \t\n{}") {
				result += "{" + itemObj.stringVal + "}"
			} else {
				result += itemObj.stringVal
			}
		}
	}
	return result
}

// SetResult sets the interpreter's result to the given object.
func (i *Interp) SetResult(obj TclObj) {
	i.result = obj
}

// SetResultString sets the interpreter's result to a string value.
func (i *Interp) SetResultString(s string) {
	i.result = i.internString(s)
}

// SetErrorString sets the interpreter's result to an error message.
func (i *Interp) SetErrorString(s string) {
	i.result = i.internString(s)
}

// Result returns the current result string.
func (i *Interp) Result() string {
	return i.GetString(i.result)
}

// SetVar sets a variable by name to a string value in the current frame.
func (i *Interp) SetVar(name, value string) {
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	frame.vars[name] = i.nextID
	i.objects[i.nextID] = &Object{stringVal: value}
	i.nextID++
}

// GetVar returns the string value of a variable from the current frame.
func (i *Interp) GetVar(name string) string {
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	if val, ok := frame.vars[name]; ok {
		if obj := i.objects[val]; obj != nil {
			return obj.stringVal
		}
	}
	return ""
}

// CreateInt creates an integer object.
func (i *Interp) CreateInt(val int64) TclObj {
	i.mu.Lock()
	defer i.mu.Unlock()
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{intVal: val, isInt: true}
	return id
}

// CreateDouble creates a double object.
func (i *Interp) CreateDouble(val float64) TclObj {
	i.mu.Lock()
	defer i.mu.Unlock()
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{dblVal: val, isDouble: true}
	return id
}

// CreateList creates an empty list object.
func (i *Interp) CreateList() TclObj {
	i.mu.Lock()
	defer i.mu.Unlock()
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{listItems: nil, isList: true}
	return id
}

// ListPush appends an item to a list.
func (i *Interp) ListPush(list, item TclObj) TclObj {
	obj := i.getObject(list)
	if obj == nil || !obj.isList {
		return list
	}
	i.mu.Lock()
	obj.listItems = append(obj.listItems, item)
	obj.stringVal = ""
	i.mu.Unlock()
	return list
}

// ListLength returns the length of a list.
func (i *Interp) ListLength(list TclObj) int {
	obj := i.getObject(list)
	if obj == nil {
		return 0
	}
	if !obj.isList {
		items, err := i.GetList(list)
		if err != nil {
			return 0
		}
		return len(items)
	}
	return len(obj.listItems)
}

// ListAt returns the element at index.
func (i *Interp) ListAt(list TclObj, index int) TclObj {
	obj := i.getObject(list)
	if obj == nil {
		return 0
	}
	if !obj.isList {
		items, err := i.GetList(list)
		if err != nil || index < 0 || index >= len(items) {
			return 0
		}
		return items[index]
	}
	if index < 0 || index >= len(obj.listItems) {
		return 0
	}
	return obj.listItems[index]
}

// ListShift removes and returns the first element.
func (i *Interp) ListShift(list TclObj) TclObj {
	obj := i.getObject(list)
	if obj == nil || !obj.isList || len(obj.listItems) == 0 {
		return 0
	}
	i.mu.Lock()
	item := obj.listItems[0]
	obj.listItems = obj.listItems[1:]
	obj.stringVal = ""
	i.mu.Unlock()
	return item
}

// ListPop removes and returns the last element.
func (i *Interp) ListPop(list TclObj) TclObj {
	obj := i.getObject(list)
	if obj == nil || !obj.isList || len(obj.listItems) == 0 {
		return 0
	}
	i.mu.Lock()
	last := len(obj.listItems) - 1
	item := obj.listItems[last]
	obj.listItems = obj.listItems[:last]
	obj.stringVal = ""
	i.mu.Unlock()
	return item
}

// ListUnshift prepends an item to a list.
func (i *Interp) ListUnshift(list, item TclObj) TclObj {
	obj := i.getObject(list)
	if obj == nil || !obj.isList {
		return list
	}
	i.mu.Lock()
	obj.listItems = append([]TclObj{item}, obj.listItems...)
	obj.stringVal = ""
	i.mu.Unlock()
	return list
}

// ListFrom creates a new list from an object (parses string if needed).
func (i *Interp) ListFrom(obj TclObj) TclObj {
	items, err := i.GetList(obj)
	if err != nil {
		return i.CreateList()
	}
	newList := i.CreateList()
	listObj := i.getObject(newList)
	i.mu.Lock()
	listObj.listItems = make([]TclObj, len(items))
	copy(listObj.listItems, items)
	i.mu.Unlock()
	return newList
}

// IsNil returns true if the object handle is nil (0).
func (i *Interp) IsNil(obj TclObj) bool {
	return obj == 0
}

// StringConcat concatenates two string objects.
func (i *Interp) StringConcat(a, b TclObj) TclObj {
	strA := i.GetString(a)
	strB := i.GetString(b)
	return i.internString(strA + strB)
}

// StringCompare compares two strings using Unicode ordering.
func (i *Interp) StringCompare(a, b TclObj) int {
	strA := i.GetString(a)
	strB := i.GetString(b)
	return strings.Compare(strA, strB)
}

// Frame operations

// FramePush adds a new call frame.
func (i *Interp) FramePush(cmd, args TclObj) TclResult {
	i.mu.Lock()
	defer i.mu.Unlock()

	if len(i.frames) >= i.getRecursionLimit() {
		return ResultError
	}

	frame := &CallFrame{
		cmd:   cmd,
		args:  args,
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: len(i.frames),
	}
	i.frames = append(i.frames, frame)
	i.active = frame.level
	return ResultOK
}

// FramePop removes the topmost frame.
func (i *Interp) FramePop() TclResult {
	i.mu.Lock()
	defer i.mu.Unlock()

	if len(i.frames) <= 1 {
		return ResultError
	}
	i.frames = i.frames[:len(i.frames)-1]
	i.active = len(i.frames) - 1
	return ResultOK
}

// FrameLevel returns the current frame level.
func (i *Interp) FrameLevel() int {
	i.mu.Lock()
	defer i.mu.Unlock()
	return i.active
}

// FrameSetActive sets the active frame level.
func (i *Interp) FrameSetActive(level int) TclResult {
	i.mu.Lock()
	defer i.mu.Unlock()
	if level < 0 || level >= len(i.frames) {
		return ResultError
	}
	i.active = level
	return ResultOK
}

// FrameSize returns the size of the call stack.
func (i *Interp) FrameSize() int {
	i.mu.Lock()
	defer i.mu.Unlock()
	return len(i.frames)
}

// FrameInfo returns info about a frame at the given level.
func (i *Interp) FrameInfo(level int) (cmd, args TclObj, ok bool) {
	i.mu.Lock()
	defer i.mu.Unlock()
	if level < 0 || level >= len(i.frames) {
		return 0, 0, false
	}
	frame := i.frames[level]
	return frame.cmd, frame.args, true
}

// Variable operations with frame awareness

// VarGet returns the value of a variable in the current frame.
func (i *Interp) VarGet(name TclObj) TclObj {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	frame := i.frames[i.active]

	if link, ok := frame.links[nameStr]; ok {
		if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
			targetFrame := i.frames[link.targetLevel]
			if val, ok := targetFrame.vars[link.targetName]; ok {
				return val
			}
		}
		return 0
	}

	if val, ok := frame.vars[nameStr]; ok {
		return val
	}
	return 0
}

// VarSet sets a variable in the current frame.
func (i *Interp) VarSet(name, value TclObj) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	frame := i.frames[i.active]

	if link, ok := frame.links[nameStr]; ok {
		if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
			targetFrame := i.frames[link.targetLevel]
			targetFrame.vars[link.targetName] = value
			return
		}
	}

	frame.vars[nameStr] = value
}

// VarUnset removes a variable from the current frame.
func (i *Interp) VarUnset(name TclObj) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	frame := i.frames[i.active]

	if link, ok := frame.links[nameStr]; ok {
		if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
			targetFrame := i.frames[link.targetLevel]
			delete(targetFrame.vars, link.targetName)
		}
		delete(frame.links, nameStr)
		return
	}

	delete(frame.vars, nameStr)
}

// VarExists checks if a variable exists in the current frame.
func (i *Interp) VarExists(name TclObj) bool {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	frame := i.frames[i.active]

	if link, ok := frame.links[nameStr]; ok {
		if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
			targetFrame := i.frames[link.targetLevel]
			_, exists := targetFrame.vars[link.targetName]
			return exists
		}
		return false
	}

	_, exists := frame.vars[nameStr]
	return exists
}

// VarLink creates an upvar link.
func (i *Interp) VarLink(local TclObj, targetLevel int, target TclObj) {
	localStr := i.GetString(local)
	targetStr := i.GetString(target)
	i.mu.Lock()
	defer i.mu.Unlock()

	frame := i.frames[i.active]
	frame.links[localStr] = varLink{
		targetLevel: targetLevel,
		targetName:  targetStr,
	}
}

// Procedure operations

// ProcDefine defines a new procedure.
func (i *Interp) ProcDefine(name, params, body TclObj) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	i.commands[nameStr] = &Command{
		cmdType:       CmdProc,
		canonicalName: nameStr,
		proc: &Procedure{
			name:   name,
			params: params,
			body:   body,
		},
	}
}

// ProcExists checks if a procedure exists.
func (i *Interp) ProcExists(name TclObj) bool {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	cmd, ok := i.commands[nameStr]
	return ok && cmd.cmdType == CmdProc
}

// ProcParams returns the parameter list of a procedure.
func (i *Interp) ProcParams(name TclObj) (TclObj, bool) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	cmd, ok := i.commands[nameStr]
	if !ok || cmd.cmdType != CmdProc || cmd.proc == nil {
		return 0, false
	}
	return cmd.proc.params, true
}

// ProcBody returns the body of a procedure.
func (i *Interp) ProcBody(name TclObj) (TclObj, bool) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	cmd, ok := i.commands[nameStr]
	if !ok || cmd.cmdType != CmdProc || cmd.proc == nil {
		return 0, false
	}
	return cmd.proc.body, true
}

// ProcNames returns a list of all command names.
func (i *Interp) ProcNames(namespace TclObj) TclObj {
	i.mu.Lock()
	defer i.mu.Unlock()

	list := i.nextID
	i.nextID++
	var items []TclObj
	for name := range i.commands {
		items = append(items, i.internStringLocked(name))
	}
	i.objects[list] = &Object{listItems: items, isList: true}
	return list
}

func (i *Interp) internStringLocked(s string) TclObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{stringVal: s}
	return id
}

// RegisterCommand records that a builtin command exists.
func (i *Interp) RegisterCommand(name TclObj) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	if _, exists := i.commands[nameStr]; !exists {
		i.commands[nameStr] = &Command{
			cmdType:       CmdBuiltin,
			canonicalName: nameStr,
		}
	}
}

// CommandLookup checks if a command exists and returns its type.
func (i *Interp) CommandLookup(name TclObj) (CommandType, string) {
	nameStr := i.GetString(name)
	i.mu.Lock()
	defer i.mu.Unlock()

	cmd, ok := i.commands[nameStr]
	if !ok {
		return CmdNone, ""
	}
	return cmd.cmdType, cmd.canonicalName
}

// CommandRename renames or deletes a command.
func (i *Interp) CommandRename(oldName, newName TclObj) TclResult {
	oldStr := i.GetString(oldName)
	newStr := i.GetString(newName)
	i.mu.Lock()
	defer i.mu.Unlock()

	cmd, ok := i.commands[oldStr]
	if !ok {
		i.result = i.internStringLocked(fmt.Sprintf("can't rename \"%s\": command doesn't exist", oldStr))
		return ResultError
	}

	if newStr == "" {
		delete(i.commands, oldStr)
		return ResultOK
	}

	if _, exists := i.commands[newStr]; exists {
		i.result = i.internStringLocked(fmt.Sprintf("can't rename to \"%s\": command already exists", newStr))
		return ResultError
	}

	delete(i.commands, oldStr)
	i.commands[newStr] = cmd
	return ResultOK
}

// ResolveNamespace resolves a namespace path.
func (i *Interp) ResolveNamespace(path TclObj) (TclObj, bool) {
	pathStr := i.GetString(path)
	if pathStr == "" || pathStr == "::" {
		return i.globalNS, true
	}
	return 0, false
}

// Interp operations

// GetResult returns the interpreter's result object.
func (i *Interp) GetResult() TclObj {
	return i.result
}

// ResetResult clears the interpreter's result.
func (i *Interp) ResetResult() {
	i.result = 0
	i.returnOptions = 0
}

// SetReturnOptions sets the return options dictionary.
func (i *Interp) SetReturnOptions(options TclObj) TclResult {
	i.returnOptions = options
	return ResultOK
}

// GetReturnOptions returns the return options dictionary.
func (i *Interp) GetReturnOptions(code TclResult) TclObj {
	return i.returnOptions
}

// EvalError represents an evaluation error
type EvalError struct {
	Message string
}

func (e *EvalError) Error() string {
	return e.Message
}
