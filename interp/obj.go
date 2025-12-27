package interp

// Obj is a Feather value.
// It follows TCL semantics where values have both a string representation
// and an optional internal representation that can be lazily computed.
type Obj struct {
	bytes  string  // string representation ("" = empty string if intrep == nil)
	intrep ObjType // internal representation (nil = pure string)
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

// Invalidate clears the cached string representation.
// Should be called after mutating the internal representation.
func (o *Obj) Invalidate() {
	if o == nil {
		return
	}
	o.bytes = ""
}

// Copy creates a shallow copy of the object.
// If the object has an internal representation, it is duplicated via Dup().
func (o *Obj) Copy() *Obj {
	if o == nil {
		return nil
	}
	if o.intrep == nil {
		return &Obj{bytes: o.bytes}
	}
	return &Obj{bytes: o.bytes, intrep: o.intrep.Dup()}
}

// NewString creates a new object with a string value.
func NewString(s string) *Obj {
	return &Obj{bytes: s}
}

// NewInt creates a new object with an integer value.
func NewInt(v int64) *Obj {
	return &Obj{intrep: IntType(v)}
}

// NewDouble creates a new object with a floating-point value.
func NewDouble(v float64) *Obj {
	return &Obj{intrep: DoubleType(v)}
}

// NewList creates a new object with a list value.
func NewList(items ...*Obj) *Obj {
	return &Obj{intrep: ListType(items)}
}

// NewDict creates a new empty dict object.
func NewDict() *Obj {
	return &Obj{intrep: &DictType{Items: make(map[string]*Obj)}}
}

// NewForeign creates a new foreign object with the given type name and Go value.
func NewForeign(typeName string, value any) *Obj {
	return &Obj{intrep: &ForeignType{TypeName: typeName, Value: value}}
}

// SetBytes sets the string representation directly (used by Interp for handle-based naming).
func (o *Obj) SetBytes(s string) {
	if o != nil {
		o.bytes = s
	}
}
