// api_dict.go provides dict operations for the low-level C API.
package main

/*
#include <stdint.h>
*/
import "C"

import "github.com/feather-lang/feather"

// -----------------------------------------------------------------------------
// Dict Operations
// -----------------------------------------------------------------------------

//export feather_dict_create
func feather_dict_create(interp C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	obj := i.Dict()
	return C.size_t(reg.registerObj(obj))
}

//export feather_dict_is_dict
func feather_dict_is_dict(interp C.size_t, obj C.size_t) C.int {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	o := reg.getObj(uintptr(obj))
	if o == nil {
		return 0
	}
	_, err := o.Dict()
	if err == nil {
		return 1
	}
	return 0
}

//export feather_dict_from
func feather_dict_from(interp C.size_t, obj C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	o := reg.getObj(uintptr(obj))
	if o == nil {
		return 0
	}
	// Try to get as dict (with shimmering)
	d, err := o.Dict()
	if err != nil {
		// Can't convert to dict
		return 0
	}
	// Create a new dict with copied items
	newDict := i.Dict()
	newD, _ := newDict.Dict()
	for k, v := range d.Items {
		newD.Items[k] = v
	}
	newD.Order = make([]string, len(d.Order))
	copy(newD.Order, d.Order)
	return C.size_t(reg.registerObj(newDict))
}

//export feather_dict_get
func feather_dict_get(interp C.size_t, dict C.size_t, key C.size_t) C.size_t {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	dictObj := reg.getObj(uintptr(dict))
	keyObj := reg.getObj(uintptr(key))
	if dictObj == nil || keyObj == nil {
		return 0
	}
	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}
	keyStr := keyObj.String()
	if val, ok := d.Items[keyStr]; ok {
		return C.size_t(reg.registerObj(val))
	}
	return 0 // Key not found
}

//export feather_dict_set
func feather_dict_set(interp C.size_t, dict C.size_t, key C.size_t, value C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	dictObj := reg.getObj(uintptr(dict))
	keyObj := reg.getObj(uintptr(key))
	valueObj := reg.getObj(uintptr(value))
	if dictObj == nil || keyObj == nil || valueObj == nil {
		return 0
	}
	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}
	keyStr := keyObj.String()

	// Add key to order if new
	if _, exists := d.Items[keyStr]; !exists {
		d.Order = append(d.Order, keyStr)
	}
	d.Items[keyStr] = valueObj

	// Create updated dict object
	newDict := i.Dict()
	newD, _ := newDict.Dict()
	for k, v := range d.Items {
		newD.Items[k] = v
	}
	newD.Order = make([]string, len(d.Order))
	copy(newD.Order, d.Order)

	// Update dict in place
	reg.mu.Lock()
	reg.objects[uintptr(dict)] = newDict
	reg.mu.Unlock()

	return dict
}

//export feather_dict_exists
func feather_dict_exists(interp C.size_t, dict C.size_t, key C.size_t) C.int {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	dictObj := reg.getObj(uintptr(dict))
	keyObj := reg.getObj(uintptr(key))
	if dictObj == nil || keyObj == nil {
		return 0
	}
	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}
	keyStr := keyObj.String()
	if _, ok := d.Items[keyStr]; ok {
		return 1
	}
	return 0
}

//export feather_dict_remove
func feather_dict_remove(interp C.size_t, dict C.size_t, key C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	dictObj := reg.getObj(uintptr(dict))
	keyObj := reg.getObj(uintptr(key))
	if dictObj == nil || keyObj == nil {
		return 0
	}
	d, err := dictObj.Dict()
	if err != nil {
		return 0
	}
	keyStr := keyObj.String()

	// Create new dict without the key
	newDict := i.Dict()
	newD, _ := newDict.Dict()
	for _, k := range d.Order {
		if k != keyStr {
			newD.Items[k] = d.Items[k]
			newD.Order = append(newD.Order, k)
		}
	}

	// Update dict in place
	reg.mu.Lock()
	reg.objects[uintptr(dict)] = newDict
	reg.mu.Unlock()

	return dict
}

//export feather_dict_size
func feather_dict_size(interp C.size_t, dict C.size_t) C.size_t {
	_, reg := getInterpAndRegistry(uintptr(interp))
	if reg == nil {
		return 0
	}
	o := reg.getObj(uintptr(dict))
	if o == nil {
		return 0
	}
	d, err := o.Dict()
	if err != nil {
		return 0
	}
	return C.size_t(len(d.Items))
}

//export feather_dict_keys
func feather_dict_keys(interp C.size_t, dict C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	o := reg.getObj(uintptr(dict))
	if o == nil {
		return 0
	}
	d, err := o.Dict()
	if err != nil {
		return 0
	}
	// Create list of keys
	keys := make([]*feather.Obj, len(d.Order))
	for idx, k := range d.Order {
		keys[idx] = i.String(k)
	}
	return C.size_t(reg.registerObj(i.List(keys...)))
}

//export feather_dict_values
func feather_dict_values(interp C.size_t, dict C.size_t) C.size_t {
	i, reg := getInterpAndRegistry(uintptr(interp))
	if i == nil {
		return 0
	}
	o := reg.getObj(uintptr(dict))
	if o == nil {
		return 0
	}
	d, err := o.Dict()
	if err != nil {
		return 0
	}
	// Create list of values in key order
	values := make([]*feather.Obj, len(d.Order))
	for idx, k := range d.Order {
		values[idx] = d.Items[k]
	}
	return C.size_t(reg.registerObj(i.List(values...)))
}
