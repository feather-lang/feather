package feather

import (
	"fmt"
	"reflect"
	"strings"

	"github.com/feather-lang/feather/interp"
)

// toTclString converts a Go value to a TCL string representation.
func toTclString(v any) string {
	if v == nil {
		return "{}"
	}

	switch val := v.(type) {
	case string:
		return quote(val)
	case int:
		return fmt.Sprintf("%d", val)
	case int64:
		return fmt.Sprintf("%d", val)
	case float64:
		return fmt.Sprintf("%g", val)
	case bool:
		if val {
			return "1"
		}
		return "0"
	case []string:
		parts := make([]string, len(val))
		for i, s := range val {
			parts[i] = quote(s)
		}
		return strings.Join(parts, " ")
	case Object:
		return quote(val.String())
	default:
		// Use reflection for other types
		rv := reflect.ValueOf(v)
		switch rv.Kind() {
		case reflect.Slice, reflect.Array:
			parts := make([]string, rv.Len())
			for i := 0; i < rv.Len(); i++ {
				parts[i] = toTclString(rv.Index(i).Interface())
			}
			return strings.Join(parts, " ")
		case reflect.Map:
			var parts []string
			iter := rv.MapRange()
			for iter.Next() {
				parts = append(parts, toTclString(iter.Key().Interface()))
				parts = append(parts, toTclString(iter.Value().Interface()))
			}
			return strings.Join(parts, " ")
		default:
			return quote(fmt.Sprintf("%v", v))
		}
	}
}

// quote adds braces around a string if it contains special characters.
func quote(s string) string {
	if s == "" {
		return "{}"
	}
	needsQuote := false
	for _, c := range s {
		if c == ' ' || c == '\t' || c == '\n' || c == '{' || c == '}' || c == '"' || c == '\\' || c == '$' || c == '[' || c == ']' {
			needsQuote = true
			break
		}
	}
	if needsQuote {
		return "{" + s + "}"
	}
	return s
}

// wrapFunc wraps a Go function to be callable from TCL.
func wrapFunc(i *Interp, fn any) interp.CommandFunc {
	fnVal := reflect.ValueOf(fn)
	fnType := fnVal.Type()

	if fnType.Kind() != reflect.Func {
		panic(fmt.Sprintf("Register: expected function, got %T", fn))
	}

	return func(ip *interp.Interp, cmd interp.FeatherObj, args []interp.FeatherObj) interp.FeatherResult {
		numIn := fnType.NumIn()
		isVariadic := fnType.IsVariadic()

		// Check argument count
		if isVariadic {
			if len(args) < numIn-1 {
				ip.SetErrorString(fmt.Sprintf("wrong # args: expected at least %d, got %d", numIn-1, len(args)))
				return interp.ResultError
			}
		} else {
			if len(args) != numIn {
				ip.SetErrorString(fmt.Sprintf("wrong # args: expected %d, got %d", numIn, len(args)))
				return interp.ResultError
			}
		}

		// Convert arguments
		callArgs := make([]reflect.Value, len(args))
		for j := 0; j < len(args); j++ {
			var paramType reflect.Type
			if isVariadic && j >= numIn-1 {
				paramType = fnType.In(numIn - 1).Elem()
			} else {
				paramType = fnType.In(j)
			}

			converted, err := convertArg(ip, args[j], paramType)
			if err != nil {
				ip.SetErrorString(fmt.Sprintf("argument %d: %v", j+1, err))
				return interp.ResultError
			}
			callArgs[j] = converted
		}

		// Call function
		results := fnVal.Call(callArgs)

		// Process results
		return processResults(ip, results, fnType)
	}
}

// convertArg converts a TCL value to a Go value of the specified type.
func convertArg(i *interp.Interp, arg interp.FeatherObj, targetType reflect.Type) (reflect.Value, error) {
	switch targetType.Kind() {
	case reflect.String:
		return reflect.ValueOf(i.GetString(arg)), nil

	case reflect.Int:
		v, err := i.GetInt(arg)
		if err != nil {
			return reflect.Value{}, err
		}
		return reflect.ValueOf(int(v)), nil

	case reflect.Int64:
		v, err := i.GetInt(arg)
		if err != nil {
			return reflect.Value{}, err
		}
		return reflect.ValueOf(v), nil

	case reflect.Float64:
		v, err := i.GetDouble(arg)
		if err != nil {
			return reflect.Value{}, err
		}
		return reflect.ValueOf(v), nil

	case reflect.Bool:
		s := i.GetString(arg)
		switch strings.ToLower(s) {
		case "1", "true", "yes", "on":
			return reflect.ValueOf(true), nil
		case "0", "false", "no", "off":
			return reflect.ValueOf(false), nil
		default:
			return reflect.Value{}, fmt.Errorf("expected boolean but got %q", s)
		}

	case reflect.Slice:
		if targetType.Elem().Kind() == reflect.String {
			// Special case: []string
			items, err := i.GetList(arg)
			if err != nil {
				return reflect.Value{}, err
			}
			slice := make([]string, len(items))
			for j, item := range items {
				slice[j] = i.GetString(item)
			}
			return reflect.ValueOf(slice), nil
		}
		// Generic slice handling
		items, err := i.GetList(arg)
		if err != nil {
			return reflect.Value{}, err
		}
		slice := reflect.MakeSlice(targetType, len(items), len(items))
		for j, item := range items {
			converted, err := convertArg(i, item, targetType.Elem())
			if err != nil {
				return reflect.Value{}, fmt.Errorf("element %d: %v", j, err)
			}
			slice.Index(j).Set(converted)
		}
		return slice, nil

	case reflect.Interface:
		// For any/interface{}, return the string value
		if targetType.NumMethod() == 0 {
			return reflect.ValueOf(i.GetString(arg)), nil
		}
		return reflect.Value{}, fmt.Errorf("cannot convert to interface %v", targetType)

	default:
		return reflect.Value{}, fmt.Errorf("unsupported parameter type: %v", targetType)
	}
}

// processResults handles the return values from a function call.
func processResults(i *interp.Interp, results []reflect.Value, fnType reflect.Type) interp.FeatherResult {
	if len(results) == 0 {
		i.SetResultString("")
		return interp.ResultOK
	}

	// Check for error return (last result is error)
	lastResult := results[len(results)-1]
	if fnType.NumOut() > 0 && fnType.Out(fnType.NumOut()-1).Implements(reflect.TypeOf((*error)(nil)).Elem()) {
		if !lastResult.IsNil() {
			err := lastResult.Interface().(error)
			i.SetErrorString(err.Error())
			return interp.ResultError
		}
		results = results[:len(results)-1]
	}

	// Convert result to TCL
	if len(results) == 0 {
		i.SetResultString("")
		return interp.ResultOK
	}

	result := results[0]
	return convertResult(i, result)
}

// convertResult converts a Go value to a TCL result.
func convertResult(i *interp.Interp, result reflect.Value) interp.FeatherResult {
	if !result.IsValid() {
		i.SetResultString("")
		return interp.ResultOK
	}

	switch result.Kind() {
	case reflect.String:
		i.SetResultString(result.String())

	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		i.SetResultString(fmt.Sprintf("%d", result.Int()))

	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		i.SetResultString(fmt.Sprintf("%d", result.Uint()))

	case reflect.Float32, reflect.Float64:
		i.SetResultString(fmt.Sprintf("%g", result.Float()))

	case reflect.Bool:
		if result.Bool() {
			i.SetResultString("1")
		} else {
			i.SetResultString("0")
		}

	case reflect.Slice:
		// Convert slice to list
		list := i.NewList()
		for j := 0; j < result.Len(); j++ {
			elem := result.Index(j)
			var elemHandle interp.FeatherObj
			switch elem.Kind() {
			case reflect.String:
				elemHandle = i.InternString(elem.String())
			case reflect.Int, reflect.Int64:
				elemHandle = i.NewInt(elem.Int())
			default:
				elemHandle = i.InternString(fmt.Sprintf("%v", elem.Interface()))
			}
			list = i.ListAppend(list, elemHandle)
		}
		i.SetResult(list)

	case reflect.Map:
		// Convert map to dict
		dict := i.NewDict()
		iter := result.MapRange()
		for iter.Next() {
			key := fmt.Sprintf("%v", iter.Key().Interface())
			val := iter.Value()
			var valHandle interp.FeatherObj
			switch val.Kind() {
			case reflect.String:
				valHandle = i.InternString(val.String())
			case reflect.Int, reflect.Int64:
				valHandle = i.NewInt(val.Int())
			default:
				valHandle = i.InternString(fmt.Sprintf("%v", val.Interface()))
			}
			dict = i.DictSet(dict, key, valHandle)
		}
		i.SetResult(dict)

	case reflect.Ptr, reflect.Interface:
		if result.IsNil() {
			i.SetResultString("")
			return interp.ResultOK
		}
		i.SetResultString(fmt.Sprintf("%v", result.Interface()))

	default:
		i.SetResultString(fmt.Sprintf("%v", result.Interface()))
	}

	return interp.ResultOK
}
