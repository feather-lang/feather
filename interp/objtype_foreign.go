package interp

import "fmt"

// ForeignType is the internal representation for foreign (host-language) objects.
type ForeignType struct {
	TypeName string
	Value    any
}

func (t *ForeignType) Name() string         { return t.TypeName }
func (t *ForeignType) Dup() ObjType         { return t }
func (t *ForeignType) UpdateString() string { return fmt.Sprintf("<%s:%p>", t.TypeName, t.Value) }
