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
