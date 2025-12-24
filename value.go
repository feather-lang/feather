package feather

import (
	"fmt"
	"strconv"
)

// Value represents a TCL value with type-safe accessors.
// TCL values use "shimmering" - lazy conversion between representations.
type Value interface {
	// String returns the string representation of the value.
	String() string

	// Int returns the integer representation of the value.
	// Returns an error if the value cannot be converted to an integer.
	Int() (int64, error)

	// Float returns the floating-point representation of the value.
	// Returns an error if the value cannot be converted to a float.
	Float() (float64, error)

	// Bool returns the boolean representation of the value.
	// TCL truthy: "1", "true", "yes", "on" (case-insensitive)
	// TCL falsy: "0", "false", "no", "off" (case-insensitive)
	Bool() (bool, error)

	// List returns the list representation of the value.
	// Returns an error if the value cannot be parsed as a TCL list.
	List() ([]Value, error)

	// Dict returns the dict representation of the value.
	// Returns an error if the value cannot be converted to a dict
	// (e.g., odd number of elements).
	Dict() (map[string]Value, error)

	// Type returns the native type: "string", "int", "double", "list", "dict",
	// or a foreign type name.
	Type() string

	// IsNil returns true if this is a nil/empty value.
	IsNil() bool
}

// stringValue is the simplest Value implementation - just a string.
// This is returned by Eval and most operations.
type stringValue string

func (v stringValue) String() string {
	return string(v)
}

func (v stringValue) Int() (int64, error) {
	return strconv.ParseInt(string(v), 10, 64)
}

func (v stringValue) Float() (float64, error) {
	return strconv.ParseFloat(string(v), 64)
}

func (v stringValue) Bool() (bool, error) {
	s := string(v)
	switch s {
	case "1", "true", "True", "TRUE", "yes", "Yes", "YES", "on", "On", "ON":
		return true, nil
	case "0", "false", "False", "FALSE", "no", "No", "NO", "off", "Off", "OFF":
		return false, nil
	default:
		return false, fmt.Errorf("expected boolean but got %q", s)
	}
}

func (v stringValue) List() ([]Value, error) {
	items, err := parseList(string(v))
	if err != nil {
		return nil, err
	}
	result := make([]Value, len(items))
	for i, item := range items {
		result[i] = stringValue(item)
	}
	return result, nil
}

func (v stringValue) Dict() (map[string]Value, error) {
	items, err := parseList(string(v))
	if err != nil {
		return nil, err
	}
	if len(items)%2 != 0 {
		return nil, fmt.Errorf("missing value to go with key")
	}
	result := make(map[string]Value, len(items)/2)
	for i := 0; i < len(items); i += 2 {
		result[items[i]] = stringValue(items[i+1])
	}
	return result, nil
}

func (v stringValue) Type() string {
	return "string"
}

func (v stringValue) IsNil() bool {
	return string(v) == ""
}

// intValue stores an integer natively.
type intValue int64

func (v intValue) String() string {
	return strconv.FormatInt(int64(v), 10)
}

func (v intValue) Int() (int64, error) {
	return int64(v), nil
}

func (v intValue) Float() (float64, error) {
	return float64(v), nil
}

func (v intValue) Bool() (bool, error) {
	if v == 0 {
		return false, nil
	}
	if v == 1 {
		return true, nil
	}
	return false, fmt.Errorf("expected boolean but got %d", v)
}

func (v intValue) List() ([]Value, error) {
	return []Value{v}, nil
}

func (v intValue) Dict() (map[string]Value, error) {
	return nil, fmt.Errorf("missing value to go with key")
}

func (v intValue) Type() string {
	return "int"
}

func (v intValue) IsNil() bool {
	return false
}

// floatValue stores a floating-point number natively.
type floatValue float64

func (v floatValue) String() string {
	return strconv.FormatFloat(float64(v), 'g', -1, 64)
}

func (v floatValue) Int() (int64, error) {
	return int64(v), nil
}

func (v floatValue) Float() (float64, error) {
	return float64(v), nil
}

func (v floatValue) Bool() (bool, error) {
	if v == 0 {
		return false, nil
	}
	if v == 1 {
		return true, nil
	}
	return false, fmt.Errorf("expected boolean but got %g", v)
}

func (v floatValue) List() ([]Value, error) {
	return []Value{v}, nil
}

func (v floatValue) Dict() (map[string]Value, error) {
	return nil, fmt.Errorf("missing value to go with key")
}

func (v floatValue) Type() string {
	return "double"
}

func (v floatValue) IsNil() bool {
	return false
}

// listValue stores a list natively, avoiding re-parsing.
type listValue []Value

func (v listValue) String() string {
	if len(v) == 0 {
		return ""
	}
	var result string
	for i, item := range v {
		if i > 0 {
			result += " "
		}
		s := item.String()
		if needsBraces(s) {
			result += "{" + s + "}"
		} else {
			result += s
		}
	}
	return result
}

func (v listValue) Int() (int64, error) {
	if len(v) == 1 {
		return v[0].Int()
	}
	return 0, fmt.Errorf("expected integer but got list")
}

func (v listValue) Float() (float64, error) {
	if len(v) == 1 {
		return v[0].Float()
	}
	return 0, fmt.Errorf("expected float but got list")
}

func (v listValue) Bool() (bool, error) {
	if len(v) == 1 {
		return v[0].Bool()
	}
	return false, fmt.Errorf("expected boolean but got list")
}

func (v listValue) List() ([]Value, error) {
	return v, nil
}

func (v listValue) Dict() (map[string]Value, error) {
	if len(v)%2 != 0 {
		return nil, fmt.Errorf("missing value to go with key")
	}
	result := make(map[string]Value, len(v)/2)
	for i := 0; i < len(v); i += 2 {
		result[v[i].String()] = v[i+1]
	}
	return result, nil
}

func (v listValue) Type() string {
	return "list"
}

func (v listValue) IsNil() bool {
	return len(v) == 0
}

// dictValue stores a dict natively, preserving key order.
type dictValue struct {
	keys   []string
	values map[string]Value
}

func (v dictValue) String() string {
	if len(v.keys) == 0 {
		return ""
	}
	var result string
	for i, key := range v.keys {
		if i > 0 {
			result += " "
		}
		if needsBraces(key) {
			result += "{" + key + "}"
		} else {
			result += key
		}
		result += " "
		val := v.values[key].String()
		if needsBraces(val) {
			result += "{" + val + "}"
		} else {
			result += val
		}
	}
	return result
}

func (v dictValue) Int() (int64, error) {
	return 0, fmt.Errorf("expected integer but got dict")
}

func (v dictValue) Float() (float64, error) {
	return 0, fmt.Errorf("expected float but got dict")
}

func (v dictValue) Bool() (bool, error) {
	return false, fmt.Errorf("expected boolean but got dict")
}

func (v dictValue) List() ([]Value, error) {
	result := make([]Value, 0, len(v.keys)*2)
	for _, key := range v.keys {
		result = append(result, stringValue(key), v.values[key])
	}
	return result, nil
}

func (v dictValue) Dict() (map[string]Value, error) {
	return v.values, nil
}

func (v dictValue) Type() string {
	return "dict"
}

func (v dictValue) IsNil() bool {
	return len(v.keys) == 0
}

// foreignValue wraps a foreign object handle.
type foreignValue struct {
	typeName string
	handle   string
}

func (v foreignValue) String() string {
	return v.handle
}

func (v foreignValue) Int() (int64, error) {
	return 0, fmt.Errorf("expected integer but got %s", v.typeName)
}

func (v foreignValue) Float() (float64, error) {
	return 0, fmt.Errorf("expected float but got %s", v.typeName)
}

func (v foreignValue) Bool() (bool, error) {
	return false, fmt.Errorf("expected boolean but got %s", v.typeName)
}

func (v foreignValue) List() ([]Value, error) {
	return []Value{v}, nil
}

func (v foreignValue) Dict() (map[string]Value, error) {
	return nil, fmt.Errorf("missing value to go with key")
}

func (v foreignValue) Type() string {
	return v.typeName
}

func (v foreignValue) IsNil() bool {
	return false
}

// NewInt creates an integer Value.
func NewInt(v int64) Value {
	return intValue(v)
}

// NewFloat creates a floating-point Value.
func NewFloat(v float64) Value {
	return floatValue(v)
}

// NewString creates a string Value.
func NewString(s string) Value {
	return stringValue(s)
}

// NewList creates a list Value from the given items.
func NewList(items ...Value) Value {
	return listValue(items)
}

// NewDict creates a dict Value from the given key-value pairs.
// Keys and values alternate: key1, val1, key2, val2, ...
func NewDict(pairs ...Value) Value {
	if len(pairs)%2 != 0 {
		return dictValue{keys: nil, values: nil}
	}
	keys := make([]string, 0, len(pairs)/2)
	values := make(map[string]Value, len(pairs)/2)
	for i := 0; i < len(pairs); i += 2 {
		key := pairs[i].String()
		keys = append(keys, key)
		values[key] = pairs[i+1]
	}
	return dictValue{keys: keys, values: values}
}

// NewForeign creates a foreign object Value.
func NewForeign(typeName, handle string) Value {
	return foreignValue{typeName: typeName, handle: handle}
}

// needsBraces returns true if the string needs braces in a list representation.
func needsBraces(s string) bool {
	if s == "" {
		return true
	}
	for _, c := range s {
		if c == ' ' || c == '\t' || c == '\n' || c == '{' || c == '}' || c == '"' {
			return true
		}
	}
	return false
}

// parseList parses a TCL list string into a slice of strings.
func parseList(s string) ([]string, error) {
	var items []string
	pos := 0

	for pos < len(s) {
		// Skip whitespace
		for pos < len(s) && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n') {
			pos++
		}
		if pos >= len(s) {
			break
		}

		var elem string
		if s[pos] == '{' {
			// Braced element
			depth := 1
			start := pos + 1
			pos++
			for pos < len(s) && depth > 0 {
				if s[pos] == '{' {
					depth++
				} else if s[pos] == '}' {
					depth--
				}
				pos++
			}
			if depth != 0 {
				return nil, fmt.Errorf("unmatched brace in list")
			}
			elem = s[start : pos-1]
		} else if s[pos] == '"' {
			// Quoted element
			start := pos + 1
			pos++
			for pos < len(s) && s[pos] != '"' {
				if s[pos] == '\\' && pos+1 < len(s) {
					pos++
				}
				pos++
			}
			if pos >= len(s) {
				return nil, fmt.Errorf("unmatched quote in list")
			}
			elem = s[start:pos]
			pos++ // skip closing quote
		} else {
			// Bare word
			start := pos
			for pos < len(s) && s[pos] != ' ' && s[pos] != '\t' && s[pos] != '\n' {
				pos++
			}
			elem = s[start:pos]
		}
		items = append(items, elem)
	}
	return items, nil
}
