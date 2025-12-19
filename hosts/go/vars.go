package main

/*
#include <stddef.h>
*/
import "C"
import (
	"path/filepath"
	"strings"
	"sync"
)

// VarTable stores variables in a hash map
type VarTable struct {
	vars   map[string]*TclObj
	arrays map[string]map[string]*TclObj // arrayName -> key -> value
	links  map[string]*VarLink           // variable links (upvar/global)
}

// VarLink represents a linked variable (for upvar, global)
type VarLink struct {
	targetTable *VarTable
	targetName  string
}

// Variable table handle management
var (
	varsMu      sync.RWMutex
	varsHandles = make(map[uintptr]*VarTable)
	nextVarsID  uintptr = 1
)

func allocVarsHandle(table *VarTable) uintptr {
	varsMu.Lock()
	defer varsMu.Unlock()
	id := nextVarsID
	nextVarsID++
	varsHandles[id] = table
	return id
}

func getVars(h uintptr) *VarTable {
	varsMu.RLock()
	defer varsMu.RUnlock()
	return varsHandles[h]
}

func freeVarsHandle(h uintptr) {
	varsMu.Lock()
	defer varsMu.Unlock()
	delete(varsHandles, h)
}

// NewVarTable creates a new variable table
func NewVarTable() *VarTable {
	return &VarTable{
		vars:   make(map[string]*TclObj),
		arrays: make(map[string]map[string]*TclObj),
		links:  make(map[string]*VarLink),
	}
}

// Get retrieves a variable value
func (t *VarTable) Get(name string) *TclObj {
	// Check for link
	if link, ok := t.links[name]; ok {
		return link.targetTable.Get(link.targetName)
	}
	return t.vars[name]
}

// Set sets a variable value
func (t *VarTable) Set(name string, val *TclObj) {
	// Check for link
	if link, ok := t.links[name]; ok {
		link.targetTable.Set(link.targetName, val)
		return
	}
	t.vars[name] = val
}

// Unset removes a variable
func (t *VarTable) Unset(name string) {
	delete(t.links, name)
	delete(t.vars, name)
}

// Exists checks if a variable exists
func (t *VarTable) Exists(name string) bool {
	if _, ok := t.links[name]; ok {
		return true
	}
	_, ok := t.vars[name]
	return ok
}

// Names returns variable names matching a pattern
func (t *VarTable) Names(pattern string) []string {
	var names []string
	for name := range t.vars {
		if pattern == "" || pattern == "*" || matchPattern(pattern, name) {
			names = append(names, name)
		}
	}
	for name := range t.links {
		if pattern == "" || pattern == "*" || matchPattern(pattern, name) {
			names = append(names, name)
		}
	}
	return names
}

// Link creates a variable link (for upvar, global)
func (t *VarTable) Link(localName string, targetTable *VarTable, targetName string) {
	t.links[localName] = &VarLink{
		targetTable: targetTable,
		targetName:  targetName,
	}
}

// ArrayGet gets an array element
func (t *VarTable) ArrayGet(arrName, key string) *TclObj {
	arr, ok := t.arrays[arrName]
	if !ok {
		return nil
	}
	return arr[key]
}

// ArraySet sets an array element
func (t *VarTable) ArraySet(arrName, key string, val *TclObj) {
	arr, ok := t.arrays[arrName]
	if !ok {
		arr = make(map[string]*TclObj)
		t.arrays[arrName] = arr
	}
	arr[key] = val
}

// ArrayExists checks if an array element exists
func (t *VarTable) ArrayExists(arrName, key string) bool {
	arr, ok := t.arrays[arrName]
	if !ok {
		return false
	}
	_, exists := arr[key]
	return exists
}

// ArrayNames returns array keys matching a pattern
func (t *VarTable) ArrayNames(arrName, pattern string) []string {
	arr, ok := t.arrays[arrName]
	if !ok {
		return nil
	}

	var names []string
	for key := range arr {
		if pattern == "" || pattern == "*" || matchPattern(pattern, key) {
			names = append(names, key)
		}
	}
	return names
}

// ArrayUnset removes an array element
func (t *VarTable) ArrayUnset(arrName, key string) {
	arr, ok := t.arrays[arrName]
	if !ok {
		return
	}
	delete(arr, key)
	if len(arr) == 0 {
		delete(t.arrays, arrName)
	}
}

// ArraySize returns the number of elements in an array
func (t *VarTable) ArraySize(arrName string) int {
	arr, ok := t.arrays[arrName]
	if !ok {
		return 0
	}
	return len(arr)
}

// matchPattern implements glob-style pattern matching
func matchPattern(pattern, str string) bool {
	matched, _ := filepath.Match(pattern, str)
	return matched
}

// CGO exports for variable operations

//export goVarsNew
func goVarsNew(ctxHandle uintptr) uintptr {
	table := NewVarTable()
	return allocVarsHandle(table)
}

//export goVarsFree
func goVarsFree(ctxHandle uintptr, varsHandle uintptr) {
	freeVarsHandle(varsHandle)
}

//export goVarGet
func goVarGet(varsHandle uintptr, name *C.char, nameLen C.size_t) uintptr {
	table := getVars(varsHandle)
	if table == nil {
		return 0
	}

	goName := C.GoStringN(name, C.int(nameLen))
	obj := table.Get(goName)
	if obj == nil {
		return 0
	}

	return allocObjHandle(obj)
}

//export goVarSet
func goVarSet(varsHandle uintptr, name *C.char, nameLen C.size_t, valHandle uintptr) {
	table := getVars(varsHandle)
	if table == nil {
		return
	}

	goName := C.GoStringN(name, C.int(nameLen))
	obj := getObj(valHandle)
	if obj == nil {
		return
	}

	table.Set(goName, obj)
}

//export goVarUnset
func goVarUnset(varsHandle uintptr, name *C.char, nameLen C.size_t) {
	table := getVars(varsHandle)
	if table == nil {
		return
	}

	goName := C.GoStringN(name, C.int(nameLen))
	table.Unset(goName)
}

//export goVarExists
func goVarExists(varsHandle uintptr, name *C.char, nameLen C.size_t) C.int {
	table := getVars(varsHandle)
	if table == nil {
		return 0
	}

	goName := C.GoStringN(name, C.int(nameLen))
	if table.Exists(goName) {
		return 1
	}
	return 0
}

//export goVarNames
func goVarNames(varsHandle uintptr, pattern *C.char) uintptr {
	table := getVars(varsHandle)
	if table == nil {
		return allocObjHandle(NewString(""))
	}

	var pat string
	if pattern != nil {
		pat = C.GoString(pattern)
	}

	names := table.Names(pat)
	result := NewString(strings.Join(names, " "))
	return allocObjHandle(result)
}

//export goVarLink
func goVarLink(localVarsHandle uintptr, localName *C.char, localNameLen C.size_t,
	targetVarsHandle uintptr, targetName *C.char, targetNameLen C.size_t) {
	localTable := getVars(localVarsHandle)
	targetTable := getVars(targetVarsHandle)
	if localTable == nil || targetTable == nil {
		return
	}

	goLocalName := C.GoStringN(localName, C.int(localNameLen))
	goTargetName := C.GoStringN(targetName, C.int(targetNameLen))

	localTable.Link(goLocalName, targetTable, goTargetName)
}

//export goArraySet
func goArraySet(varsHandle uintptr, arr *C.char, arrLen C.size_t,
	key *C.char, keyLen C.size_t, valHandle uintptr) {
	table := getVars(varsHandle)
	if table == nil {
		return
	}

	goArr := C.GoStringN(arr, C.int(arrLen))
	goKey := C.GoStringN(key, C.int(keyLen))
	obj := getObj(valHandle)
	if obj == nil {
		return
	}

	table.ArraySet(goArr, goKey, obj)
}

//export goArrayGet
func goArrayGet(varsHandle uintptr, arr *C.char, arrLen C.size_t,
	key *C.char, keyLen C.size_t) uintptr {
	table := getVars(varsHandle)
	if table == nil {
		return 0
	}

	goArr := C.GoStringN(arr, C.int(arrLen))
	goKey := C.GoStringN(key, C.int(keyLen))
	obj := table.ArrayGet(goArr, goKey)
	if obj == nil {
		return 0
	}

	return allocObjHandle(obj)
}

//export goArrayExists
func goArrayExists(varsHandle uintptr, arr *C.char, arrLen C.size_t,
	key *C.char, keyLen C.size_t) C.int {
	table := getVars(varsHandle)
	if table == nil {
		return 0
	}

	goArr := C.GoStringN(arr, C.int(arrLen))
	goKey := C.GoStringN(key, C.int(keyLen))
	if table.ArrayExists(goArr, goKey) {
		return 1
	}
	return 0
}

//export goArrayNames
func goArrayNames(varsHandle uintptr, arr *C.char, arrLen C.size_t, pattern *C.char) uintptr {
	table := getVars(varsHandle)
	if table == nil {
		return allocObjHandle(NewString(""))
	}

	goArr := C.GoStringN(arr, C.int(arrLen))
	var pat string
	if pattern != nil {
		pat = C.GoString(pattern)
	}

	names := table.ArrayNames(goArr, pat)
	result := NewString(strings.Join(names, " "))
	return allocObjHandle(result)
}

//export goArrayUnset
func goArrayUnset(varsHandle uintptr, arr *C.char, arrLen C.size_t,
	key *C.char, keyLen C.size_t) {
	table := getVars(varsHandle)
	if table == nil {
		return
	}

	goArr := C.GoStringN(arr, C.int(arrLen))
	goKey := C.GoStringN(key, C.int(keyLen))
	table.ArrayUnset(goArr, goKey)
}

//export goArraySize
func goArraySize(varsHandle uintptr, arr *C.char, arrLen C.size_t) C.size_t {
	table := getVars(varsHandle)
	if table == nil {
		return 0
	}

	goArr := C.GoStringN(arr, C.int(arrLen))
	return C.size_t(table.ArraySize(goArr))
}
