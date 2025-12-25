package interp

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#cgo LDFLAGS: -L${SRCDIR}/../build -lfeather -Wl,-rpath,${SRCDIR}/../build
#include "feather.h"
#include <stdlib.h>

// Implemented in callbacks.c
extern FeatherResult call_feather_eval_obj(FeatherInterp interp, FeatherObj script, FeatherEvalFlags flags);
extern FeatherParseStatus call_feather_parse(FeatherInterp interp, FeatherObj script);
extern void call_feather_interp_init(FeatherInterp interp);

// Type for the list comparison callback
typedef int (*ListCmpFunc)(FeatherInterp interp, FeatherObj a, FeatherObj b, void *ctx);

// Helper to call the comparison function
static inline int call_list_compare(FeatherInterp interp, FeatherObj a, FeatherObj b, void *fn, void *ctx) {
    ListCmpFunc cmp = (ListCmpFunc)fn;
    return cmp(interp, a, b, ctx);
}
*/
import "C"

import (
	"regexp"
	"sort"
	"strings"
	"unicode"
	"unicode/utf8"
	"unsafe"
)

// Go callback implementations - these are called from C via the wrappers in callbacks.c

//export goBindUnknown
func goBindUnknown(interp C.FeatherInterp, cmd C.FeatherObj, args C.FeatherObj, value *C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}

	// Convert args FeatherObj (list) to []FeatherObj slice
	var argSlice []FeatherObj
	if args != 0 {
		o := i.getObject(FeatherObj(args))
		if o != nil && o.isList {
			argSlice = make([]FeatherObj, len(o.listItems))
			copy(argSlice, o.listItems)
		}
	}

	// Dispatch to registered Go commands
	result := i.dispatch(FeatherObj(cmd), argSlice)
	*value = C.FeatherObj(i.result)
	return C.FeatherResult(result)
}

//export goStringIntern
func goStringIntern(interp C.FeatherInterp, s *C.char, length C.size_t) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	goStr := C.GoStringN(s, C.int(length))
	return C.FeatherObj(i.internString(goStr))
}

//export goStringGet
func goStringGet(interp C.FeatherInterp, obj C.FeatherObj, length *C.size_t) *C.char {
	i := getInterp(interp)
	if i == nil {
		*length = 0
		return nil
	}
	// Use GetString for shimmering (int/list → string)
	str := i.GetString(FeatherObj(obj))
	if str == "" {
		// Check if object exists but has empty string
		o := i.getObject(FeatherObj(obj))
		if o == nil {
			*length = 0
			return nil
		}
	}
	// Cache the C string in the object to avoid memory issues
	o := i.getObject(FeatherObj(obj))
	if o.cstr == nil {
		o.cstr = C.CString(str)
	}
	*length = C.size_t(len(str))
	return o.cstr
}

//export goStringConcat
func goStringConcat(interp C.FeatherInterp, a C.FeatherObj, b C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetString for shimmering (int/list → string)
	strA := i.GetString(FeatherObj(a))
	strB := i.GetString(FeatherObj(b))
	return C.FeatherObj(i.internString(strA + strB))
}

//export goStringCompare
func goStringCompare(interp C.FeatherInterp, a C.FeatherObj, b C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetString for shimmering (int/list → string)
	strA := i.GetString(FeatherObj(a))
	strB := i.GetString(FeatherObj(b))
	// Go's string comparison is already Unicode-aware (UTF-8)
	if strA < strB {
		return -1
	}
	if strA > strB {
		return 1
	}
	return 0
}

//export goStringRegexMatch
func goStringRegexMatch(interp C.FeatherInterp, pattern C.FeatherObj, str C.FeatherObj, result *C.int) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	patternStr := i.GetString(FeatherObj(pattern))
	strStr := i.GetString(FeatherObj(str))

	re, err := regexp.Compile(patternStr)
	if err != nil {
		errMsg := i.internString("couldn't compile regular expression pattern: " + err.Error())
		i.result = errMsg
		return C.TCL_ERROR
	}

	if re.MatchString(strStr) {
		*result = 1
	} else {
		*result = 0
	}
	return C.TCL_OK
}

// Rune operations (Unicode-aware)

//export goRuneLength
func goRuneLength(interp C.FeatherInterp, str C.FeatherObj) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	s := i.GetString(FeatherObj(str))
	return C.size_t(utf8.RuneCountInString(s))
}

//export goRuneAt
func goRuneAt(interp C.FeatherInterp, str C.FeatherObj, index C.size_t) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	s := i.GetString(FeatherObj(str))
	runes := []rune(s)
	idx := int(index)
	if idx < 0 || idx >= len(runes) {
		return C.FeatherObj(i.internString(""))
	}
	return C.FeatherObj(i.internString(string(runes[idx])))
}

//export goRuneRange
func goRuneRange(interp C.FeatherInterp, str C.FeatherObj, first C.int64_t, last C.int64_t) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	s := i.GetString(FeatherObj(str))
	runes := []rune(s)
	length := len(runes)

	// Clamp indices
	f := int(first)
	l := int(last)
	if f < 0 {
		f = 0
	}
	if l >= length {
		l = length - 1
	}
	if f > l || length == 0 {
		return C.FeatherObj(i.internString(""))
	}

	return C.FeatherObj(i.internString(string(runes[f : l+1])))
}

//export goRuneToUpper
func goRuneToUpper(interp C.FeatherInterp, str C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	s := i.GetString(FeatherObj(str))
	return C.FeatherObj(i.internString(strings.ToUpper(s)))
}

//export goRuneToLower
func goRuneToLower(interp C.FeatherInterp, str C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	s := i.GetString(FeatherObj(str))
	return C.FeatherObj(i.internString(strings.ToLower(s)))
}

//export goRuneFold
func goRuneFold(interp C.FeatherInterp, str C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	s := i.GetString(FeatherObj(str))
	// Case folding: convert each rune to its folded form
	// This handles cases like ß -> ss properly for comparison
	var result strings.Builder
	for _, r := range s {
		// unicode.SimpleFold gives the next case-folded rune in the chain
		// We use ToLower as primary, but handle special cases
		folded := unicode.ToLower(r)
		// Special case: German sharp s (ß) folds to "ss"
		if r == 'ß' {
			result.WriteString("ss")
		} else {
			result.WriteRune(folded)
		}
	}
	return C.FeatherObj(i.internString(result.String()))
}

//export goInterpSetResult
func goInterpSetResult(interp C.FeatherInterp, result C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.result = FeatherObj(result)
	return C.TCL_OK
}

//export goInterpGetResult
func goInterpGetResult(interp C.FeatherInterp) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.FeatherObj(i.result)
}

//export goInterpResetResult
func goInterpResetResult(interp C.FeatherInterp, result C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.result = 0
	return C.TCL_OK
}

//export goInterpSetReturnOptions
func goInterpSetReturnOptions(interp C.FeatherInterp, options C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	i.returnOptions = FeatherObj(options)
	return C.TCL_OK
}

//export goInterpGetReturnOptions
func goInterpGetReturnOptions(interp C.FeatherInterp, code C.FeatherResult) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.FeatherObj(i.returnOptions)
}

//export goListCreate
func goListCreate(interp C.FeatherInterp) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
	return C.FeatherObj(id)
}

//export goListIsNil
func goListIsNil(interp C.FeatherInterp, obj C.FeatherObj) C.int {
	if obj == 0 {
		return 1
	}
	return 0
}

//export goListFrom
func goListFrom(interp C.FeatherInterp, obj C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Get the list items (with shimmering)
	items, err := i.GetList(FeatherObj(obj))
	if err != nil {
		return 0
	}
	// Create a new list with copied items
	id := i.nextID
	i.nextID++
	// Make a copy of the items slice
	copiedItems := make([]FeatherObj, len(items))
	copy(copiedItems, items)
	i.objects[id] = &Object{isList: true, listItems: copiedItems}
	return C.FeatherObj(id)
}

//export goListPush
func goListPush(interp C.FeatherInterp, list C.FeatherObj, item C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return list
	}
	// Use GetList for shimmering (string → list)
	_, err := i.GetList(FeatherObj(list))
	if err != nil {
		return list
	}
	o := i.getObject(FeatherObj(list))
	if o == nil {
		return list
	}
	o.listItems = append(o.listItems, FeatherObj(item))
	// Invalidate string and dict representations (list is now authoritative)
	o.stringVal = ""
	o.isDict = false
	o.dictItems = nil
	o.dictOrder = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}
	return list
}

//export goListPop
func goListPop(interp C.FeatherInterp, list C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetList for shimmering (string → list)
	items, err := i.GetList(FeatherObj(list))
	if err != nil || len(items) == 0 {
		return 0
	}
	o := i.getObject(FeatherObj(list))
	if o == nil {
		return 0
	}
	last := o.listItems[len(items)-1]
	o.listItems = o.listItems[:len(items)-1]
	// Invalidate string and dict representations (list is now authoritative)
	o.stringVal = ""
	o.isDict = false
	o.dictItems = nil
	o.dictOrder = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}
	return C.FeatherObj(last)
}

//export goListUnshift
func goListUnshift(interp C.FeatherInterp, list C.FeatherObj, item C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return list
	}
	// Use GetList for shimmering (string → list)
	_, err := i.GetList(FeatherObj(list))
	if err != nil {
		return list
	}
	o := i.getObject(FeatherObj(list))
	if o == nil {
		return list
	}
	// Prepend item to the list
	o.listItems = append([]FeatherObj{FeatherObj(item)}, o.listItems...)
	// Invalidate string and dict representations (list is now authoritative)
	o.stringVal = ""
	o.isDict = false
	o.dictItems = nil
	o.dictOrder = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}
	return list
}

//export goListShift
func goListShift(interp C.FeatherInterp, list C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetList for shimmering (string → list)
	items, err := i.GetList(FeatherObj(list))
	if err != nil || len(items) == 0 {
		return 0
	}
	o := i.getObject(FeatherObj(list))
	if o == nil {
		return 0
	}
	first := o.listItems[0]
	o.listItems = o.listItems[1:]
	// Invalidate string and dict representations (list is now authoritative)
	o.stringVal = ""
	o.isDict = false
	o.dictItems = nil
	o.dictOrder = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}
	return C.FeatherObj(first)
}

//export goListLength
func goListLength(interp C.FeatherInterp, list C.FeatherObj) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Use GetList for shimmering (string → list)
	items, err := i.GetList(FeatherObj(list))
	if err != nil {
		return 0
	}
	return C.size_t(len(items))
}

//export goListAt
func goListAt(interp C.FeatherInterp, list C.FeatherObj, index C.size_t) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	items, err := i.GetList(FeatherObj(list))
	if err != nil {
		return 0
	}
	idx := int(index)
	if idx < 0 || idx >= len(items) {
		return 0
	}
	return C.FeatherObj(items[idx])
}

//export goListSlice
func goListSlice(interp C.FeatherInterp, list C.FeatherObj, first C.size_t, last C.size_t) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	items, err := i.GetList(FeatherObj(list))
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
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
		return C.FeatherObj(id)
	}

	// Create new list with sliced items
	slicedItems := make([]FeatherObj, l-f+1)
	copy(slicedItems, items[f:l+1])

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: slicedItems}
	return C.FeatherObj(id)
}

//export goListSetAt
func goListSetAt(interp C.FeatherInterp, list C.FeatherObj, index C.size_t, value C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}

	// Use GetList for shimmering (string → list)
	items, err := i.GetList(FeatherObj(list))
	if err != nil {
		return C.TCL_ERROR
	}

	idx := int(index)
	if idx < 0 || idx >= len(items) {
		return C.TCL_ERROR
	}

	o := i.getObject(FeatherObj(list))
	if o == nil {
		return C.TCL_ERROR
	}

	// Mutate in place
	o.listItems[idx] = FeatherObj(value)

	// Invalidate string and dict representations (list is now authoritative)
	o.stringVal = ""
	o.isDict = false
	o.dictItems = nil
	o.dictOrder = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}

	return C.TCL_OK
}

//export goListSplice
func goListSplice(interp C.FeatherInterp, list C.FeatherObj, first C.size_t, deleteCount C.size_t, insertions C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}

	items, err := i.GetList(FeatherObj(list))
	if err != nil {
		return 0
	}

	f := int(first)
	dc := int(deleteCount)
	length := len(items)

	// Get insertion items
	var insertItems []FeatherObj
	if insertions != 0 {
		insertItems, err = i.GetList(FeatherObj(insertions))
		if err != nil {
			insertItems = []FeatherObj{}
		}
	}

	// Clamp first index
	if f < 0 {
		f = 0
	}
	if f > length {
		f = length
	}

	// Clamp delete count
	if f+dc > length {
		dc = length - f
	}

	// Build new list: [0:first] + insertItems + [first+deleteCount:]
	newLen := length - dc + len(insertItems)
	newItems := make([]FeatherObj, 0, newLen)
	newItems = append(newItems, items[:f]...)
	newItems = append(newItems, insertItems...)
	if f+dc < length {
		newItems = append(newItems, items[f+dc:]...)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: newItems}
	return C.FeatherObj(id)
}

// ListSortContext holds context for list sorting
type ListSortContext struct {
	interp  C.FeatherInterp
	cmpFunc unsafe.Pointer
	ctx     unsafe.Pointer
}

// Global sort context for the current sort operation
var currentSortCtx *ListSortContext

//export goListSort
func goListSort(interp C.FeatherInterp, list C.FeatherObj, cmpFunc unsafe.Pointer, ctx unsafe.Pointer) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}

	// Use GetList for shimmering (string → list)
	items, err := i.GetList(FeatherObj(list))
	if err != nil {
		return C.TCL_ERROR
	}

	if len(items) <= 1 {
		return C.TCL_OK // Already sorted
	}

	o := i.getObject(FeatherObj(list))
	if o == nil {
		return C.TCL_ERROR
	}

	// Set up sort context
	currentSortCtx = &ListSortContext{
		interp:  interp,
		cmpFunc: cmpFunc,
		ctx:     ctx,
	}

	// Sort using Go's sort with the C comparison function
	sort.Slice(o.listItems, func(a, b int) bool {
		result := C.call_list_compare(currentSortCtx.interp, C.FeatherObj(o.listItems[a]), C.FeatherObj(o.listItems[b]),
			currentSortCtx.cmpFunc, currentSortCtx.ctx)
		return result < 0
	})

	currentSortCtx = nil

	// Invalidate string and dict representations (list is now authoritative)
	o.stringVal = ""
	o.isDict = false
	o.dictItems = nil
	o.dictOrder = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}

	return C.TCL_OK
}

// Dict operations

//export goDictCreate
func goDictCreate(interp C.FeatherInterp) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{
		isDict:    true,
		dictItems: make(map[string]FeatherObj),
		dictOrder: []string{},
	}
	return C.FeatherObj(id)
}

//export goDictIsDict
func goDictIsDict(interp C.FeatherInterp, obj C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	o := i.getObject(FeatherObj(obj))
	if o != nil && o.isDict {
		return 1
	}
	return 0
}

//export goDictFrom
func goDictFrom(interp C.FeatherInterp, obj C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	// Try to convert to dict
	dictItems, dictOrder, err := i.GetDict(FeatherObj(obj))
	if err != nil {
		return 0 // Return nil on error
	}
	// Create new dict object with copied data
	id := i.nextID
	i.nextID++
	newItems := make(map[string]FeatherObj, len(dictItems))
	for k, v := range dictItems {
		newItems[k] = v
	}
	newOrder := make([]string, len(dictOrder))
	copy(newOrder, dictOrder)
	i.objects[id] = &Object{
		isDict:    true,
		dictItems: newItems,
		dictOrder: newOrder,
	}
	return C.FeatherObj(id)
}

//export goDictGet
func goDictGet(interp C.FeatherInterp, dict C.FeatherObj, key C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	dictItems, _, err := i.GetDict(FeatherObj(dict))
	if err != nil {
		return 0
	}
	keyStr := i.GetString(FeatherObj(key))
	if val, ok := dictItems[keyStr]; ok {
		return C.FeatherObj(val)
	}
	return 0 // Key not found
}

//export goDictSet
func goDictSet(interp C.FeatherInterp, dict C.FeatherObj, key C.FeatherObj, value C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	o := i.getObject(FeatherObj(dict))
	if o == nil {
		return 0
	}
	// Ensure it's a dict (shimmer if needed)
	if !o.isDict {
		_, _, err := i.GetDict(FeatherObj(dict))
		if err != nil {
			return 0
		}
	}
	keyStr := i.GetString(FeatherObj(key))
	// Add to order if new key
	if _, exists := o.dictItems[keyStr]; !exists {
		o.dictOrder = append(o.dictOrder, keyStr)
	}
	o.dictItems[keyStr] = FeatherObj(value)
	// Invalidate string and list caches (dict is now authoritative)
	o.stringVal = ""
	o.isList = false
	o.listItems = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}
	return dict
}

//export goDictExists
func goDictExists(interp C.FeatherInterp, dict C.FeatherObj, key C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	dictItems, _, err := i.GetDict(FeatherObj(dict))
	if err != nil {
		return 0
	}
	keyStr := i.GetString(FeatherObj(key))
	if _, ok := dictItems[keyStr]; ok {
		return 1
	}
	return 0
}

//export goDictRemove
func goDictRemove(interp C.FeatherInterp, dict C.FeatherObj, key C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	o := i.getObject(FeatherObj(dict))
	if o == nil {
		return 0
	}
	// Ensure it's a dict
	if !o.isDict {
		_, _, err := i.GetDict(FeatherObj(dict))
		if err != nil {
			return 0
		}
	}
	keyStr := i.GetString(FeatherObj(key))
	// Remove from map
	delete(o.dictItems, keyStr)
	// Remove from order
	for idx, k := range o.dictOrder {
		if k == keyStr {
			o.dictOrder = append(o.dictOrder[:idx], o.dictOrder[idx+1:]...)
			break
		}
	}
	// Invalidate string and list caches (dict is now authoritative)
	o.stringVal = ""
	o.isList = false
	o.listItems = nil
	if o.cstr != nil {
		C.free(unsafe.Pointer(o.cstr))
		o.cstr = nil
	}
	return dict
}

//export goDictSize
func goDictSize(interp C.FeatherInterp, dict C.FeatherObj) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	dictItems, _, err := i.GetDict(FeatherObj(dict))
	if err != nil {
		return 0
	}
	return C.size_t(len(dictItems))
}

//export goDictKeys
func goDictKeys(interp C.FeatherInterp, dict C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	_, dictOrder, err := i.GetDict(FeatherObj(dict))
	if err != nil {
		return 0
	}
	// Create list of keys
	id := i.nextID
	i.nextID++
	items := make([]FeatherObj, len(dictOrder))
	for idx, key := range dictOrder {
		items[idx] = i.internString(key)
	}
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goDictValues
func goDictValues(interp C.FeatherInterp, dict C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	dictItems, dictOrder, err := i.GetDict(FeatherObj(dict))
	if err != nil {
		return 0
	}
	// Create list of values in key order
	id := i.nextID
	i.nextID++
	items := make([]FeatherObj, len(dictOrder))
	for idx, key := range dictOrder {
		items[idx] = dictItems[key]
	}
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goIntCreate
func goIntCreate(interp C.FeatherInterp, val C.int64_t) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{intVal: int64(val), isInt: true}
	return C.FeatherObj(id)
}

//export goIntGet
func goIntGet(interp C.FeatherInterp, obj C.FeatherObj, out *C.int64_t) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	val, err := i.GetInt(FeatherObj(obj))
	if err != nil {
		return C.TCL_ERROR
	}
	*out = C.int64_t(val)
	return C.TCL_OK
}

//export goDoubleCreate
func goDoubleCreate(interp C.FeatherInterp, val C.double) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{dblVal: float64(val), isDouble: true}
	return C.FeatherObj(id)
}

//export goDoubleGet
func goDoubleGet(interp C.FeatherInterp, obj C.FeatherObj, out *C.double) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	val, err := i.GetDouble(FeatherObj(obj))
	if err != nil {
		return C.TCL_ERROR
	}
	*out = C.double(val)
	return C.TCL_OK
}

//export goFramePush
func goFramePush(interp C.FeatherInterp, cmd C.FeatherObj, args C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	newLevel := len(i.frames)
	// Check recursion limit
	limit := i.recursionLimit
	if limit <= 0 {
		limit = DefaultRecursionLimit
	}
	if newLevel >= limit {
		// Set error message and return error
		errMsg := "too many nested evaluations (infinite loop?)"
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{stringVal: errMsg}
		i.result = id
		return C.TCL_ERROR
	}
	// Inherit namespace from current frame
	currentNS := i.globalNamespace
	if i.active < len(i.frames) && i.frames[i.active].ns != nil {
		currentNS = i.frames[i.active].ns
	}
	// Create an anonymous locals namespace for this frame's variables
	localsNS := &Namespace{
		vars: make(map[string]FeatherObj),
	}
	frame := &CallFrame{
		cmd:    FeatherObj(cmd),
		args:   FeatherObj(args),
		locals: localsNS,
		links:  make(map[string]varLink),
		level:  newLevel,
		ns:     currentNS,
	}
	i.frames = append(i.frames, frame)
	i.active = newLevel
	return C.TCL_OK
}

//export goFramePop
func goFramePop(interp C.FeatherInterp) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	// Cannot pop the global frame (frame 0)
	if len(i.frames) <= 1 {
		return C.TCL_ERROR
	}
	i.frames = i.frames[:len(i.frames)-1]
	i.active = len(i.frames) - 1
	return C.TCL_OK
}

//export goFrameLevel
func goFrameLevel(interp C.FeatherInterp) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.size_t(i.active)
}

//export goFrameSetActive
func goFrameSetActive(interp C.FeatherInterp, level C.size_t) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	lvl := int(level)
	if lvl < 0 || lvl >= len(i.frames) {
		return C.TCL_ERROR
	}
	i.active = lvl
	return C.TCL_OK
}

//export goFrameSize
func goFrameSize(interp C.FeatherInterp) C.size_t {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	return C.size_t(len(i.frames))
}

//export goFrameInfo
func goFrameInfo(interp C.FeatherInterp, level C.size_t, cmd *C.FeatherObj, args *C.FeatherObj, ns *C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	lvl := int(level)
	if lvl < 0 || lvl >= len(i.frames) {
		return C.TCL_ERROR
	}
	frame := i.frames[lvl]
	*cmd = C.FeatherObj(frame.cmd)
	*args = C.FeatherObj(frame.args)
	// Return the frame's namespace
	if frame.ns != nil {
		*ns = C.FeatherObj(i.internString(frame.ns.fullPath))
	} else {
		*ns = C.FeatherObj(i.internString("::"))
	}
	return C.TCL_OK
}

//export goVarGet
func goVarGet(interp C.FeatherInterp, name C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	nameObj := i.getObject(FeatherObj(name))
	if nameObj == nil {
		return 0
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	originalVarName := varName // Save for trace firing
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					if val, ok := ns.vars[link.nsName]; ok {
						// Copy traces before unlocking
						traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
						copy(traces, i.varTraces[originalVarName])
						// Fire read traces
						if len(traces) > 0 {
							fireVarTraces(i, originalVarName, "read", traces)
						}
						return C.FeatherObj(val)
					}
				}
				return 0
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return 0
			}
		} else {
			break
		}
	}
	var result C.FeatherObj
	if val, ok := frame.locals.vars[varName]; ok {
		result = C.FeatherObj(val)
	}
	// Copy traces before unlocking
	traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
	copy(traces, i.varTraces[originalVarName])
	// Fire read traces
	if len(traces) > 0 {
		fireVarTraces(i, originalVarName, "read", traces)
	}
	return result
}

//export goVarSet
func goVarSet(interp C.FeatherInterp, name C.FeatherObj, value C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameObj := i.getObject(FeatherObj(name))
	if nameObj == nil {
		return
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	originalVarName := varName // Save for trace firing
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					ns.vars[link.nsName] = FeatherObj(value)
				}
				// Copy traces before unlocking
				traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
				copy(traces, i.varTraces[originalVarName])
				// Fire write traces
				if len(traces) > 0 {
					fireVarTraces(i, originalVarName, "write", traces)
				}
				return
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return
			}
		} else {
			break
		}
	}
	frame.locals.vars[varName] = FeatherObj(value)
	// Copy traces before unlocking
	traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
	copy(traces, i.varTraces[originalVarName])
	// Fire write traces
	if len(traces) > 0 {
		fireVarTraces(i, originalVarName, "write", traces)
	}
}

//export goVarUnset
func goVarUnset(interp C.FeatherInterp, name C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameObj := i.getObject(FeatherObj(name))
	if nameObj == nil {
		return
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	originalVarName := varName // Save for trace firing
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					delete(ns.vars, link.nsName)
				}
				// Copy traces before unlocking
				traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
				copy(traces, i.varTraces[originalVarName])
				// Fire unset traces
				if len(traces) > 0 {
					fireVarTraces(i, originalVarName, "unset", traces)
				}
				return
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return
			}
		} else {
			break
		}
	}
	delete(frame.locals.vars, varName)
	// Copy traces before unlocking
	traces := make([]TraceEntry, len(i.varTraces[originalVarName]))
	copy(traces, i.varTraces[originalVarName])
	// Fire unset traces
	if len(traces) > 0 {
		fireVarTraces(i, originalVarName, "unset", traces)
	}
}

//export goVarExists
func goVarExists(interp C.FeatherInterp, name C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameObj := i.getObject(FeatherObj(name))
	if nameObj == nil {
		return C.TCL_ERROR
	}
	frame := i.frames[i.active]
	varName := nameObj.stringVal
	// Follow links to find the actual variable location
	for {
		if link, ok := frame.links[varName]; ok {
			if link.targetLevel == -1 {
				// Namespace variable link
				if ns, ok := i.namespaces[link.nsPath]; ok {
					if _, ok := ns.vars[link.nsName]; ok {
						return C.TCL_OK
					}
				}
				return C.TCL_ERROR
			} else if link.targetLevel >= 0 && link.targetLevel < len(i.frames) {
				frame = i.frames[link.targetLevel]
				varName = link.targetName
			} else {
				return C.TCL_ERROR
			}
		} else {
			break
		}
	}
	if _, ok := frame.locals.vars[varName]; ok {
		return C.TCL_OK
	}
	return C.TCL_ERROR
}

//export goVarLink
func goVarLink(interp C.FeatherInterp, local C.FeatherObj, target_level C.size_t, target C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	localObj := i.getObject(FeatherObj(local))
	targetObj := i.getObject(FeatherObj(target))
	if localObj == nil || targetObj == nil {
		return
	}
	frame := i.frames[i.active]
	frame.links[localObj.stringVal] = varLink{
		targetLevel: int(target_level),
		targetName:  targetObj.stringVal,
	}
}

//export goProcDefine
func goProcDefine(interp C.FeatherInterp, name C.FeatherObj, params C.FeatherObj, body C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameStr := i.GetString(FeatherObj(name))
	proc := &Procedure{
		name:   FeatherObj(name),
		params: FeatherObj(params),
		body:   FeatherObj(body),
	}
	cmd := &Command{
		cmdType: CmdProc,
		proc:    proc,
	}

	// Store in namespace's commands map
	// Split the qualified name into namespace and simple name
	var nsPath, simpleName string
	if strings.HasPrefix(nameStr, "::") {
		// Absolute path like "::foo::bar" or "::cmd"
		lastSep := strings.LastIndex(nameStr, "::")
		if lastSep == 0 {
			// Just "::cmd"
			nsPath = "::"
			simpleName = nameStr[2:]
		} else {
			nsPath = nameStr[:lastSep]
			simpleName = nameStr[lastSep+2:]
		}
	} else {
		// Should not happen - procs should always be fully qualified by now
		nsPath = "::"
		simpleName = nameStr
	}

	ns := i.ensureNamespace(nsPath)
	ns.commands[simpleName] = cmd
}

// lookupCommandByQualified looks up a command by its fully-qualified name.
// Returns the command and true if found, nil and false otherwise.
func (i *Interp) lookupCommandByQualified(nameStr string) (*Command, bool) {
	var nsPath, simpleName string
	if strings.HasPrefix(nameStr, "::") {
		lastSep := strings.LastIndex(nameStr, "::")
		if lastSep == 0 {
			nsPath = "::"
			simpleName = nameStr[2:]
		} else {
			nsPath = nameStr[:lastSep]
			simpleName = nameStr[lastSep+2:]
		}
	} else {
		nsPath = "::"
		simpleName = nameStr
	}

	ns, ok := i.namespaces[nsPath]
	if !ok {
		return nil, false
	}
	cmd, ok := ns.commands[simpleName]
	return cmd, ok
}

//export goProcExists
func goProcExists(interp C.FeatherInterp, name C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	nameStr := i.GetString(FeatherObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if ok && cmd.cmdType == CmdProc {
		return 1
	}
	return 0
}

//export goProcParams
func goProcParams(interp C.FeatherInterp, name C.FeatherObj, result *C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameStr := i.GetString(FeatherObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if !ok || cmd.cmdType != CmdProc || cmd.proc == nil {
		return C.TCL_ERROR
	}
	*result = C.FeatherObj(cmd.proc.params)
	return C.TCL_OK
}

//export goProcBody
func goProcBody(interp C.FeatherInterp, name C.FeatherObj, result *C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	nameStr := i.GetString(FeatherObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if !ok || cmd.cmdType != CmdProc || cmd.proc == nil {
		return C.TCL_ERROR
	}
	*result = C.FeatherObj(cmd.proc.body)
	return C.TCL_OK
}

// callCEval invokes the C interpreter
func callCEval(interpHandle FeatherInterp, scriptHandle FeatherObj) C.FeatherResult {
	return C.call_feather_eval_obj(C.FeatherInterp(interpHandle), C.FeatherObj(scriptHandle), C.TCL_EVAL_LOCAL)
}

// fireVarTraces fires variable traces for the given operation.
// This function must be called WITHOUT holding the mutex.
func fireVarTraces(i *Interp, varName string, op string, traces []TraceEntry) {
	// Variable traces are invoked as: command name1 name2 op
	// - name1 is the variable name
	// - name2 is empty (array element, not supported)
	// - op is "read", "write", or "unset"
	for _, trace := range traces {
		// Check if this trace matches the operation
		ops := strings.Fields(trace.ops)
		matches := false
		for _, traceOp := range ops {
			if traceOp == op {
				matches = true
				break
			}
		}
		if !matches {
			continue
		}

		// Get the script command prefix - GetString acquires lock internally
		scriptStr := i.GetString(trace.script)
		// Build the full command: script name1 name2 op
		// We'll construct this as a string to eval
		cmd := scriptStr + " " + varName + " {} " + op
		cmdObj := i.internString(cmd)

		// Fire the trace by evaluating the command
		callCEval(i.handle, cmdObj)
	}
}

// fireCmdTraces fires command traces for the given operation.
// This function must be called WITHOUT holding the mutex.
func fireCmdTraces(i *Interp, oldName string, newName string, op string, traces []TraceEntry) {
	// Command traces are invoked as: command oldName newName op
	// - oldName is the original command name
	// - newName is the new name (empty for delete)
	// - op is "rename" or "delete"
	for _, trace := range traces {
		// Check if this trace matches the operation
		ops := strings.Fields(trace.ops)
		matches := false
		for _, traceOp := range ops {
			if traceOp == op {
				matches = true
				break
			}
		}
		if !matches {
			continue
		}

		// Get the script command prefix - GetString acquires lock internally
		scriptStr := i.GetString(trace.script)
		// Build the full command: script oldName newName op
		// Use display names (strip :: for global namespace commands)
		displayOld := i.DisplayName(oldName)
		displayNew := i.DisplayName(newName)
		// Empty strings must be properly quoted with {}
		quotedNew := displayNew
		if displayNew == "" {
			quotedNew = "{}"
		}
		cmd := scriptStr + " " + displayOld + " " + quotedNew + " " + op
		cmdObj := i.internString(cmd)

		// Fire the trace by evaluating the command
		callCEval(i.handle, cmdObj)
	}
}

// callCParse invokes the C parser
func callCParse(interpHandle FeatherInterp, scriptHandle FeatherObj) C.FeatherParseStatus {
	return C.call_feather_parse(C.FeatherInterp(interpHandle), C.FeatherObj(scriptHandle))
}

// callCInterpInit invokes the C interpreter initialization
func callCInterpInit(interpHandle FeatherInterp) {
	C.call_feather_interp_init(C.FeatherInterp(interpHandle))
}

//export goProcNames
func goProcNames(interp C.FeatherInterp, namespace C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}

	// Determine which namespace to list (default to global)
	nsPath := "::"
	if namespace != 0 {
		nsPath = i.GetString(FeatherObj(namespace))
	}
	if nsPath == "" {
		nsPath = "::"
	}

	ns, ok := i.namespaces[nsPath]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
		return C.FeatherObj(id)
	}

	// Collect command names and build fully-qualified names
	names := make([]string, 0, len(ns.commands))
	for name := range ns.commands {
		// Build fully-qualified name
		var fullName string
		if nsPath == "::" {
			fullName = "::" + name
		} else {
			fullName = nsPath + "::" + name
		}
		names = append(names, fullName)
	}

	// Sort for consistent ordering
	sort.Strings(names)

	// Create a list object with all names
	items := make([]FeatherObj, len(names))
	for idx, name := range names {
		items[idx] = i.internString(name)
	}
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goProcResolveNamespace
func goProcResolveNamespace(interp C.FeatherInterp, path C.FeatherObj, result *C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	// For now, only the global namespace "::" exists
	// If path is nil, empty, or "::", return global namespace
	if path == 0 {
		*result = C.FeatherObj(i.globalNS)
		return C.TCL_OK
	}
	pathStr := i.GetString(FeatherObj(path))
	if pathStr == "" || pathStr == "::" {
		*result = C.FeatherObj(i.globalNS)
		return C.TCL_OK
	}
	// Any other namespace doesn't exist yet
	i.SetErrorString("namespace \"" + pathStr + "\" not found")
	return C.TCL_ERROR
}

//export goProcRegisterBuiltin
func goProcRegisterBuiltin(interp C.FeatherInterp, name C.FeatherObj, fn C.FeatherBuiltinCmd) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	nameStr := i.GetString(FeatherObj(name))

	// Split the qualified name and store in namespace
	var nsPath, simpleName string
	if strings.HasPrefix(nameStr, "::") {
		lastSep := strings.LastIndex(nameStr, "::")
		if lastSep == 0 {
			nsPath = "::"
			simpleName = nameStr[2:]
		} else {
			nsPath = nameStr[:lastSep]
			simpleName = nameStr[lastSep+2:]
		}
	} else {
		nsPath = "::"
		simpleName = nameStr
	}

	cmd := &Command{
		cmdType: CmdBuiltin,
		builtin: fn,
	}
	ns := i.ensureNamespace(nsPath)
	ns.commands[simpleName] = cmd
}

//export goProcLookup
func goProcLookup(interp C.FeatherInterp, name C.FeatherObj, fn *C.FeatherBuiltinCmd) C.FeatherCommandType {
	i := getInterp(interp)
	if i == nil {
		*fn = nil
		return C.TCL_CMD_NONE
	}
	nameStr := i.GetString(FeatherObj(name))
	cmd, ok := i.lookupCommandByQualified(nameStr)
	if !ok {
		*fn = nil
		return C.TCL_CMD_NONE
	}
	switch cmd.cmdType {
	case CmdBuiltin:
		*fn = cmd.builtin
		return C.TCL_CMD_BUILTIN
	case CmdProc:
		*fn = nil
		return C.TCL_CMD_PROC
	default:
		*fn = nil
		return C.TCL_CMD_NONE
	}
}

//export goProcRename
func goProcRename(interp C.FeatherInterp, oldName C.FeatherObj, newName C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	oldNameStr := i.GetString(FeatherObj(oldName))
	newNameStr := i.GetString(FeatherObj(newName))

	// Helper to split qualified name into namespace and simple name
	splitQualified := func(name string) (nsPath, simple string) {
		if strings.HasPrefix(name, "::") {
			lastSep := strings.LastIndex(name, "::")
			if lastSep == 0 {
				return "::", name[2:]
			}
			return name[:lastSep], name[lastSep+2:]
		}
		return "::", name
	}

	// Get old namespace location
	oldNsPath, oldSimple := splitQualified(oldNameStr)

	// Compute fully qualified name for trace lookups
	// Traces are always registered with fully qualified names
	oldQualified := oldNameStr
	if !strings.HasPrefix(oldNameStr, "::") {
		if oldNsPath == "::" {
			oldQualified = "::" + oldSimple
		} else {
			oldQualified = oldNsPath + "::" + oldSimple
		}
	}

	// Check if old command exists in namespace
	oldNs, ok := i.namespaces[oldNsPath]
	if !ok {
		i.SetErrorString("can't rename \"" + i.DisplayName(oldNameStr) + "\": command doesn't exist")
		return C.TCL_ERROR
	}
	cmd, ok := oldNs.commands[oldSimple]
	if !ok {
		i.SetErrorString("can't rename \"" + i.DisplayName(oldNameStr) + "\": command doesn't exist")
		return C.TCL_ERROR
	}

	// If newName is empty, delete the command
	if newNameStr == "" {
		delete(oldNs.commands, oldSimple)
		// Copy traces before unlocking - use fully qualified name
		traces := make([]TraceEntry, len(i.cmdTraces[oldQualified]))
		copy(traces, i.cmdTraces[oldQualified])
		// Fire delete traces
		if len(traces) > 0 {
			fireCmdTraces(i, oldQualified, "", "delete", traces)
		}
		return C.TCL_OK
	}

	// Get new namespace location
	newNsPath, newSimple := splitQualified(newNameStr)

	// Check if new name already exists
	newNs := i.ensureNamespace(newNsPath)
	if _, exists := newNs.commands[newSimple]; exists {
		i.SetErrorString("can't rename to \"" + i.DisplayName(newNameStr) + "\": command already exists")
		return C.TCL_ERROR
	}

	// Move command from old namespace to new namespace
	delete(oldNs.commands, oldSimple)
	newNs.commands[newSimple] = cmd

	// Copy traces before unlocking - use fully qualified name
	traces := make([]TraceEntry, len(i.cmdTraces[oldQualified]))
	copy(traces, i.cmdTraces[oldQualified])
	// Fire rename traces
	if len(traces) > 0 {
		fireCmdTraces(i, oldQualified, newNameStr, "rename", traces)
	}

	return C.TCL_OK
}

// Helper to create or get a namespace by path
func (i *Interp) ensureNamespace(path string) *Namespace {
	if ns, ok := i.namespaces[path]; ok {
		return ns
	}
	// Parse path and create hierarchy
	// Path like "::foo::bar" -> create :: -> ::foo -> ::foo::bar
	parts := strings.Split(strings.TrimPrefix(path, "::"), "::")
	current := i.globalNamespace
	currentPath := "::"

	for _, part := range parts {
		if part == "" {
			continue
		}
		childPath := currentPath
		if currentPath == "::" {
			childPath = "::" + part
		} else {
			childPath = currentPath + "::" + part
		}

		child, ok := current.children[part]
		if !ok {
			child = &Namespace{
				fullPath: childPath,
				parent:   current,
				children: make(map[string]*Namespace),
				vars:     make(map[string]FeatherObj),
				commands: make(map[string]*Command),
			}
			current.children[part] = child
			i.namespaces[childPath] = child
		}
		current = child
		currentPath = childPath
	}
	return current
}

//export goNsCreate
func goNsCreate(interp C.FeatherInterp, path C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(FeatherObj(path))
	i.ensureNamespace(pathStr)
	return C.TCL_OK
}

//export goNsDelete
func goNsDelete(interp C.FeatherInterp, path C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(FeatherObj(path))

	// Cannot delete global namespace
	if pathStr == "::" {
		return C.TCL_ERROR
	}

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return C.TCL_ERROR
	}

	// Delete all children recursively
	var deleteRecursive func(n *Namespace)
	deleteRecursive = func(n *Namespace) {
		for _, child := range n.children {
			deleteRecursive(child)
		}
		delete(i.namespaces, n.fullPath)
	}
	deleteRecursive(ns)

	// Remove from parent's children
	if ns.parent != nil {
		for name, child := range ns.parent.children {
			if child == ns {
				delete(ns.parent.children, name)
				break
			}
		}
	}

	return C.TCL_OK
}

//export goNsExists
func goNsExists(interp C.FeatherInterp, path C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(path))
	if _, ok := i.namespaces[pathStr]; ok {
		return 1
	}
	return 0
}

//export goNsCurrent
func goNsCurrent(interp C.FeatherInterp) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	frame := i.frames[i.active]
	if frame.ns != nil {
		return C.FeatherObj(i.internString(frame.ns.fullPath))
	}
	return C.FeatherObj(i.internString("::"))
}

//export goNsParent
func goNsParent(interp C.FeatherInterp, nsPath C.FeatherObj, result *C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(FeatherObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return C.TCL_ERROR
	}

	if ns.parent == nil {
		// Global namespace has no parent - return empty string
		*result = C.FeatherObj(i.internString(""))
	} else {
		*result = C.FeatherObj(i.internString(ns.parent.fullPath))
	}
	return C.TCL_OK
}

//export goNsChildren
func goNsChildren(interp C.FeatherInterp, nsPath C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
		return C.FeatherObj(id)
	}

	// Collect and sort child names for consistent ordering
	names := make([]string, 0, len(ns.children))
	for name := range ns.children {
		names = append(names, name)
	}
	sort.Strings(names)

	// Build list of full paths
	items := make([]FeatherObj, len(names))
	for idx, name := range names {
		child := ns.children[name]
		items[idx] = i.internString(child.fullPath)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goNsGetVar
func goNsGetVar(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return 0
	}
	if val, ok := ns.vars[nameStr]; ok {
		return C.FeatherObj(val)
	}
	return 0
}

//export goNsSetVar
func goNsSetVar(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj, value C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	// Create namespace if needed
	ns := i.ensureNamespace(pathStr)
	ns.vars[nameStr] = FeatherObj(value)
}

//export goNsVarExists
func goNsVarExists(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return 0
	}
	if _, ok := ns.vars[nameStr]; ok {
		return 1
	}
	return 0
}

//export goNsUnsetVar
func goNsUnsetVar(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return
	}
	delete(ns.vars, nameStr)
}

//export goNsGetCommand
func goNsGetCommand(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj, fn *C.FeatherBuiltinCmd) C.FeatherCommandType {
	i := getInterp(interp)
	if i == nil {
		*fn = nil
		return C.TCL_CMD_NONE
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		*fn = nil
		return C.TCL_CMD_NONE
	}

	cmd, ok := ns.commands[nameStr]
	if !ok {
		*fn = nil
		return C.TCL_CMD_NONE
	}

	switch cmd.cmdType {
	case CmdBuiltin:
		*fn = cmd.builtin
		return C.TCL_CMD_BUILTIN
	case CmdProc:
		*fn = nil
		return C.TCL_CMD_PROC
	default:
		*fn = nil
		return C.TCL_CMD_NONE
	}
}

//export goNsSetCommand
func goNsSetCommand(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj,
	kind C.FeatherCommandType, fn C.FeatherBuiltinCmd,
	params C.FeatherObj, body C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	// Ensure namespace exists
	ns := i.ensureNamespace(pathStr)

	cmd := &Command{
		cmdType: CommandType(kind),
	}
	if kind == C.TCL_CMD_BUILTIN {
		cmd.builtin = fn
	} else if kind == C.TCL_CMD_PROC {
		cmd.proc = &Procedure{
			name:   FeatherObj(name),
			params: FeatherObj(params),
			body:   FeatherObj(body),
		}
	}
	ns.commands[nameStr] = cmd
}

//export goNsDeleteCommand
func goNsDeleteCommand(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return C.TCL_ERROR
	}

	if _, ok := ns.commands[nameStr]; !ok {
		return C.TCL_ERROR
	}

	delete(ns.commands, nameStr)
	return C.TCL_OK
}

//export goNsListCommands
func goNsListCommands(interp C.FeatherInterp, nsPath C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
		return C.FeatherObj(id)
	}

	names := make([]string, 0, len(ns.commands))
	for name := range ns.commands {
		names = append(names, name)
	}
	sort.Strings(names)

	items := make([]FeatherObj, len(names))
	for idx, name := range names {
		items[idx] = i.internString(name)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goFrameSetNamespace
func goFrameSetNamespace(interp C.FeatherInterp, nsPath C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	pathStr := i.GetString(FeatherObj(nsPath))

	// Create namespace if needed
	ns := i.ensureNamespace(pathStr)
	i.frames[i.active].ns = ns
	return C.TCL_OK
}

//export goFrameGetNamespace
func goFrameGetNamespace(interp C.FeatherInterp) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	frame := i.frames[i.active]
	if frame.ns != nil {
		return C.FeatherObj(i.internString(frame.ns.fullPath))
	}
	return C.FeatherObj(i.internString("::"))
}

//export goVarLinkNs
func goVarLinkNs(interp C.FeatherInterp, local C.FeatherObj, nsPath C.FeatherObj, name C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	localStr := i.GetString(FeatherObj(local))
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	frame := i.frames[i.active]
	frame.links[localStr] = varLink{
		targetLevel: -1, // -1 indicates namespace link
		nsPath:      pathStr,
		nsName:      nameStr,
	}
}

//export goInterpGetScript
func goInterpGetScript(interp C.FeatherInterp) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	if i.scriptPath == 0 {
		// Return empty string if no script path set
		return C.FeatherObj(i.internString(""))
	}
	return C.FeatherObj(i.scriptPath)
}

//export goInterpSetScript
func goInterpSetScript(interp C.FeatherInterp, path C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	i.scriptPath = FeatherObj(path)
}

//export goVarNames
func goVarNames(interp C.FeatherInterp, ns C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}

	var names []string

	if ns == 0 {
		// Return variables in current frame (locals)
		frame := i.frames[i.active]
		for name := range frame.locals.vars {
			names = append(names, name)
		}
		// Also include linked variables (upvar, variable)
		for name := range frame.links {
			// Only include if not already in locals (avoid duplicates)
			found := false
			for _, n := range names {
				if n == name {
					found = true
					break
				}
			}
			if !found {
				names = append(names, name)
			}
		}
	} else {
		// Return variables in the specified namespace
		pathStr := i.GetString(FeatherObj(ns))
		if nsObj, ok := i.namespaces[pathStr]; ok {
			for name := range nsObj.vars {
				names = append(names, name)
			}
		}
	}

	// Sort for consistent ordering
	sort.Strings(names)

	// Create list of names
	items := make([]FeatherObj, len(names))
	for idx, name := range names {
		items[idx] = i.internString(name)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goTraceAdd
func goTraceAdd(interp C.FeatherInterp, kind C.FeatherObj, name C.FeatherObj, ops C.FeatherObj, script C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	kindStr := i.GetString(FeatherObj(kind))
	nameStr := i.GetString(FeatherObj(name))
	opsStr := i.GetString(FeatherObj(ops))


	entry := TraceEntry{
		ops:    opsStr,
		script: FeatherObj(script),
	}

	if kindStr == "variable" {
		i.varTraces[nameStr] = append(i.varTraces[nameStr], entry)
	} else if kindStr == "command" {
		i.cmdTraces[nameStr] = append(i.cmdTraces[nameStr], entry)
	} else {
		return C.TCL_ERROR
	}

	return C.TCL_OK
}

//export goTraceRemove
func goTraceRemove(interp C.FeatherInterp, kind C.FeatherObj, name C.FeatherObj, ops C.FeatherObj, script C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	kindStr := i.GetString(FeatherObj(kind))
	nameStr := i.GetString(FeatherObj(name))
	opsStr := i.GetString(FeatherObj(ops))
	scriptStr := i.GetString(FeatherObj(script))

	var traces *map[string][]TraceEntry
	if kindStr == "variable" {
		traces = &i.varTraces
	} else if kindStr == "command" {
		traces = &i.cmdTraces
	} else {
		return C.TCL_ERROR
	}

	// Find and remove matching trace - compare by string value, not handle
	entries := (*traces)[nameStr]
	for idx, entry := range entries {
		entryScriptStr := i.GetString(entry.script)
		if entry.ops == opsStr && entryScriptStr == scriptStr {
			// Remove this entry
			(*traces)[nameStr] = append(entries[:idx], entries[idx+1:]...)
			return C.TCL_OK
		}
	}

	// No matching trace found
	return C.TCL_ERROR
}

//export goTraceInfo
func goTraceInfo(interp C.FeatherInterp, kind C.FeatherObj, name C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	kindStr := i.GetString(FeatherObj(kind))
	nameStr := i.GetString(FeatherObj(name))

	var entries []TraceEntry
	if kindStr == "variable" {
		entries = i.varTraces[nameStr]
	} else if kindStr == "command" {
		entries = i.cmdTraces[nameStr]
	}

	// Build list of {ops... script} sublists
	// Format: each trace is {op1 op2 ... script} where ops are individual elements
	items := make([]FeatherObj, 0, len(entries))
	for _, entry := range entries {
		// Split ops into individual elements
		ops := strings.Fields(entry.ops)
		subItems := make([]FeatherObj, 0, len(ops)+1)
		for _, op := range ops {
			subItems = append(subItems, i.internString(op))
		}
		// Add the script at the end
		subItems = append(subItems, entry.script)

		subId := i.nextID
		i.nextID++
		i.objects[subId] = &Object{isList: true, listItems: subItems}
		items = append(items, subId)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goNsGetExports
func goNsGetExports(interp C.FeatherInterp, nsPath C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(nsPath))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		// Return empty list
		id := i.nextID
		i.nextID++
		i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
		return C.FeatherObj(id)
	}

	// Return export patterns as a list
	items := make([]FeatherObj, len(ns.exportPatterns))
	for idx, pattern := range ns.exportPatterns {
		items[idx] = i.internString(pattern)
	}

	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: items}
	return C.FeatherObj(id)
}

//export goNsSetExports
func goNsSetExports(interp C.FeatherInterp, nsPath C.FeatherObj, patterns C.FeatherObj, clear C.int) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	pathStr := i.GetString(FeatherObj(nsPath))

	ns := i.ensureNamespace(pathStr)

	// Get patterns from list
	patternList, err := i.GetList(FeatherObj(patterns))
	if err != nil {
		return
	}
	newPatterns := make([]string, len(patternList))
	for idx, p := range patternList {
		newPatterns[idx] = i.GetString(p)
	}

	if clear != 0 {
		// Replace existing patterns
		ns.exportPatterns = newPatterns
	} else {
		// Append to existing patterns
		ns.exportPatterns = append(ns.exportPatterns, newPatterns...)
	}
}

//export goNsIsExported
func goNsIsExported(interp C.FeatherInterp, nsPath C.FeatherObj, name C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	pathStr := i.GetString(FeatherObj(nsPath))
	nameStr := i.GetString(FeatherObj(name))

	ns, ok := i.namespaces[pathStr]
	if !ok {
		return 0
	}

	// Check if name matches any export pattern
	for _, pattern := range ns.exportPatterns {
		if globMatch(pattern, nameStr) {
			return 1
		}
	}
	return 0
}

// globMatch performs simple glob pattern matching
// Supports * for any sequence and ? for single character
func globMatch(pattern, str string) bool {
	return globMatchHelper(pattern, str, 0, 0)
}

func globMatchHelper(pattern, str string, pi, si int) bool {
	for pi < len(pattern) {
		if pattern[pi] == '*' {
			// Skip consecutive *
			for pi < len(pattern) && pattern[pi] == '*' {
				pi++
			}
			if pi >= len(pattern) {
				return true // * at end matches everything
			}
			// Try matching * with 0, 1, 2, ... characters
			for si <= len(str) {
				if globMatchHelper(pattern, str, pi, si) {
					return true
				}
				si++
			}
			return false
		} else if si >= len(str) {
			return false // pattern has chars but string is empty
		} else if pattern[pi] == '?' || pattern[pi] == str[si] {
			pi++
			si++
		} else {
			return false
		}
	}
	return si >= len(str)
}

//export goNsCopyCommand
func goNsCopyCommand(interp C.FeatherInterp, srcNs C.FeatherObj, srcName C.FeatherObj,
	dstNs C.FeatherObj, dstName C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	srcNsStr := i.GetString(FeatherObj(srcNs))
	srcNameStr := i.GetString(FeatherObj(srcName))
	dstNsStr := i.GetString(FeatherObj(dstNs))
	dstNameStr := i.GetString(FeatherObj(dstName))

	// Find source namespace
	srcNsObj, ok := i.namespaces[srcNsStr]
	if !ok {
		return C.TCL_ERROR
	}

	// Find source command
	cmd, ok := srcNsObj.commands[srcNameStr]
	if !ok {
		return C.TCL_ERROR
	}

	// Ensure destination namespace exists
	dstNsObj := i.ensureNamespace(dstNsStr)

	// Copy command to destination (it's a pointer copy, so both share the same Command)
	dstNsObj.commands[dstNameStr] = cmd

	return C.TCL_OK
}

// ============================================================================
// Foreign Object Operations
// ============================================================================

//export goForeignIsForeign
func goForeignIsForeign(interp C.FeatherInterp, obj C.FeatherObj) C.int {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	if i.IsForeign(FeatherObj(obj)) {
		return 1
	}
	return 0
}

//export goForeignTypeName
func goForeignTypeName(interp C.FeatherInterp, obj C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	typeName := i.GetForeignType(FeatherObj(obj))
	if typeName == "" {
		return 0
	}
	return C.FeatherObj(i.internString(typeName))
}

//export goForeignStringRep
func goForeignStringRep(interp C.FeatherInterp, obj C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	o := i.getObject(FeatherObj(obj))
	if o == nil || !o.isForeign {
		return 0
	}
	// Return the cached string representation
	return C.FeatherObj(i.internString(o.stringVal))
}

//export goForeignMethods
func goForeignMethods(interp C.FeatherInterp, obj C.FeatherObj) C.FeatherObj {
	i := getInterp(interp)
	if i == nil {
		return 0
	}
	o := i.getObject(FeatherObj(obj))
	if o == nil {
		return 0
	}

	// Determine the foreign type name
	var typeName string
	if o.isForeign {
		typeName = o.foreignType
	} else if i.ForeignRegistry != nil {
		// Check if string value is a foreign handle name
		i.ForeignRegistry.mu.RLock()
		if instance, ok := i.ForeignRegistry.instances[o.stringVal]; ok {
			typeName = instance.typeName
		}
		i.ForeignRegistry.mu.RUnlock()
	}

	if typeName == "" {
		return 0
	}

	// Use the high-level registry if available
	var methods []string
	if i.ForeignRegistry != nil {
		i.ForeignRegistry.mu.RLock()
		if info, ok := i.ForeignRegistry.types[typeName]; ok {
			for name := range info.methods {
				methods = append(methods, name)
			}
			methods = append(methods, "destroy")
		}
		i.ForeignRegistry.mu.RUnlock()
	}
	// Build a list of method names
	id := i.nextID
	i.nextID++
	methodHandles := make([]FeatherObj, len(methods))
	for j, m := range methods {
		methodHandles[j] = i.internString(m)
	}
	i.objects[id] = &Object{isList: true, listItems: methodHandles}
	return C.FeatherObj(id)
}

//export goForeignInvoke
func goForeignInvoke(interp C.FeatherInterp, obj C.FeatherObj, method C.FeatherObj, args C.FeatherObj) C.FeatherResult {
	i := getInterp(interp)
	if i == nil {
		return C.TCL_ERROR
	}
	o := i.getObject(FeatherObj(obj))
	if o == nil || !o.isForeign {
		i.SetResult(i.internString("not a foreign object"))
		return C.TCL_ERROR
	}
	// For now, method invocation is not implemented at the low level.
	// The high-level library will handle method dispatch via the type registry.
	methodStr := i.GetString(FeatherObj(method))
	i.SetResult(i.internString("method invocation not implemented: " + methodStr))
	return C.TCL_ERROR
}

//export goForeignDestroy
func goForeignDestroy(interp C.FeatherInterp, obj C.FeatherObj) {
	i := getInterp(interp)
	if i == nil {
		return
	}
	o := i.getObject(FeatherObj(obj))
	if o == nil || !o.isForeign {
		return
	}
	// Clear the foreign value to allow GC
	o.foreignValue = nil
	o.isForeign = false
	o.foreignType = ""
}
