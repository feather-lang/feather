// api_list.go provides list operations for the low-level C API.
package main

/*
#include <stdint.h>
*/
import "C"

import "github.com/feather-lang/feather"

// -----------------------------------------------------------------------------
// List Operations
// -----------------------------------------------------------------------------

//export feather_list_is_nil
func feather_list_is_nil(interp C.size_t, obj C.size_t) C.int {
	if obj == 0 {
		return 1
	}
	return 0
}

//export feather_list_create
func feather_list_create(interp C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	obj := i.List()
	return C.size_t(reg.registerObj(obj))
}

//export feather_list_from
func feather_list_from(interp C.size_t, obj C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	o := reg.getObj(uintptr(obj))
	if o == nil {
		return 0
	}
	// Try to get as list (with shimmering from string)
	items, err := o.List()
	if err != nil {
		// Can't convert to list
		return 0
	}
	// Create a new list with copied items
	return C.size_t(reg.registerObj(i.List(items...)))
}

//export feather_list_push
func feather_list_push(interp C.size_t, list C.size_t, item C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return list
	}
	listObj := reg.getObj(uintptr(list))
	itemObj := reg.getObj(uintptr(item))
	if listObj == nil || itemObj == nil {
		return list
	}
	items, err := listObj.List()
	if err != nil {
		return list
	}
	newItems := append(items, itemObj)
	newList := i.List(newItems...)
	// Re-register with same handle (update in place)
	reg.mu.Lock()
	reg.objects[uintptr(list)] = newList
	reg.mu.Unlock()
	return list
}

//export feather_list_pop
func feather_list_pop(interp C.size_t, list C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	listObj := reg.getObj(uintptr(list))
	if listObj == nil {
		return 0
	}
	items, err := listObj.List()
	if err != nil || len(items) == 0 {
		return 0
	}
	last := items[len(items)-1]
	newList := i.List(items[:len(items)-1]...)
	// Update list in place
	reg.mu.Lock()
	reg.objects[uintptr(list)] = newList
	reg.mu.Unlock()
	return C.size_t(reg.registerObj(last))
}

//export feather_list_length
func feather_list_length(interp C.size_t, list C.size_t) C.size_t {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	o := reg.getObj(uintptr(list))
	if o == nil {
		return 0
	}
	items, err := o.List()
	if err != nil {
		return 0
	}
	return C.size_t(len(items))
}

//export feather_list_at
func feather_list_at(interp C.size_t, list C.size_t, index C.size_t) C.size_t {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	o := reg.getObj(uintptr(list))
	if o == nil {
		return 0
	}
	items, err := o.List()
	if err != nil {
		return 0
	}
	idx := int(index)
	if idx < 0 || idx >= len(items) {
		return 0
	}
	return C.size_t(reg.registerObj(items[idx]))
}

//export feather_list_slice
func feather_list_slice(interp C.size_t, list C.size_t, first C.size_t, last C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	o := reg.getObj(uintptr(list))
	if o == nil {
		return 0
	}
	items, err := o.List()
	if err != nil {
		return 0
	}

	length := len(items)
	f := int(first)
	l := int(last)

	// Clamp indices
	if f < 0 {
		f = 0
	}
	if l >= length {
		l = length - 1
	}

	// Empty result if invalid range
	if f > l || length == 0 {
		return C.size_t(reg.registerObj(i.List()))
	}

	sliced := items[f : l+1]
	return C.size_t(reg.registerObj(i.List(sliced...)))
}

//export feather_list_set_at
func feather_list_set_at(interp C.size_t, list C.size_t, index C.size_t, value C.size_t) C.int {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 1 // FEATHER_ERROR
	}
	listObj := reg.getObj(uintptr(list))
	valueObj := reg.getObj(uintptr(value))
	if listObj == nil || valueObj == nil {
		return 1
	}
	items, err := listObj.List()
	if err != nil {
		return 1
	}
	idx := int(index)
	if idx < 0 || idx >= len(items) {
		return 1
	}

	// Create new list with updated item
	newItems := make([]*feather.Obj, len(items))
	copy(newItems, items)
	newItems[idx] = valueObj
	newList := i.List(newItems...)

	// Update list in place
	reg.mu.Lock()
	reg.objects[uintptr(list)] = newList
	reg.mu.Unlock()

	return 0 // FEATHER_OK
}
