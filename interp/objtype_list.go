package interp

import (
	"slices"
	"strings"
)

// ListType is the internal representation for list values.
type ListType []*Obj

func (t ListType) Name() string { return "list" }
func (t ListType) Dup() ObjType { return ListType(slices.Clone(t)) }
func (t ListType) UpdateString() string {
	var result strings.Builder
	for i, item := range t {
		if i > 0 {
			result.WriteByte(' ')
		}
		s := item.String()
		// Quote strings that contain spaces, special chars, or are empty
		if len(s) == 0 || strings.ContainsAny(s, " \t\n{}") {
			result.WriteByte('{')
			result.WriteString(s)
			result.WriteByte('}')
		} else {
			result.WriteString(s)
		}
	}
	return result.String()
}

func (t ListType) IntoList() ([]*Obj, bool) { return t, true }

func (t ListType) IntoDict() (map[string]*Obj, []string, bool) {
	if len(t)%2 != 0 {
		return nil, nil, false
	}
	items := make(map[string]*Obj)
	var order []string
	for i := 0; i < len(t); i += 2 {
		key := t[i].String()
		if _, exists := items[key]; !exists {
			order = append(order, key)
		}
		items[key] = t[i+1]
	}
	return items, order, true
}
