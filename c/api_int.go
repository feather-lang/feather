// api_int.go provides integer operations for the low-level C API.
package main

/*
#include <stdint.h>
*/
import "C"

// -----------------------------------------------------------------------------
// Integer Operations
// -----------------------------------------------------------------------------

//export feather_int_create
func feather_int_create(interp C.size_t, val C.int64_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	obj := i.Int(int64(val))
	return C.size_t(reg.registerObj(obj))
}

//export feather_int_get
func feather_int_get(interp C.size_t, obj C.size_t, out *C.int64_t) C.int {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 1 // FEATHER_ERROR
	}
	o := reg.getObj(uintptr(obj))
	if o == nil {
		return 1
	}
	val, err := o.Int()
	if err != nil {
		return 1
	}
	*out = C.int64_t(val)
	return 0 // FEATHER_OK
}
