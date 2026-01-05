// api_string.go provides string operations for the low-level C API.
package main

/*
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
*/
import "C"

import (
	"strings"
	"unsafe"
)

// -----------------------------------------------------------------------------
// String Operations
// -----------------------------------------------------------------------------

//export feather_string_create
func feather_string_create(interp C.size_t, s *C.char, length C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	goStr := C.GoStringN(s, C.int(length))
	obj := i.String(goStr)
	return C.size_t(reg.registerObj(obj))
}

//export feather_string_get
func feather_string_get(interp C.size_t, str C.size_t) *C.char {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return nil
	}
	o := reg.getObj(uintptr(str))
	if o == nil {
		return nil
	}
	return C.CString(o.String())
}

//export feather_string_data
func feather_string_data(interp C.size_t, str C.size_t, length *C.size_t) *C.char {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return nil
	}
	o := reg.getObj(uintptr(str))
	if o == nil {
		return nil
	}
	s := o.String()
	if length != nil {
		*length = C.size_t(len(s))
	}
	// Return pointer to Go string data
	// WARNING: This is only valid until the next operation on the object
	if len(s) == 0 {
		return (*C.char)(unsafe.Pointer(&[]byte{0}[0]))
	}
	return (*C.char)(unsafe.Pointer(&([]byte(s))[0]))
}

//export feather_string_free
func feather_string_free(s *C.char) {
	if s != nil {
		C.free(unsafe.Pointer(s))
	}
}

//export feather_string_byte_at
func feather_string_byte_at(interp C.size_t, str C.size_t, index C.size_t) C.int {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return -1
	}
	o := reg.getObj(uintptr(str))
	if o == nil {
		return -1
	}
	s := o.String()
	idx := int(index)
	if idx < 0 || idx >= len(s) {
		return -1
	}
	return C.int(s[idx])
}

//export feather_string_byte_length
func feather_string_byte_length(interp C.size_t, str C.size_t) C.size_t {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	o := reg.getObj(uintptr(str))
	if o == nil {
		return 0
	}
	return C.size_t(len(o.String()))
}

//export feather_string_slice
func feather_string_slice(interp C.size_t, str C.size_t, start C.size_t, end C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	o := reg.getObj(uintptr(str))
	if o == nil {
		return 0
	}
	s := o.String()
	startIdx := int(start)
	endIdx := int(end)
	if startIdx >= len(s) {
		startIdx = len(s)
	}
	if endIdx > len(s) {
		endIdx = len(s)
	}
	if startIdx >= endIdx {
		return C.size_t(reg.registerObj(i.String("")))
	}
	return C.size_t(reg.registerObj(i.String(s[startIdx:endIdx])))
}

//export feather_string_concat
func feather_string_concat(interp C.size_t, a C.size_t, b C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	objA := reg.getObj(uintptr(a))
	objB := reg.getObj(uintptr(b))
	if objA == nil || objB == nil {
		return 0
	}
	result := objA.String() + objB.String()
	return C.size_t(reg.registerObj(i.String(result)))
}

//export feather_string_compare
func feather_string_compare(interp C.size_t, a C.size_t, b C.size_t) C.int {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	objA := reg.getObj(uintptr(a))
	objB := reg.getObj(uintptr(b))
	if objA == nil || objB == nil {
		return 0
	}
	strA := objA.String()
	strB := objB.String()
	if strA < strB {
		return -1
	}
	if strA > strB {
		return 1
	}
	return 0
}

//export feather_string_equal
func feather_string_equal(interp C.size_t, a C.size_t, b C.size_t) C.int {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	objA := reg.getObj(uintptr(a))
	objB := reg.getObj(uintptr(b))
	if objA == nil || objB == nil {
		return 0
	}
	if objA.String() == objB.String() {
		return 1
	}
	return 0
}

//export feather_string_match
func feather_string_match(interp C.size_t, pattern C.size_t, str C.size_t, nocase C.int) C.int {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	patternObj := reg.getObj(uintptr(pattern))
	strObj := reg.getObj(uintptr(str))
	if patternObj == nil || strObj == nil {
		return 0
	}
	patternStr := patternObj.String()
	strStr := strObj.String()
	if nocase != 0 {
		patternStr = strings.ToLower(patternStr)
		strStr = strings.ToLower(strStr)
	}
	if globMatch(patternStr, strStr) {
		return 1
	}
	return 0
}

// globMatch performs simple glob pattern matching.
// Supports * for any sequence and ? for single character.
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

// -----------------------------------------------------------------------------
// String Builder Operations
// -----------------------------------------------------------------------------

// String builders are stored separately since they're not feather.Obj
var (
	builderMu   = make(map[uintptr]*strings.Builder)
	builderNext uintptr = 1
)

//export feather_string_builder_new
func feather_string_builder_new(interp C.size_t, capacity C.size_t) C.size_t {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	builder := &strings.Builder{}
	if capacity > 0 {
		builder.Grow(int(capacity))
	}

	// Store builder with unique ID
	reg.mu.Lock()
	id := builderNext
	builderNext++
	builderMu[id] = builder
	reg.mu.Unlock()

	return C.size_t(id)
}

//export feather_string_builder_append_byte
func feather_string_builder_append_byte(interp C.size_t, builder C.size_t, b C.int) {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return
	}
	reg.mu.Lock()
	bldr := builderMu[uintptr(builder)]
	reg.mu.Unlock()
	if bldr != nil {
		bldr.WriteByte(byte(b))
	}
}

//export feather_string_builder_append_obj
func feather_string_builder_append_obj(interp C.size_t, builder C.size_t, str C.size_t) {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return
	}
	reg.mu.Lock()
	bldr := builderMu[uintptr(builder)]
	reg.mu.Unlock()
	if bldr == nil {
		return
	}
	o := reg.getObj(uintptr(str))
	if o != nil {
		bldr.WriteString(o.String())
	}
}

//export feather_string_builder_finish
func feather_string_builder_finish(interp C.size_t, builder C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	reg.mu.Lock()
	bldr := builderMu[uintptr(builder)]
	delete(builderMu, uintptr(builder))
	reg.mu.Unlock()
	if bldr == nil {
		return C.size_t(reg.registerObj(i.String("")))
	}
	result := bldr.String()
	return C.size_t(reg.registerObj(i.String(result)))
}
