package interp

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#cgo LDFLAGS: -L${SRCDIR}/../build -ltclc -Wl,-rpath,${SRCDIR}/../build
#include "tclc.h"

// Implemented in callbacks.c
extern TclResult call_tcl_eval_obj(TclInterp interp, TclObj script, TclEvalFlags flags);
extern TclParseStatus call_tcl_parse(TclInterp interp, TclObj script);
extern void call_tcl_interp_init(TclInterp interp);
*/
import "C"

import (
	"sort"
	"strings"
)

// Go callback implementations - these are called from C via the wrappers in callbacks.c

//export goBindUnknown
func goBindUnknown(interp C.TclInterp, cmd C.TclObj, args C.TclObj, value *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}

	if i.UnknownHandler != nil {
		// Convert args TclObj (list) to []TclObj slice
		var argSlice []TclObj
		if args != 0 {
			o := i.getObject(TclObj(args))
			if o != nil && o.isList {
				argSlice = make([]TclObj, len(o.listItems))
				copy(argSlice, o.listItems)
			}
		}
		result := i.UnknownHandler(i, TclObj(cmd), argSlice)
		*value = C.TclObj(i.result)
		return C.TclResult(result)
	}

	return C.TCL_ERROR
}

//export goStringIntern
func goStringIntern(interp C.TclInterp, s *C.char, length C.size_t) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	goStr := C.GoStringN(s, C.int(length))
	return C.TclObj(i.internString(goStr))
}

//export goStringGet
func goStringGet(interp C.TclInterp, obj C.TclObj, length *C.size_t) *C.char {
	i := getInterp(interp)
	if i == nil {
		*length = 0
		return nil
	}
	// Use GetString for shimmering (int/list → string)
	str := i.GetString(TclObj(obj))
	if str == "" {
		// Check if object exists but has empty string
		o := i.getObject(TclObj(obj))
		if o == nil {
			*length = 0
			return nil
		}
	}
	// Cache the C string in the object to avoid memory issues
	o := i.getObject(TclObj(obj))
	if o.cstr == nil {
		o.cstr = C.CString(str)
	}
	*length = C.size_t(len(str))
	return o.cstr
}

//export goStringConcat
func goStringConcat(interp C.TclInterp, a C.TclObj, b C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetString for shimmering (int/list → string)
	strA := i.GetString(TclObj(a))
	strB := i.GetString(TclObj(b))
	return C.TclObj(i.internString(strA + strB))
}

//export goStringCompare
func goStringCompare(interp C.TclInterp, a C.TclObj, b C.TclObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetString for shimmering (int/list → string)
	strA := i.GetString(TclObj(a))
	strB := i.GetString(TclObj(b))
	// Go's string comparison is already Unicode-aware (UTF-8)
	if strA < strB {
		return -1
	}
	if strA > strB {
		return 1
	}
	return 0
}

//export goInterpSetResult
func goInterpSetResult(interp C.TclInterp, result C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.result = TclObj(result)
	return C.TCL_OK
}

//export goInterpGetResult
func goInterpGetResult(interp C.TclInterp) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.TclObj(i.result)
}

//export goInterpResetResult
func goInterpResetResult(interp C.TclInterp, result C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.result = 0
	return C.TCL_OK
}

//export goInterpSetReturnOptions
func goInterpSetReturnOptions(interp C.TclInterp, options C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.returnOptions = TclObj(options)
	return C.TCL_OK
}

//export goInterpGetReturnOptions
func goInterpGetReturnOptions(interp C.TclInterp, code C.TclResult) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.TclObj(i.returnOptions)
}

//export goListCreate
func goListCreate(interp C.TclInterp) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: []TclObj{}}
	return C.TclObj(id)
}

//export goListIsNil
func goListIsNil(interp C.TclInterp, obj C.TclObj) C.int {
	if obj == 0 {
		return 1
	}
	return 0
}

//export goListFrom
func goListFrom(interp C.TclInterp, obj C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Get the list items (with shimmering)
	items, err := i.GetList(TclObj(obj))
	if err != nil {
		return 0
	}
	// Create a new list with copied items
	id := i.nextID
	i.nextID++
	// Make a copy of the items slice
	copiedItems := make([]TclObj, len(items))
	copy(copiedItems, items)
	i.objects[id] = &Object{isList: true, listItems: copiedItems}
	return C.TclObj(id)
}

//export goListPush
func goListPush(interp C.TclInterp, list C.TclObj, item C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return list
	}
	o := i.getObject(TclObj(list))
	if o == nil || !o.isList {
		return list
	}
	o.listItems = append(o.listItems, TclObj(item))
	return list
}

//export goListPop
func goListPop(interp C.TclInterp, list C.TclObj) C.TclObj {
	return 0
}

//export goListUnshift
func goListUnshift(interp C.TclInterp, list C.TclObj, item C.TclObj) C.TclObj {
	return list
}

//export goListShift
func goListShift(interp C.TclInterp, list C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetList for shimmering (string → list)
	items, err := i.GetList(TclObj(list))
	if err != nil || len(items) == 0 {
		return 0
	}
	o := i.getObject(TclObj(list))
	if o == nil {
		return 0
	}
	first := o.listItems[0]
	o.listItems = o.listItems[1:]
	return C.TclObj(first)
}

//export goListLength
func goListLength(interp C.TclInterp, list C.TclObj) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetList for shimmering (string → list)
	items, err := i.GetList(TclObj(list))
	if err != nil {
		return 0
	}
	return C.size_t(len(items))
}

//export goListAt
func goListAt(interp C.TclInterp, list C.TclObj, index C.size_t) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	items, err := i.GetList(TclObj(list))
	if err != nil {
		return 0
	}
	idx := int(index)
	if idx < 0 || idx >= len(items) {
		return 0
	}
	return C.TclObj(items[idx])
}

//export goIntCreate
func goIntCreate(interp C.TclInterp, val C.int64_t) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{intVal: int64(val), isInt: true}
	return C.TclObj(id)
}

//export goIntGet
func goIntGet(interp C.TclInterp, obj C.TclObj, out *C.int64_t) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	val, err := i.GetInt(TclObj(obj))
	if err != nil {
		return C.TCL_ERROR
	}
	*out = C.int64_t(val)
	return C.TCL_OK
}

//export goDoubleCreate
func goDoubleCreate(interp C.TclInterp, val C.double) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{dblVal: float64(val), isDouble: true}
	return C.TclObj(id)
}

//export goDoubleGet
func goDoubleGet(interp C.TclInterp, obj C.TclObj, out *C.double) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	val, err := i.GetDouble(TclObj(obj))
	if err != nil {
		return C.TCL_ERROR
	}
	*out = C.double(val)
	return C.TCL_OK
}

//export goFramePush
func goFramePush(interp C.TclInterp, cmd C.TclObj, args C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	newLevel := len(i.frames)
	// Check recursion limit
	limit := i.recursionLimit
	if limit <= 0 {
		limit = DefaultRecursionLimit
	}
	if newLevel >= limit {
		// Set error message and return error
		errMsg := "too many nested evaluations (infinite loop?)"
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{stringVal: errMsg}
		i.result = id
		return C.TCL_ERROR
	}
	// Inherit namespace from current frame
	currentNS := i.globalNamespace
	if i.active < len(i.frames) && i.frames[i.active].ns != nil {
		currentNS = i.frames[i.active].ns
	}
	frame := &CallFrame{
		cmd:   TclObj(cmd),
		args:  TclObj(args),
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: newLevel,
		ns:    currentNS,
	}
	i.frames = append(i.frames, frame)
	i.active = newLevel
	return C.TCL_OK
}

//export goFramePop
func goFramePop(interp C.TclInterp) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	// Cannot pop the global frame (frame 0)
	if len(i.frames) <= 1 {
		return C.TCL_ERROR
	}
	i.frames = i.frames[:len(i.frames)-1]
	i.active = len(i.frames) - 1
	return C.TCL_OK
}

//export goFrameLevel
func goFrameLevel(interp C.TclInterp) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.size_t(i.active)
}

//export goFrameSetActive
func goFrameSetActive(interp C.TclInterp, level C.size_t) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	lvl := int(level)
	if lvl < 0 || lvl >= len(i.frames) {
		return C.TCL_ERROR
	}
	i.active = lvl
	return C.TCL_OK
}

//export goFrameSize
func goFrameSize(interp C.TclInterp) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.size_t(len(i.frames))
}

//export goFrameInfo
func goFrameInfo(interp C.TclInterp, level C.size_t, cmd *C.TclObj, args *C.TclObj, ns *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	lvl := int(level)
	if lvl < 0 || lvl >= len(i.frames) {
		return C.TCL_ERROR
	}
	frame := i.frames[lvl]
	*cmd = C.TclObj(frame.cmd)
	*args = C.TclObj(frame.args)
	// Return the frame's namespace
	if frame.ns != nil {
		*ns = C.TclObj(i.internString(frame.ns.fullPath))
	} else {
		*ns = C.TclObj(i.internString("::"))
	}
	return C.TCL_OK
}

//export goVarGet
func goVarGet(interp C.TclInterp, name C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	nameObj := i.getObject(TclObj(name))
	if nameObj == nil {
		return 0
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	originalVarName := varName // Save for trace firing
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					if val, ok := ns.vars[link.nsName]; ok {
						// Copy traces before unlocking
						traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
						copy(traces, i.varTraces[originalVarName])
						// Fire read traces
						if len(traces) > 0 {
							fireVarTraces(i, originalVarName, "read", traces)
						}
						return C.TclObj(val)
					}
				}
				return 0
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return 0
			}
		} else {
			break
		}
	}
	var result C.TclObj
	if val, ok := frame.vars[varName]; ok {
		result = C.TclObj(val)
	}
	// Copy traces before unlocking
	traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
	copy(traces, i.varTraces[originalVarName])
	// Fire read traces
	if len(traces) > 0 {
		fireVarTraces(i, originalVarName, "read", traces)
	}
	return result
}

//export goVarSet
func goVarSet(interp C.TclInterp, name C.TclObj, value C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameObj := i.getObject(TclObj(name))
	if nameObj == nil {
		return
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	originalVarName := varName // Save for trace firing
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					ns.vars[link.nsName] = TclObj(value)
				}
				// Copy traces before unlocking
				traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
				copy(traces, i.varTraces[originalVarName])
				// Fire write traces
				if len(traces) > 0 {
					fireVarTraces(i, originalVarName, "write", traces)
				}
				return
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return
			}
		} else {
			break
		}
	}
	frame.vars[varName] = TclObj(value)
	// Copy traces before unlocking
	traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
	copy(traces, i.varTraces[originalVarName])
	// Fire write traces
	if len(traces) > 0 {
		fireVarTraces(i, originalVarName, "write", traces)
	}
}

//export goVarUnset
func goVarUnset(interp C.TclInterp, name C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameObj := i.getObject(TclObj(name))
	if nameObj == nil {
		return
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	originalVarName := varName // Save for trace firing
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					delete(ns.vars, link.nsName)
				}
				// Copy traces before unlocking
				traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
				copy(traces, i.varTraces[originalVarName])
				// Fire unset traces
				if len(traces) > 0 {
					fireVarTraces(i, originalVarName, "unset", traces)
				}
				return
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return
			}
		} else {
			break
		}
	}
	delete(frame.vars, varName)
	// Copy traces before unlocking
	traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
	copy(traces, i.varTraces[originalVarName])
	// Fire unset traces
	if len(traces) > 0 {
		fireVarTraces(i, originalVarName, "unset", traces)
	}
}

//export goVarExists
func goVarExists(interp C.TclInterp, name C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameObj := i.getObject(TclObj(name))
	if nameObj == nil {
		return C.TCL_ERROR
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					if _, ok := ns.vars[link.nsName]; ok {
						return C.TCL_OK
					}
				}
				return C.TCL_ERROR
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return C.TCL_ERROR
			}
		} else {
			break
		}
	}
	if _, ok := frame.vars[varName]; ok {
		return C.TCL_OK
	}
	return C.TCL_ERROR
}

//export goVarLink
func goVarLink(interp C.TclInterp, local C.TclObj, target_level C.size_t, target C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	localObj := i.getObject(TclObj(local))
	targetObj := i.getObject(TclObj(target))
	if localObj == nil || targetObj == nil {
		return
	}
	frame := i.frames[i.active]
	frame.links[localObj.stringVal] = varLink{
		targetLevel: int(target_level),
		targetName:  targetObj.stringVal,
	}
}

//export goProcDefine
func goProcDefine(interp C.TclInterp, name C.TclObj, params C.TclObj, body C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameStr := i.GetString(TclObj(name))
	proc := &Procedure{
		name:   TclObj(name),
		params: TclObj(params),
		body:   TclObj(body),
	}
	cmd := &Command{
		cmdType: CmdProc,
		proc:    proc,
	}

	// Store in namespace's commands map
	// Split the qualified name into namespace and simple name
	var nsPath, simpleName string
	if strings.HasPrefix(nameStr, "::") {
		// Absolute path like "::foo::bar" or "::cmd"
		lastSep := strings.LastIndex(nameStr, "::")
		if lastSep == 0 {
			// Just "::cmd"
			nsPath = "::"
			simpleName = nameStr[2:]
		} else {
			nsPath = nameStr[:lastSep]
			simpleName = nameStr[lastSep+2:]
		}
	} else {
		// Should not happen - procs should always be fully qualified by now
		nsPath = "::"
		simpleName = nameStr
	}

	ns := i.ensureNamespace(nsPath)
	ns.commands[simpleName] = cmd
}

// lookupCommandByQualified looks up a command by its fully-qualified name.
// Returns the command and true if found, nil and false otherwise.
func (i *Interp) lookupCommandByQualified(nameStr string) (*Command, bool) {
	var nsPath, simpleName string
	if strings.HasPrefix(nameStr, "::") {
		lastSep := strings.LastIndex(nameStr, "::")
		if lastSep == 0 {
			nsPath = "::"
			simpleName = nameStr[2:]
		} else {
			nsPath = nameStr[:lastSep]
			simpleName = nameStr[lastSep+2:]
		}
	} else {
		nsPath = "::"
		simpleName = nameStr
	}

	ns, ok := i.namespaces[nsPath]
	if !ok {
		return nil, false
	}
	cmd, ok := ns.commands[simpleName]
	return cmd, ok
}

//export goProcExists
func goProcExists(interp C.TclInterp, name C.TclObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	nameStr := i.GetString(TclObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if ok && cmd.cmdType == CmdProc {
		return 1
	}
	return 0
}

//export goProcParams
func goProcParams(interp C.TclInterp, name C.TclObj, result *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameStr := i.GetString(TclObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if !ok || cmd.cmdType != CmdProc || cmd.proc == nil {
		return C.TCL_ERROR
	}
	*result = C.TclObj(cmd.proc.params)
	return C.TCL_OK
}

//export goProcBody
func goProcBody(interp C.TclInterp, name C.TclObj, result *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameStr := i.GetString(TclObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if !ok || cmd.cmdType != CmdProc || cmd.proc == nil {
		return C.TCL_ERROR
	}
	*result = C.TclObj(cmd.proc.body)
	return C.TCL_OK
}

// callCEval invokes the C interpreter
func callCEval(interpHandle TclInterp, scriptHandle TclObj) C.TclResult {
	return C.call_tcl_eval_obj(C.TclInterp(interpHandle), C.TclObj(scriptHandle), C.TCL_EVAL_LOCAL)
}

// fireVarTraces fires variable traces for the given operation.
// This function must be called WITHOUT holding the mutex.
func fireVarTraces(i *Interp, varName string, op string, traces []TraceEntry) {
	// Variable traces are invoked as: command name1 name2 op
	// - name1 is the variable name
	// - name2 is empty (array element, not supported)
	// - op is "read", "write", or "unset"
	for _, trace := range traces {
		// Check if this trace matches the operation
		ops := strings.Fields(trace.ops)
		matches := false
		for _, traceOp := range ops {
			if traceOp == op {
				matches = true
				break
			}
		}
		if !matches {
			continue
		}

		// Get the script command prefix - GetString acquires lock internally
		scriptStr := i.GetString(trace.script)
		// Build the full command: script name1 name2 op
		// We'll construct this as a string to eval
		cmd := scriptStr + " " + varName + " {} " + op
		cmdObj := i.internString(cmd)

		// Fire the trace by evaluating the command
		callCEval(i.handle, cmdObj)
	}
}

// fireCmdTraces fires command traces for the given operation.
// This function must be called WITHOUT holding the mutex.
func fireCmdTraces(i *Interp, oldName string, newName string, op string, traces []TraceEntry) {
	// Command traces are invoked as: command oldName newName op
	// - oldName is the original command name
	// - newName is the new name (empty for delete)
	// - op is "rename" or "delete"
	for _, trace := range traces {
		// Check if this trace matches the operation
		ops := strings.Fields(trace.ops)
		matches := false
		for _, traceOp := range ops {
			if traceOp == op {
				matches = true
				break
			}
		}
		if !matches {
			continue
		}

		// Get the script command prefix - GetString acquires lock internally
		scriptStr := i.GetString(trace.script)
		// Build the full command: script oldName newName op
		// Use display names (strip :: for global namespace commands)
		displayOld := i.DisplayName(oldName)
		displayNew := i.DisplayName(newName)
		// Empty strings must be properly quoted with {}
		quotedNew := displayNew
		if displayNew == "" {
			quotedNew = "{}"
		}
		cmd := scriptStr + " " + displayOld + " " + quotedNew + " " + op
		cmdObj := i.internString(cmd)

		// Fire the trace by evaluating the command
		callCEval(i.handle, cmdObj)
	}
}

// callCParse invokes the C parser
func callCParse(interpHandle TclInterp, scriptHandle TclObj) C.TclParseStatus {
	return C.call_tcl_parse(C.TclInterp(interpHandle), C.TclObj(scriptHandle))
}

// callCInterpInit invokes the C interpreter initialization
func callCInterpInit(interpHandle TclInterp) {
	C.call_tcl_interp_init(C.TclInterp(interpHandle))
}

//export goProcNames
func goProcNames(interp C.TclInterp, namespace C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}

	// Determine which namespace to list (default to global)
	nsPath := "::"
	if namespace != 0 {
		nsPath = i.GetString(TclObj(namespace))
	}
	if nsPath == "" {
		nsPath = "::"
	}

	ns, ok := i.namespaces[nsPath]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []TclObj{}}
		return C.TclObj(id)
	}

	// Collect command names and build fully-qualified names
	names := make([]string, 0, len(ns.commands))
	for name := range ns.commands {
		// Build fully-qualified name
		var fullName string
		if nsPath == "::" {
			fullName = "::" + name
		} else {
			fullName = nsPath + "::" + name
		}
		names = append(names, fullName)
	}

	// Sort for consistent ordering
	sort.Strings(names)

	// Create a list object with all names
	items := make([]TclObj, len(names))
	for idx, name := range names {
		items[idx] = i.internString(name)
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.TclObj(id)
}

//export goProcResolveNamespace
func goProcResolveNamespace(interp C.TclInterp, path C.TclObj, result *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	// For now, only the global namespace "::" exists
	// If path is nil, empty, or "::", return global namespace
	if path == 0 {
		*result = C.TclObj(i.globalNS)
		return C.TCL_OK
	}
	pathStr := i.GetString(TclObj(path))
	if pathStr == "" || pathStr == "::" {
		*result = C.TclObj(i.globalNS)
		return C.TCL_OK
	}
	// Any other namespace doesn't exist yet
	i.SetErrorString("namespace \"" + pathStr + "\" not found")
	return C.TCL_ERROR
}

//export goProcRegisterBuiltin
func goProcRegisterBuiltin(interp C.TclInterp, name C.TclObj, fn C.TclBuiltinCmd) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameStr := i.GetString(TclObj(name))

	// Split the qualified name and store in namespace
	var nsPath, simpleName string
	if strings.HasPrefix(nameStr, "::") {
		lastSep := strings.LastIndex(nameStr, "::")
		if lastSep == 0 {
			nsPath = "::"
			simpleName = nameStr[2:]
		} else {
			nsPath = nameStr[:lastSep]
			simpleName = nameStr[lastSep+2:]
		}
	} else {
		nsPath = "::"
		simpleName = nameStr
	}

	cmd := &Command{
		cmdType: CmdBuiltin,
		builtin: fn,
	}
	ns := i.ensureNamespace(nsPath)
	ns.commands[simpleName] = cmd
}

//export goProcLookup
func goProcLookup(interp C.TclInterp, name C.TclObj, fn *C.TclBuiltinCmd) C.TclCommandType {
	i := getInterp(interp)
	if i == nil {
		*fn = nil
		return C.TCL_CMD_NONE
	}
	nameStr := i.GetString(TclObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if !ok {
		*fn = nil
		return C.TCL_CMD_NONE
	}
	switch cmd.cmdType {
	case CmdBuiltin:
		*fn = cmd.builtin
		return C.TCL_CMD_BUILTIN
	case CmdProc:
		*fn = nil
		return C.TCL_CMD_PROC
	default:
		*fn = nil
		return C.TCL_CMD_NONE
	}
}

//export goProcRename
func goProcRename(interp C.TclInterp, oldName C.TclObj, newName C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	oldNameStr := i.GetString(TclObj(oldName))
	newNameStr := i.GetString(TclObj(newName))

	// Helper to split qualified name into namespace and simple name
	splitQualified := func(name string) (nsPath, simple string) {
		if strings.HasPrefix(name, "::") {
			lastSep := strings.LastIndex(name, "::")
			if lastSep == 0 {
				return "::", name[2:]
			}
			return name[:lastSep], name[lastSep+2:]
		}
		return "::", name
	}

	// Get old namespace location
	oldNsPath, oldSimple := splitQualified(oldNameStr)

	// Compute fully qualified name for trace lookups
	// Traces are always registered with fully qualified names
	oldQualified := oldNameStr
	if !strings.HasPrefix(oldNameStr, "::") {
		if oldNsPath == "::" {
			oldQualified = "::" + oldSimple
		} else {
			oldQualified = oldNsPath + "::" + oldSimple
		}
	}

	// Check if old command exists in namespace
	oldNs, ok := i.namespaces[oldNsPath]
	if !ok {
		i.SetErrorString("can't rename \"" + i.DisplayName(oldNameStr) + "\": command doesn't exist")
		return C.TCL_ERROR
	}
	cmd, ok := oldNs.commands[oldSimple]
	if !ok {
		i.SetErrorString("can't rename \"" + i.DisplayName(oldNameStr) + "\": command doesn't exist")
		return C.TCL_ERROR
	}

	// If newName is empty, delete the command
	if newNameStr == "" {
		delete(oldNs.commands, oldSimple)
		// Copy traces before unlocking - use fully qualified name
		traces := make([]TraceEntry, len(i.cmdTraces[oldQualified]))
		copy(traces, i.cmdTraces[oldQualified])
		// Fire delete traces
		if len(traces) > 0 {
			fireCmdTraces(i, oldQualified, "", "delete", traces)
		}
		return C.TCL_OK
	}

	// Get new namespace location
	newNsPath, newSimple := splitQualified(newNameStr)

	// Check if new name already exists
	newNs := i.ensureNamespace(newNsPath)
	if _, exists := newNs.commands[newSimple]; exists {
		i.SetErrorString("can't rename to \"" + i.DisplayName(newNameStr) + "\": command already exists")
		return C.TCL_ERROR
	}

	// Move command from old namespace to new namespace
	delete(oldNs.commands, oldSimple)
	newNs.commands[newSimple] = cmd

	// Copy traces before unlocking - use fully qualified name
	traces := make([]TraceEntry, len(i.cmdTraces[oldQualified]))
	copy(traces, i.cmdTraces[oldQualified])
	// Fire rename traces
	if len(traces) > 0 {
		fireCmdTraces(i, oldQualified, newNameStr, "rename", traces)
	}

	return C.TCL_OK
}

// Helper to create or get a namespace by path
func (i *Interp) ensureNamespace(path string) *Namespace {
	if ns, ok := i.namespaces[path]; ok {
		return ns
	}
	// Parse path and create hierarchy
	// Path like "::foo::bar" -> create :: -> ::foo -> ::foo::bar
	parts := strings.Split(strings.TrimPrefix(path, "::"), "::")
	current := i.globalNamespace
	currentPath := "::"

	for _, part := range parts {
		if part == "" {
			continue
		}
		childPath := currentPath
		if currentPath == "::" {
			childPath = "::" + part
		} else {
			childPath = currentPath + "::" + part
		}

		child, ok := current.children[part]
		if !ok {
			child = &Namespace{
				fullPath: childPath,
				parent:   current,
				children: make(map[string]*Namespace),
				vars:     make(map[string]TclObj),
				commands: make(map[string]*Command),
			}
			current.children[part] = child
			i.namespaces[childPath] = child
		}
		current = child
		currentPath = childPath
	}
	return current
}

//export goNsCreate
func goNsCreate(interp C.TclInterp, path C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(TclObj(path))
	i.ensureNamespace(pathStr)
	return C.TCL_OK
}

//export goNsDelete
func goNsDelete(interp C.TclInterp, path C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(TclObj(path))

	// Cannot delete global namespace
	if pathStr == "::" {
		return C.TCL_ERROR
	}

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return C.TCL_ERROR
	}

	// Delete all children recursively
	var deleteRecursive func(n *Namespace)
	deleteRecursive = func(n *Namespace) {
		for _, child := range n.children {
			deleteRecursive(child)
		}
		delete(i.namespaces, n.fullPath)
	}
	deleteRecursive(ns)

	// Remove from parent's children
	if ns.parent != nil {
		for name, child := range ns.parent.children {
			if child == ns {
				delete(ns.parent.children, name)
				break
			}
		}
	}

	return C.TCL_OK
}

//export goNsExists
func goNsExists(interp C.TclInterp, path C.TclObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(path))
	if _, ok := i.namespaces[pathStr]; ok {
		return 1
	}
	return 0
}

//export goNsCurrent
func goNsCurrent(interp C.TclInterp) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	frame := i.frames[i.active]
	if frame.ns != nil {
		return C.TclObj(i.internString(frame.ns.fullPath))
	}
	return C.TclObj(i.internString("::"))
}

//export goNsParent
func goNsParent(interp C.TclInterp, nsPath C.TclObj, result *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(TclObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return C.TCL_ERROR
	}

	if ns.parent == nil {
		// Global namespace has no parent - return empty string
		*result = C.TclObj(i.internString(""))
	} else {
		*result = C.TclObj(i.internString(ns.parent.fullPath))
	}
	return C.TCL_OK
}

//export goNsChildren
func goNsChildren(interp C.TclInterp, nsPath C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []TclObj{}}
		return C.TclObj(id)
	}

	// Collect and sort child names for consistent ordering
	names := make([]string, 0, len(ns.children))
	for name := range ns.children {
		names = append(names, name)
	}
	sort.Strings(names)

	// Build list of full paths
	items := make([]TclObj, len(names))
	for idx, name := range names {
		child := ns.children[name]
		items[idx] = i.internString(child.fullPath)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.TclObj(id)
}

//export goNsGetVar
func goNsGetVar(interp C.TclInterp, nsPath C.TclObj, name C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return 0
	}
	if val, ok := ns.vars[nameStr]; ok {
		return C.TclObj(val)
	}
	return 0
}

//export goNsSetVar
func goNsSetVar(interp C.TclInterp, nsPath C.TclObj, name C.TclObj, value C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	// Create namespace if needed
	ns := i.ensureNamespace(pathStr)
	ns.vars[nameStr] = TclObj(value)
}

//export goNsVarExists
func goNsVarExists(interp C.TclInterp, nsPath C.TclObj, name C.TclObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return 0
	}
	if _, ok := ns.vars[nameStr]; ok {
		return 1
	}
	return 0
}

//export goNsUnsetVar
func goNsUnsetVar(interp C.TclInterp, nsPath C.TclObj, name C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return
	}
	delete(ns.vars, nameStr)
}

//export goNsGetCommand
func goNsGetCommand(interp C.TclInterp, nsPath C.TclObj, name C.TclObj, fn *C.TclBuiltinCmd) C.TclCommandType {
	i := getInterp(interp)
	if i == nil {
		*fn = nil
		return C.TCL_CMD_NONE
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		*fn = nil
		return C.TCL_CMD_NONE
	}

	cmd, ok := ns.commands[nameStr]
	if !ok {
		*fn = nil
		return C.TCL_CMD_NONE
	}

	switch cmd.cmdType {
	case CmdBuiltin:
		*fn = cmd.builtin
		return C.TCL_CMD_BUILTIN
	case CmdProc:
		*fn = nil
		return C.TCL_CMD_PROC
	default:
		*fn = nil
		return C.TCL_CMD_NONE
	}
}

//export goNsSetCommand
func goNsSetCommand(interp C.TclInterp, nsPath C.TclObj, name C.TclObj,
	kind C.TclCommandType, fn C.TclBuiltinCmd,
	params C.TclObj, body C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	// Ensure namespace exists
	ns := i.ensureNamespace(pathStr)

	cmd := &Command{
		cmdType: CommandType(kind),
	}
	if kind == C.TCL_CMD_BUILTIN {
		cmd.builtin = fn
	} else if kind == C.TCL_CMD_PROC {
		cmd.proc = &Procedure{
			name:   TclObj(name),
			params: TclObj(params),
			body:   TclObj(body),
		}
	}
	ns.commands[nameStr] = cmd
}

//export goNsDeleteCommand
func goNsDeleteCommand(interp C.TclInterp, nsPath C.TclObj, name C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return C.TCL_ERROR
	}

	if _, ok := ns.commands[nameStr]; !ok {
		return C.TCL_ERROR
	}

	delete(ns.commands, nameStr)
	return C.TCL_OK
}

//export goNsListCommands
func goNsListCommands(interp C.TclInterp, nsPath C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []TclObj{}}
		return C.TclObj(id)
	}

	names := make([]string, 0, len(ns.commands))
	for name := range ns.commands {
		names = append(names, name)
	}
	sort.Strings(names)

	items := make([]TclObj, len(names))
	for idx, name := range names {
		items[idx] = i.internString(name)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.TclObj(id)
}

//export goFrameSetNamespace
func goFrameSetNamespace(interp C.TclInterp, nsPath C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(TclObj(nsPath))

	// Create namespace if needed
	ns := i.ensureNamespace(pathStr)
	i.frames[i.active].ns = ns
	return C.TCL_OK
}

//export goFrameGetNamespace
func goFrameGetNamespace(interp C.TclInterp) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	frame := i.frames[i.active]
	if frame.ns != nil {
		return C.TclObj(i.internString(frame.ns.fullPath))
	}
	return C.TclObj(i.internString("::"))
}

//export goVarLinkNs
func goVarLinkNs(interp C.TclInterp, local C.TclObj, nsPath C.TclObj, name C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	localStr := i.GetString(TclObj(local))
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	frame := i.frames[i.active]
	frame.links[localStr] = varLink{
		targetLevel: -1, // -1 indicates namespace link
		nsPath:      pathStr,
		nsName:      nameStr,
	}
}

//export goInterpGetScript
func goInterpGetScript(interp C.TclInterp) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	if i.scriptPath == 0 {
		// Return empty string if no script path set
		return C.TclObj(i.internString(""))
	}
	return C.TclObj(i.scriptPath)
}

//export goInterpSetScript
func goInterpSetScript(interp C.TclInterp, path C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.scriptPath = TclObj(path)
}

//export goVarNames
func goVarNames(interp C.TclInterp, ns C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}

	var names []string

	if ns == 0 {
		// Return variables in current frame (locals)
		frame := i.frames[i.active]
		for name := range frame.vars {
			names = append(names, name)
		}
		// Also include linked variables (upvar, variable)
		for name := range frame.links {
			// Only include if not already in vars (avoid duplicates)
			found := false
			for _, n := range names {
				if n == name {
					found = true
					break
				}
			}
			if !found {
				names = append(names, name)
			}
		}
	} else {
		// Return variables in the specified namespace
		pathStr := i.GetString(TclObj(ns))
		if pathStr == "::" {
			// Global namespace - return variables from the global frame (frame 0)
			globalFrame := i.frames[0]
			for name := range globalFrame.vars {
				names = append(names, name)
			}
		} else if nsObj, ok := i.namespaces[pathStr]; ok {
			for name := range nsObj.vars {
				names = append(names, name)
			}
		}
	}

	// Sort for consistent ordering
	sort.Strings(names)

	// Create list of names
	items := make([]TclObj, len(names))
	for idx, name := range names {
		items[idx] = i.internString(name)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.TclObj(id)
}

//export goTraceAdd
func goTraceAdd(interp C.TclInterp, kind C.TclObj, name C.TclObj, ops C.TclObj, script C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	kindStr := i.GetString(TclObj(kind))
	nameStr := i.GetString(TclObj(name))
	opsStr := i.GetString(TclObj(ops))


	entry := TraceEntry{
		ops:    opsStr,
		script: TclObj(script),
	}

	if kindStr == "variable" {
		i.varTraces[nameStr] = append(i.varTraces[nameStr], entry)
	} else if kindStr == "command" {
		i.cmdTraces[nameStr] = append(i.cmdTraces[nameStr], entry)
	} else {
		return C.TCL_ERROR
	}

	return C.TCL_OK
}

//export goTraceRemove
func goTraceRemove(interp C.TclInterp, kind C.TclObj, name C.TclObj, ops C.TclObj, script C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	kindStr := i.GetString(TclObj(kind))
	nameStr := i.GetString(TclObj(name))
	opsStr := i.GetString(TclObj(ops))
	scriptStr := i.GetString(TclObj(script))

	var traces *map[string][]TraceEntry
	if kindStr == "variable" {
		traces = &i.varTraces
	} else if kindStr == "command" {
		traces = &i.cmdTraces
	} else {
		return C.TCL_ERROR
	}

	// Find and remove matching trace - compare by string value, not handle
	entries := (*traces)[nameStr]
	for idx, entry := range entries {
		entryScriptStr := i.GetString(entry.script)
		if entry.ops == opsStr && entryScriptStr == scriptStr {
			// Remove this entry
			(*traces)[nameStr] = append(entries[:idx], entries[idx+1:]...)
			return C.TCL_OK
		}
	}

	// No matching trace found
	return C.TCL_ERROR
}

//export goTraceInfo
func goTraceInfo(interp C.TclInterp, kind C.TclObj, name C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	kindStr := i.GetString(TclObj(kind))
	nameStr := i.GetString(TclObj(name))

	var entries []TraceEntry
	if kindStr == "variable" {
		entries = i.varTraces[nameStr]
	} else if kindStr == "command" {
		entries = i.cmdTraces[nameStr]
	}

	// Build list of {ops... script} sublists
	// Format: each trace is {op1 op2 ... script} where ops are individual elements
	items := make([]TclObj, 0, len(entries))
	for _, entry := range entries {
		// Split ops into individual elements
		ops := strings.Fields(entry.ops)
		subItems := make([]TclObj, 0, len(ops)+1)
		for _, op := range ops {
			subItems = append(subItems, i.internString(op))
		}
		// Add the script at the end
		subItems = append(subItems, entry.script)

		subId := i.nextID
		i.nextID++
		i.objects[subId] = &Object{isList: true, listItems: subItems}
		items = append(items, subId)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.TclObj(id)
}

//export goNsGetExports
func goNsGetExports(interp C.TclInterp, nsPath C.TclObj) C.TclObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []TclObj{}}
		return C.TclObj(id)
	}

	// Return export patterns as a list
	items := make([]TclObj, len(ns.exportPatterns))
	for idx, pattern := range ns.exportPatterns {
		items[idx] = i.internString(pattern)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.TclObj(id)
}

//export goNsSetExports
func goNsSetExports(interp C.TclInterp, nsPath C.TclObj, patterns C.TclObj, clear C.int) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(TclObj(nsPath))

	ns := i.ensureNamespace(pathStr)

	// Get patterns from list
	patternList, err := i.GetList(TclObj(patterns))
	if err != nil {
		return
	}
	newPatterns := make([]string, len(patternList))
	for idx, p := range patternList {
		newPatterns[idx] = i.GetString(p)
	}

	if clear != 0 {
		// Replace existing patterns
		ns.exportPatterns = newPatterns
	} else {
		// Append to existing patterns
		ns.exportPatterns = append(ns.exportPatterns, newPatterns...)
	}
}

//export goNsIsExported
func goNsIsExported(interp C.TclInterp, nsPath C.TclObj, name C.TclObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(TclObj(nsPath))
	nameStr := i.GetString(TclObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return 0
	}

	// Check if name matches any export pattern
	for _, pattern := range ns.exportPatterns {
		if globMatch(pattern, nameStr) {
			return 1
		}
	}
	return 0
}

// globMatch performs simple glob pattern matching
// Supports * for any sequence and ? for single character
func globMatch(pattern, str string) bool {
	return globMatchHelper(pattern, str, 0, 0)
}

func globMatchHelper(pattern, str string, pi, si int) bool {
	for pi < len(pattern) {
		if pattern[pi] == '*' {
			// Skip consecutive *
			for pi < len(pattern) && pattern[pi] == '*' {
				pi++
			}
			if pi >= len(pattern) {
				return true // * at end matches everything
			}
			// Try matching * with 0, 1, 2, ... characters
			for si <= len(str) {
				if globMatchHelper(pattern, str, pi, si) {
					return true
				}
				si++
			}
			return false
		} else if si >= len(str) {
			return false // pattern has chars but string is empty
		} else if pattern[pi] == '?' || pattern[pi] == str[si] {
			pi++
			si++
		} else {
			return false
		}
	}
	return si >= len(str)
}

//export goNsCopyCommand
func goNsCopyCommand(interp C.TclInterp, srcNs C.TclObj, srcName C.TclObj,
	dstNs C.TclObj, dstName C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	srcNsStr := i.GetString(TclObj(srcNs))
	srcNameStr := i.GetString(TclObj(srcName))
	dstNsStr := i.GetString(TclObj(dstNs))
	dstNameStr := i.GetString(TclObj(dstName))

	// Find source namespace
	srcNsObj, ok := i.namespaces[srcNsStr]
	if !ok {
		return C.TCL_ERROR
	}

	// Find source command
	cmd, ok := srcNsObj.commands[srcNameStr]
	if !ok {
		return C.TCL_ERROR
	}

	// Ensure destination namespace exists
	dstNsObj := i.ensureNamespace(dstNsStr)

	// Copy command to destination (it's a pointer copy, so both share the same Command)
	dstNsObj.commands[dstNameStr] = cmd

	return C.TCL_OK
}
