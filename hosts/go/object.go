package main

/*
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"runtime/cgo"
	"strconv"
	"strings"
	"unicode/utf8"
	"unsafe"
)

// TclObj represents a TCL value with lazy type conversion (shimmering).
type TclObj struct {
	handle     cgo.Handle // Stable identity, assigned at creation
	stringRep  string     // Always valid
	intRep     *int64     // Cached integer representation
	doubleRep  *float64   // Cached double representation
	listRep    []*TclObj  // Cached list representation
	cachedCStr *C.char    // Cached C string for getStringPtr
}

// Handle returns the stable handle for this object
func (o *TclObj) Handle() uintptr {
	if o == nil {
		return 0
	}
	return uintptr(o.handle)
}

// getObj retrieves a TclObj by handle using cgo.Handle
func getObj(h uintptr) *TclObj {
	if h == 0 {
		return nil
	}
	return cgo.Handle(h).Value().(*TclObj)
}

// freeObj frees the object's handle and cached C string
func freeObj(obj *TclObj) {
	if obj == nil {
		return
	}
	if obj.cachedCStr != nil {
		C.free(unsafe.Pointer(obj.cachedCStr))
		obj.cachedCStr = nil
	}
	if obj.handle != 0 {
		obj.handle.Delete()
		obj.handle = 0
	}
}

// NewString creates a new string object
func NewString(s string) *TclObj {
	obj := &TclObj{stringRep: s}
	obj.handle = cgo.NewHandle(obj)
	return obj
}

// NewInt creates a new integer object
func NewInt(val int64) *TclObj {
	obj := &TclObj{
		stringRep: strconv.FormatInt(val, 10),
		intRep:    &val,
	}
	obj.handle = cgo.NewHandle(obj)
	return obj
}

// NewDouble creates a new double object
func NewDouble(val float64) *TclObj {
	s := strconv.FormatFloat(val, 'g', -1, 64)
	obj := &TclObj{
		stringRep: s,
		doubleRep: &val,
	}
	obj.handle = cgo.NewHandle(obj)
	return obj
}

// NewBool creates a new boolean object
func NewBool(val bool) *TclObj {
	var obj *TclObj
	if val {
		one := int64(1)
		obj = &TclObj{stringRep: "1", intRep: &one}
	} else {
		zero := int64(0)
		obj = &TclObj{stringRep: "0", intRep: &zero}
	}
	obj.handle = cgo.NewHandle(obj)
	return obj
}

// NewList creates a new list object
func NewList(elems []*TclObj) *TclObj {
	var parts []string
	for _, e := range elems {
		parts = append(parts, listQuote(e.stringRep))
	}
	obj := &TclObj{
		stringRep: strings.Join(parts, " "),
		listRep:   elems,
	}
	obj.handle = cgo.NewHandle(obj)
	return obj
}

// NewDict creates a new empty dict object
func NewDict() *TclObj {
	obj := &TclObj{stringRep: ""}
	obj.handle = cgo.NewHandle(obj)
	return obj
}

// Dup duplicates an object
func (o *TclObj) Dup() *TclObj {
	if o == nil {
		return nil
	}
	dup := &TclObj{stringRep: o.stringRep}
	if o.intRep != nil {
		val := *o.intRep
		dup.intRep = &val
	}
	if o.doubleRep != nil {
		val := *o.doubleRep
		dup.doubleRep = &val
	}
	if o.listRep != nil {
		dup.listRep = make([]*TclObj, len(o.listRep))
		copy(dup.listRep, o.listRep)
	}
	dup.handle = cgo.NewHandle(dup)
	return dup
}

// String returns the string representation
func (o *TclObj) String() string {
	if o == nil {
		return ""
	}
	return o.stringRep
}

// AsInt converts to integer, caching the result
func (o *TclObj) AsInt() (int64, error) {
	if o == nil {
		return 0, fmt.Errorf("nil object")
	}
	if o.intRep != nil {
		return *o.intRep, nil
	}

	s := strings.TrimSpace(o.stringRep)
	var val int64
	var err error

	// Handle different bases
	if strings.HasPrefix(s, "0x") || strings.HasPrefix(s, "0X") {
		val, err = strconv.ParseInt(s[2:], 16, 64)
	} else if strings.HasPrefix(s, "0o") || strings.HasPrefix(s, "0O") {
		val, err = strconv.ParseInt(s[2:], 8, 64)
	} else if strings.HasPrefix(s, "0b") || strings.HasPrefix(s, "0B") {
		val, err = strconv.ParseInt(s[2:], 2, 64)
	} else {
		val, err = strconv.ParseInt(s, 10, 64)
	}

	if err != nil {
		return 0, fmt.Errorf("expected integer but got %q", o.stringRep)
	}

	o.intRep = &val
	return val, nil
}

// AsDouble converts to double, caching the result
func (o *TclObj) AsDouble() (float64, error) {
	if o == nil {
		return 0, fmt.Errorf("nil object")
	}
	if o.doubleRep != nil {
		return *o.doubleRep, nil
	}

	val, err := strconv.ParseFloat(strings.TrimSpace(o.stringRep), 64)
	if err != nil {
		return 0, fmt.Errorf("expected floating-point number but got %q", o.stringRep)
	}

	o.doubleRep = &val
	return val, nil
}

// AsBool converts to boolean
func (o *TclObj) AsBool() (bool, error) {
	if o == nil {
		return false, fmt.Errorf("nil object")
	}

	s := strings.ToLower(strings.TrimSpace(o.stringRep))

	switch s {
	case "1", "true", "yes", "on":
		return true, nil
	case "0", "false", "no", "off":
		return false, nil
	}

	// Try as integer
	if val, err := o.AsInt(); err == nil {
		return val != 0, nil
	}

	return false, fmt.Errorf("expected boolean value but got %q", o.stringRep)
}

// AsList converts to list, caching the result
func (o *TclObj) AsList() ([]*TclObj, error) {
	if o == nil {
		return nil, nil
	}
	if o.listRep != nil {
		return o.listRep, nil
	}

	elems, err := parseList(o.stringRep)
	if err != nil {
		return nil, err
	}

	o.listRep = elems
	return elems, nil
}

// StringLength returns length in Unicode code points
func (o *TclObj) StringLength() int {
	if o == nil {
		return 0
	}
	return utf8.RuneCountInString(o.stringRep)
}

// listQuote quotes a string for list representation if needed
func listQuote(s string) string {
	if s == "" {
		return "{}"
	}

	needsQuoting := false
	braceBalance := 0
	hasBackslash := false

	for _, c := range s {
		switch c {
		case ' ', '\t', '\n', '\r', '"':
			needsQuoting = true
		case '\\':
			hasBackslash = true
			needsQuoting = true
		case '{':
			braceBalance++
		case '}':
			braceBalance--
			if braceBalance < 0 {
				needsQuoting = true
			}
		}
	}

	// Simple case: no special chars and balanced braces
	if !needsQuoting && braceBalance == 0 {
		return s
	}

	// Can use braces if balanced and no backslashes
	if braceBalance == 0 && !hasBackslash {
		return "{" + s + "}"
	}

	// Fall back to backslash quoting
	return backslashQuote(s)
}

// backslashQuote escapes special characters with backslashes
func backslashQuote(s string) string {
	var b strings.Builder
	for _, c := range s {
		switch c {
		case ' ', '\t', '\n', '\r', '{', '}', '"', '\\', '[', ']', '$':
			b.WriteRune('\\')
		}
		b.WriteRune(c)
	}
	return b.String()
}

// parseList parses a TCL list string into elements
func parseList(s string) ([]*TclObj, error) {
	var elems []*TclObj
	s = strings.TrimSpace(s)

	for len(s) > 0 {
		// Skip whitespace
		s = strings.TrimLeft(s, " \t\n\r")
		if len(s) == 0 {
			break
		}

		var elem string
		var rest string

		if s[0] == '{' {
			// Braced element
			depth := 1
			i := 1
			for i < len(s) && depth > 0 {
				if s[i] == '{' {
					depth++
				} else if s[i] == '}' {
					depth--
				}
				if depth > 0 {
					i++
				}
			}
			if depth != 0 {
				return nil, fmt.Errorf("unmatched open brace in list")
			}
			elem = s[1:i]
			rest = s[i+1:]
		} else if s[0] == '"' {
			// Quoted element
			i := 1
			for i < len(s) && s[i] != '"' {
				if s[i] == '\\' && i+1 < len(s) {
					i++
				}
				i++
			}
			if i >= len(s) {
				return nil, fmt.Errorf("unmatched open quote in list")
			}
			elem = s[1:i]
			rest = s[i+1:]
		} else {
			// Bare word
			i := 0
			for i < len(s) && !isListSpace(s[i]) {
				i++
			}
			elem = s[:i]
			rest = s[i:]
		}

		elems = append(elems, NewString(elem))
		s = rest
	}

	return elems, nil
}

func isListSpace(c byte) bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r'
}

// CGO exports for object operations

//export goNewString
func goNewString(s *C.char, length C.size_t) uintptr {
	goStr := C.GoStringN(s, C.int(length))
	obj := NewString(goStr)
	return obj.Handle()
}

//export goNewInt
func goNewInt(val C.int64_t) uintptr {
	obj := NewInt(int64(val))
	return obj.Handle()
}

//export goNewDouble
func goNewDouble(val C.double) uintptr {
	obj := NewDouble(float64(val))
	return obj.Handle()
}

//export goNewBool
func goNewBool(val C.int) uintptr {
	obj := NewBool(val != 0)
	return obj.Handle()
}

//export goNewList
func goNewList(elemsPtr *uintptr, count C.size_t) uintptr {
	if count == 0 {
		obj := NewList(nil)
		return obj.Handle()
	}

	elems := make([]*TclObj, int(count))
	elemSlice := unsafe.Slice(elemsPtr, int(count))
	for i := range elems {
		elems[i] = getObj(elemSlice[i])
	}

	obj := NewList(elems)
	return obj.Handle()
}

//export goNewDict
func goNewDict() uintptr {
	obj := NewDict()
	return obj.Handle()
}

//export goDup
func goDup(h uintptr) uintptr {
	obj := getObj(h)
	if obj == nil {
		return 0
	}
	dup := obj.Dup()
	return dup.Handle()
}

//export goGetStringPtr
func goGetStringPtr(h uintptr, lenOut *C.size_t) *C.char {
	obj := getObj(h)
	if obj == nil {
		if lenOut != nil {
			*lenOut = 0
		}
		return nil
	}

	if lenOut != nil {
		*lenOut = C.size_t(len(obj.stringRep))
	}

	// Cache the C string in the object to avoid repeated allocation
	if obj.cachedCStr == nil {
		obj.cachedCStr = C.CString(obj.stringRep)
	}
	return obj.cachedCStr
}

//export goAsInt
func goAsInt(h uintptr, out *C.int64_t) C.int {
	obj := getObj(h)
	if obj == nil {
		return -1
	}

	val, err := obj.AsInt()
	if err != nil {
		return -1
	}

	*out = C.int64_t(val)
	return 0
}

//export goAsDouble
func goAsDouble(h uintptr, out *C.double) C.int {
	obj := getObj(h)
	if obj == nil {
		return -1
	}

	val, err := obj.AsDouble()
	if err != nil {
		return -1
	}

	*out = C.double(val)
	return 0
}

//export goAsBool
func goAsBool(h uintptr, out *C.int) C.int {
	obj := getObj(h)
	if obj == nil {
		return -1
	}

	val, err := obj.AsBool()
	if err != nil {
		return -1
	}

	if val {
		*out = 1
	} else {
		*out = 0
	}
	return 0
}

//export goStringLength
func goStringLength(h uintptr) C.size_t {
	obj := getObj(h)
	if obj == nil {
		return 0
	}
	return C.size_t(obj.StringLength())
}

//export goStringCompare
func goStringCompare(h1, h2 uintptr) C.int {
	obj1 := getObj(h1)
	obj2 := getObj(h2)

	s1, s2 := "", ""
	if obj1 != nil {
		s1 = obj1.stringRep
	}
	if obj2 != nil {
		s2 = obj2.stringRep
	}

	return C.int(strings.Compare(s1, s2))
}

//export goAsList
func goAsList(h uintptr, elemsOut **C.uintptr_t, countOut *C.size_t) C.int {
	obj := getObj(h)
	if obj == nil {
		*elemsOut = nil
		*countOut = 0
		return 0
	}

	list, err := obj.AsList()
	if err != nil {
		return -1
	}

	if len(list) == 0 {
		*elemsOut = nil
		*countOut = 0
		return 0
	}

	// Allocate C array for handles
	arrPtr := C.malloc(C.size_t(len(list)) * C.size_t(unsafe.Sizeof(C.uintptr_t(0))))
	if arrPtr == nil {
		return -1
	}

	arr := (*[1 << 20]C.uintptr_t)(arrPtr)[:len(list):len(list)]
	for i, elem := range list {
		// Return existing handles instead of duplicating
		arr[i] = C.uintptr_t(elem.Handle())
	}

	*elemsOut = (*C.uintptr_t)(arrPtr)
	*countOut = C.size_t(len(list))
	return 0
}

//export goListLength
func goListLength(h uintptr) C.size_t {
	obj := getObj(h)
	if obj == nil {
		return 0
	}

	list, err := obj.AsList()
	if err != nil {
		return 0
	}
	return C.size_t(len(list))
}

//export goListIndex
func goListIndex(h uintptr, idx C.size_t) uintptr {
	obj := getObj(h)
	if obj == nil {
		return 0
	}

	list, err := obj.AsList()
	if err != nil {
		return 0
	}

	if int(idx) >= len(list) {
		return 0
	}

	// Return existing handle instead of duplicating
	return list[idx].Handle()
}

//export goListAppend
func goListAppend(h uintptr, elemH uintptr) uintptr {
	obj := getObj(h)
	elem := getObj(elemH)

	var list []*TclObj
	if obj != nil {
		var err error
		list, err = obj.AsList()
		if err != nil {
			list = nil
		}
	}

	if elem != nil {
		list = append(list, elem)
	}

	result := NewList(list)
	return result.Handle()
}
