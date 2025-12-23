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

import "sort"

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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	frame := &CallFrame{
		cmd:   TclObj(cmd),
		args:  TclObj(args),
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: newLevel,
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
	return C.size_t(i.active)
}

//export goFrameSetActive
func goFrameSetActive(interp C.TclInterp, level C.size_t) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
	return C.size_t(len(i.frames))
}

//export goFrameInfo
func goFrameInfo(interp C.TclInterp, level C.size_t, cmd *C.TclObj, args *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.mu.Lock()
	defer i.mu.Unlock()
	lvl := int(level)
	if lvl < 0 || lvl >= len(i.frames) {
		return C.TCL_ERROR
	}
	frame := i.frames[lvl]
	*cmd = C.TclObj(frame.cmd)
	*args = C.TclObj(frame.args)
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
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return 0
			}
		} else {
			break
		}
	}
	if val, ok := frame.vars[varName]; ok {
		return C.TclObj(val)
	}
	return 0
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
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
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
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
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
	i.mu.Lock()
	defer i.mu.Unlock()
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
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
	i.mu.Lock()
	defer i.mu.Unlock()
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
	i.mu.Lock()
	defer i.mu.Unlock()
	i.procs[nameStr] = &Procedure{
		name:   TclObj(name),
		params: TclObj(params),
		body:   TclObj(body),
	}
	// Also register in commands map for enumeration
	i.commands[nameStr] = struct{}{}
}

//export goProcExists
func goProcExists(interp C.TclInterp, name C.TclObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	nameStr := i.GetString(TclObj(name))
	i.mu.Lock()
	defer i.mu.Unlock()
	if _, ok := i.procs[nameStr]; ok {
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
	i.mu.Lock()
	proc, ok := i.procs[nameStr]
	i.mu.Unlock()
	if !ok {
		return C.TCL_ERROR
	}
	*result = C.TclObj(proc.params)
	return C.TCL_OK
}

//export goProcBody
func goProcBody(interp C.TclInterp, name C.TclObj, result *C.TclObj) C.TclResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameStr := i.GetString(TclObj(name))
	i.mu.Lock()
	proc, ok := i.procs[nameStr]
	i.mu.Unlock()
	if !ok {
		return C.TCL_ERROR
	}
	*result = C.TclObj(proc.body)
	return C.TCL_OK
}

// callCEval invokes the C interpreter
func callCEval(interpHandle TclInterp, scriptHandle TclObj) C.TclResult {
	return C.call_tcl_eval_obj(C.TclInterp(interpHandle), C.TclObj(scriptHandle), C.TCL_EVAL_LOCAL)
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
	// For now, ignore namespace parameter (only global namespace exists)
	// Collect all command names
	i.mu.Lock()
	names := make([]string, 0, len(i.commands))
	for name := range i.commands {
		names = append(names, name)
	}
	i.mu.Unlock()

	// Sort for consistent ordering
	sort.Strings(names)

	// Create a list object with all names
	i.mu.Lock()
	defer i.mu.Unlock()
	id := i.nextID
	i.nextID++
	items := make([]TclObj, len(names))
	for idx, name := range names {
		itemID := i.nextID
		i.nextID++
		i.objects[itemID] = &Object{stringVal: name}
		items[idx] = itemID
	}
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

//export goProcRegisterCommand
func goProcRegisterCommand(interp C.TclInterp, name C.TclObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameStr := i.GetString(TclObj(name))
	i.mu.Lock()
	defer i.mu.Unlock()
	i.commands[nameStr] = struct{}{}
}
