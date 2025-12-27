package feather

import (
	"fmt"
	"strconv"
	"strings"
)

// NOTE: String-to-list and string-to-dict conversions require the interpreter's
// parsing facilities (which delegate to the C core). The AsList and AsDict
// functions only handle direct conversions via IntoList/IntoDict interfaces.

// AsInt converts o to int64, shimmering if needed.
func AsInt(o *Obj) (int64, error) {
	if o == nil {
		return 0, nil
	}
	// Try direct conversion via IntoInt interface
	if c, ok := o.intrep.(IntoInt); ok {
		if v, ok := c.IntoInt(); ok {
			// Already has int representation, no need to shimmer
			return v, nil
		}
	}
	// Fallback: parse string
	v, err := strconv.ParseInt(o.String(), 10, 64)
	if err != nil {
		return 0, fmt.Errorf("expected integer but got %q", o.String())
	}
	// Shimmer: update internal representation
	o.intrep = IntType(v)
	return v, nil
}

// AsDouble converts o to float64, shimmering if needed.
func AsDouble(o *Obj) (float64, error) {
	if o == nil {
		return 0, nil
	}
	// Try direct conversion via IntoDouble interface
	if c, ok := o.intrep.(IntoDouble); ok {
		if v, ok := c.IntoDouble(); ok {
			return v, nil
		}
	}
	// Fallback: parse string
	v, err := strconv.ParseFloat(o.String(), 64)
	if err != nil {
		return 0, fmt.Errorf("expected floating-point number but got %q", o.String())
	}
	// Shimmer: update internal representation
	o.intrep = DoubleType(v)
	return v, nil
}

// AsList converts o to a list if it has a list-compatible internal representation.
// For string-to-list conversion, use the interpreter's parsing facilities.
func AsList(o *Obj) ([]*Obj, error) {
	if o == nil {
		return nil, nil
	}
	// Try direct conversion via IntoList interface
	if c, ok := o.intrep.(IntoList); ok {
		if v, ok := c.IntoList(); ok {
			return v, nil
		}
	}
	// Pure string objects require parsing through the interpreter
	return nil, fmt.Errorf("cannot convert %q to list without interpreter", o.String())
}

// AsDict converts o to a dictionary if it has a dict-compatible internal representation.
// For string-to-dict conversion, use the interpreter's parsing facilities.
func AsDict(o *Obj) (*DictType, error) {
	if o == nil {
		return &DictType{Items: make(map[string]*Obj)}, nil
	}
	// Try direct conversion via IntoDict interface
	if c, ok := o.intrep.(IntoDict); ok {
		if items, order, ok := c.IntoDict(); ok {
			d := &DictType{Items: items, Order: order}
			o.intrep = d
			return d, nil
		}
	}
	// Pure string objects require parsing through the interpreter
	return nil, fmt.Errorf("cannot convert %q to dict without interpreter", o.String())
}

// AsBool converts o to a boolean, shimmering if needed.
func AsBool(o *Obj) (bool, error) {
	if o == nil {
		return false, nil
	}
	// Try direct conversion via IntoBool interface
	if c, ok := o.intrep.(IntoBool); ok {
		if v, ok := c.IntoBool(); ok {
			return v, nil
		}
	}
	// Fallback: try as int, then check truthiness
	if v, err := AsInt(o); err == nil {
		return v != 0, nil
	}
	// String truthiness
	s := strings.ToLower(o.String())
	switch s {
	case "true", "yes", "on", "1":
		return true, nil
	case "false", "no", "off", "0":
		return false, nil
	}
	return false, fmt.Errorf("expected boolean but got %q", o.String())
}

// List helper functions - these only work with objects that have list representations.

// ObjListLen returns the length of a list object.
// Returns 0 if the object is nil or not a list.
func ObjListLen(o *Obj) int {
	if o == nil {
		return 0
	}
	if c, ok := o.intrep.(IntoList); ok {
		if list, ok := c.IntoList(); ok {
			return len(list)
		}
	}
	return 0
}

// ObjListAt returns the element at index i in a list object.
// Returns nil if the object is not a list or index is out of bounds.
func ObjListAt(o *Obj, i int) *Obj {
	if o == nil {
		return nil
	}
	if c, ok := o.intrep.(IntoList); ok {
		if list, ok := c.IntoList(); ok {
			if i >= 0 && i < len(list) {
				return list[i]
			}
		}
	}
	return nil
}

// ObjListAppend appends an element to a list object.
// Converts the object to a list if it has a list-compatible representation.
// Invalidates the string representation.
func ObjListAppend(o *Obj, elem *Obj) {
	if o == nil || elem == nil {
		return
	}
	if c, ok := o.intrep.(IntoList); ok {
		if list, ok := c.IntoList(); ok {
			o.intrep = ListType(append(list, elem))
			o.invalidate()
		}
	}
}

// ObjListSet sets the element at index i in a list object.
// Does nothing if the object is not a list or index is out of bounds.
// Invalidates the string representation.
func ObjListSet(o *Obj, i int, elem *Obj) {
	if o == nil || elem == nil {
		return
	}
	if c, ok := o.intrep.(IntoList); ok {
		if list, ok := c.IntoList(); ok {
			if i >= 0 && i < len(list) {
				list[i] = elem
				o.invalidate()
			}
		}
	}
}

// Dict helper functions - these only work with objects that have dict representations.

// ObjDictGet returns the value for a key in a dict object.
// Returns nil, false if the object is not a dict or key doesn't exist.
func ObjDictGet(o *Obj, key string) (*Obj, bool) {
	if o == nil {
		return nil, false
	}
	if c, ok := o.intrep.(IntoDict); ok {
		if items, _, ok := c.IntoDict(); ok {
			v, exists := items[key]
			return v, exists
		}
	}
	return nil, false
}

// ObjDictSet sets a key-value pair in a dict object.
// Creates the dict if needed (for nil intrep).
// Invalidates the string representation.
func ObjDictSet(o *Obj, key string, val *Obj) {
	if o == nil {
		return
	}
	// Get or create dict representation
	var d *DictType
	if c, ok := o.intrep.(IntoDict); ok {
		if items, order, ok := c.IntoDict(); ok {
			d = &DictType{Items: items, Order: order}
		}
	}
	if d == nil {
		d = &DictType{Items: make(map[string]*Obj)}
	}
	// Add key if new
	if _, exists := d.Items[key]; !exists {
		d.Order = append(d.Order, key)
	}
	d.Items[key] = val
	o.intrep = d
	o.invalidate()
}

// ObjDictKeys returns the keys of a dict object in insertion order.
// Returns nil if the object is not a dict.
func ObjDictKeys(o *Obj) []string {
	if o == nil {
		return nil
	}
	if c, ok := o.intrep.(IntoDict); ok {
		if _, order, ok := c.IntoDict(); ok {
			return order
		}
	}
	return nil
}

// ObjDictLen returns the number of keys in a dict object.
// Returns 0 if the object is nil or not a dict.
func ObjDictLen(o *Obj) int {
	if o == nil {
		return 0
	}
	if c, ok := o.intrep.(IntoDict); ok {
		if items, _, ok := c.IntoDict(); ok {
			return len(items)
		}
	}
	return 0
}
