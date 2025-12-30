package feather

import "strings"

// DictType is the internal representation for dictionary values.
type DictType struct {
	Items map[string]*Obj
	Order []string
}

func (t *DictType) Name() string { return "dict" }

func (t *DictType) Dup() ObjType {
	newItems := make(map[string]*Obj, len(t.Items))
	for k, v := range t.Items {
		newItems[k] = v
	}
	newOrder := make([]string, len(t.Order))
	copy(newOrder, t.Order)
	return &DictType{Items: newItems, Order: newOrder}
}

func (t *DictType) UpdateString() string {
	var result strings.Builder
	for i, key := range t.Order {
		if i > 0 {
			result.WriteByte(' ')
		}
		// Quote key if needed
		if len(key) == 0 || strings.ContainsAny(key, " \t\n{}") {
			result.WriteByte('{')
			result.WriteString(key)
			result.WriteByte('}')
		} else {
			result.WriteString(key)
		}
		result.WriteByte(' ')
		// Quote value if needed
		val := t.Items[key]
		s := val.String()
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

func (t *DictType) IntoDict() (map[string]*Obj, []string, bool) {
	return t.Items, t.Order, true
}

func (t *DictType) IntoList() ([]*Obj, bool) {
	list := make([]*Obj, 0, len(t.Order)*2)
	// Get interpreter from first value (if any) to set on key objects
	var interp *Interp
	for _, v := range t.Items {
		if v != nil && v.interp != nil {
			interp = v.interp
			break
		}
	}
	for _, k := range t.Order {
		list = append(list, &Obj{bytes: k, interp: interp}, t.Items[k])
	}
	return list, true
}
