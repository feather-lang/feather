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
