// registry.go provides object handle registry for the low-level C API.
// This allows C code to hold references to feather objects without
// worrying about Go GC.
package main

import (
	"sync"

	"github.com/feather-lang/feather"
)

// objRegistry holds object handles for a single interpreter.
// Object handles are uintptrs that C code can use to reference Go objects.
type objRegistry struct {
	mu       sync.Mutex
	objects  map[uintptr]*feather.Obj
	nextObj  uintptr
}

// Global registries per interpreter
var (
	registryMu sync.Mutex
	registries = make(map[uintptr]*objRegistry)
)

// getRegistry returns the object registry for an interpreter, creating if needed.
func getRegistry(interpID uintptr) *objRegistry {
	registryMu.Lock()
	defer registryMu.Unlock()

	reg, ok := registries[interpID]
	if !ok {
		reg = &objRegistry{
			objects: make(map[uintptr]*feather.Obj),
			nextObj: 1,
		}
		registries[interpID] = reg
	}
	return reg
}

// cleanupRegistry removes the registry for an interpreter when it's closed.
func cleanupRegistry(interpID uintptr) {
	registryMu.Lock()
	defer registryMu.Unlock()
	delete(registries, interpID)
}

// registerObj stores an object and returns a handle for C code.
func (r *objRegistry) registerObj(obj *feather.Obj) uintptr {
	if obj == nil {
		return 0
	}
	r.mu.Lock()
	defer r.mu.Unlock()

	handle := r.nextObj
	r.nextObj++
	r.objects[handle] = obj
	return handle
}

// getObj retrieves an object by its handle.
func (r *objRegistry) getObj(handle uintptr) *feather.Obj {
	if handle == 0 {
		return nil
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.objects[handle]
}

// Helper to get interp and registry together
func getInterpAndRegistry(id uintptr) (*feather.Interp, *objRegistry) {
	state := getInterp(id)
	if state == nil {
		return nil, nil
	}
	return state.interp, getRegistry(id)
}
