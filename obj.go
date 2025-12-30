package feather

import "fmt"

// Obj is a Feather value.
// It follows TCL semantics where values have both a string representation
// and an optional internal representation that can be lazily computed.
type Obj struct {
	bytes  string  // string representation ("" = empty string if intrep == nil)
	intrep ObjType // internal representation (nil = pure string)
	interp *Interp // owning interpreter (for shimmering that requires parsing)
}

// ObjType defines the core behavior for an internal representation.
type ObjType interface {
	// Name returns the type name (e.g., "int", "list").
	Name() string

	// UpdateString regenerates string representation from this internal rep.
	UpdateString() string

	// Dup creates a copy of this internal representation.
	Dup() ObjType
}

// IntoInt can convert directly to int64.
type IntoInt interface {
	IntoInt() (int64, bool)
}

// IntoDouble can convert directly to float64.
type IntoDouble interface {
	IntoDouble() (float64, bool)
}

// IntoList can convert directly to a list.
type IntoList interface {
	IntoList() ([]*Obj, bool)
}

// IntoDict can convert directly to a dictionary.
type IntoDict interface {
	IntoDict() (map[string]*Obj, []string, bool)
}

// IntoBool can convert directly to a boolean.
type IntoBool interface {
	IntoBool() (bool, bool)
}

// String returns the string representation of the object.
// If the string representation is empty and there's an internal representation,
// it regenerates the string from the internal rep.
func (o *Obj) String() string {
	if o == nil {
		return ""
	}
	if o.bytes == "" && o.intrep != nil {
		o.bytes = o.intrep.UpdateString()
	}
	return o.bytes
}

// Type returns the type name of the object.
// Returns "string" for pure string objects (no internal representation).
func (o *Obj) Type() string {
	if o == nil || o.intrep == nil {
		return "string"
	}
	return o.intrep.Name()
}

// InternalRep returns the internal representation of the object.
// Returns nil for pure string objects.
//
// Use type assertion to access custom ObjType implementations:
//
//	if myType, ok := obj.InternalRep().(*MyType); ok {
//	    // use myType
//	}
func (o *Obj) InternalRep() ObjType {
	if o == nil {
		return nil
	}
	return o.intrep
}

// invalidate clears the cached string representation.
// Should be called after mutating the internal representation.
func (o *Obj) invalidate() {
	if o == nil {
		return
	}
	o.bytes = ""
}

// Copy creates a shallow copy of the object.
// If the object has an internal representation, it is duplicated via Dup().
// The copy remains tied to the same interpreter as the original.
func (o *Obj) Copy() *Obj {
	if o == nil {
		return nil
	}
	if o.intrep == nil {
		return &Obj{bytes: o.bytes, interp: o.interp}
	}
	return &Obj{bytes: o.bytes, intrep: o.intrep.Dup(), interp: o.interp}
}

// setBytes sets the string representation directly (used by Interp for handle-based naming).
func (o *Obj) setBytes(s string) {
	if o != nil {
		o.bytes = s
	}
}

// Int returns the integer value of this object, shimmering if needed.
func (o *Obj) Int() (int64, error) {
	return asInt(o)
}

// Double returns the float64 value of this object, shimmering if needed.
func (o *Obj) Double() (float64, error) {
	return asDouble(o)
}

// Bool returns the boolean value of this object using TCL boolean rules.
func (o *Obj) Bool() (bool, error) {
	return asBool(o)
}

// List returns the list elements of this object, shimmering if needed.
// If the object is a pure string, it will be parsed as a TCL list.
func (o *Obj) List() ([]*Obj, error) {
	// Try existing list rep first
	if list, err := asList(o); err == nil {
		return list, nil
	}
	// Shimmer via interpreter
	if o == nil || o.interp == nil {
		return nil, fmt.Errorf("cannot parse list without interpreter")
	}
	list, err := o.interp.parseList(o.String())
	if err != nil {
		return nil, err
	}
	o.intrep = ListType(list)
	return list, nil
}

// Dict returns the dict representation of this object, shimmering if needed.
// If the object is a pure string, it will be parsed as a TCL dict.
func (o *Obj) Dict() (*DictType, error) {
	// Try existing dict rep first
	if d, err := asDict(o); err == nil {
		return d, nil
	}
	// Shimmer via interpreter
	if o == nil || o.interp == nil {
		return nil, fmt.Errorf("cannot parse dict without interpreter")
	}
	d, err := o.interp.parseDict(o.String())
	if err != nil {
		return nil, err
	}
	o.intrep = d
	return d, nil
}
