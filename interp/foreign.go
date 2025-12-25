package interp

import (
	"fmt"
	"reflect"
	"strings"
	"sync"
)

// Methods is a map of method name to method implementation.
// Method functions should have the signature:
//
//	func(receiver T, args...) [result] [error]
//
// Where T is the foreign type being wrapped.
type Methods map[string]any

// ForeignTypeDef defines a foreign type that can be exposed to TCL.
// Use DefineType to register a type with the host.
type ForeignTypeDef[T any] struct {
	// New is the constructor function. Called when "TypeName new" is evaluated.
	// Should return a new instance of T.
	New func() T

	// Methods maps method names to implementations.
	// Each method should be a function where the first argument is the receiver (T).
	Methods Methods

	// StringRep optionally provides a custom string representation.
	// If nil, a default "<TypeName:id>" format is used.
	StringRep func(T) string

	// Destroy is called when the object is destroyed (command deleted).
	// Use for cleanup (close connections, release resources).
	Destroy func(T)
}

// foreignTypeInfo stores runtime information about a registered foreign type.
type foreignTypeInfo struct {
	name       string
	newFunc    reflect.Value        // constructor function
	methods    map[string]reflect.Value // method name -> function
	stringRep  reflect.Value        // optional string representation function
	destroy    reflect.Value        // optional destructor function
	receiverType reflect.Type       // type of the receiver (T)
}

// foreignInstance stores information about a live foreign object instance.
type foreignInstance struct {
	typeName   string
	handleName string     // e.g., "mux1"
	objHandle  FeatherObj     // the FeatherObj handle
	value      any        // the actual Go value
}

// ForeignRegistry manages foreign type definitions and object instances.
type ForeignRegistry struct {
	mu           sync.RWMutex
	types        map[string]*foreignTypeInfo    // type name -> type info
	instances    map[string]*foreignInstance    // handle name -> instance
	counters     map[string]int                 // type name -> next counter
	handleToType map[FeatherObj]*foreignInstance    // FeatherObj handle -> instance
}

// newForeignRegistry creates a new foreign registry.
func newForeignRegistry() *ForeignRegistry {
	return &ForeignRegistry{
		types:        make(map[string]*foreignTypeInfo),
		instances:    make(map[string]*foreignInstance),
		counters:     make(map[string]int),
		handleToType: make(map[FeatherObj]*foreignInstance),
	}
}

// DefineType registers a foreign type with the interpreter.
// After registration, the type name becomes a command that supports "new" to create instances.
//
// Example:
//
//	DefineType[*http.ServeMux](interp, "Mux", ForeignTypeDef[*http.ServeMux]{
//	    New: func() *http.ServeMux { return http.NewServeMux() },
//	    Methods: Methods{
//	        "handle": func(m *http.ServeMux, pattern string, handler string) { ... },
//	    },
//	})
func DefineType[T any](i *Interp, typeName string, def ForeignTypeDef[T]) error {
	if i.ForeignRegistry == nil {
		i.ForeignRegistry = newForeignRegistry()
	}

	i.ForeignRegistry.mu.Lock()
	defer i.ForeignRegistry.mu.Unlock()

	// Validate that New is provided
	if def.New == nil {
		return fmt.Errorf("DefineType: New function is required for type %s", typeName)
	}

	// Build the type info
	info := &foreignTypeInfo{
		name:       typeName,
		newFunc:    reflect.ValueOf(def.New),
		methods:    make(map[string]reflect.Value),
		receiverType: reflect.TypeOf((*T)(nil)).Elem(),
	}

	// Register methods
	for name, fn := range def.Methods {
		info.methods[name] = reflect.ValueOf(fn)
	}

	// Register optional callbacks
	if def.StringRep != nil {
		info.stringRep = reflect.ValueOf(def.StringRep)
	}
	if def.Destroy != nil {
		info.destroy = reflect.ValueOf(def.Destroy)
	}

	// Store the type info
	i.ForeignRegistry.types[typeName] = info
	i.ForeignRegistry.counters[typeName] = 1

	// Register the constructor command (e.g., "Mux")
	i.Register(typeName, func(interp *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
		return interp.foreignConstructor(typeName, cmd, args)
	})

	return nil
}

// foreignConstructor handles "TypeName new" and "TypeName subcommand" calls.
func (i *Interp) foreignConstructor(typeName string, cmd FeatherObj, args []FeatherObj) FeatherResult {
	if len(args) == 0 {
		i.SetErrorString(fmt.Sprintf("wrong # args: should be \"%s subcommand ?arg ...?\"", typeName))
		return ResultError
	}

	subCmd := i.GetString(args[0])
	if subCmd != "new" {
		i.SetErrorString(fmt.Sprintf("unknown subcommand \"%s\": must be new", subCmd))
		return ResultError
	}

	// Get type info
	i.ForeignRegistry.mu.RLock()
	info, ok := i.ForeignRegistry.types[typeName]
	i.ForeignRegistry.mu.RUnlock()
	if !ok {
		i.SetErrorString(fmt.Sprintf("unknown foreign type \"%s\"", typeName))
		return ResultError
	}

	// Call the constructor
	results := info.newFunc.Call(nil)
	if len(results) == 0 {
		i.SetErrorString(fmt.Sprintf("%s new: constructor returned no value", typeName))
		return ResultError
	}
	value := results[0].Interface()

	// Generate handle name (e.g., "mux1")
	i.ForeignRegistry.mu.Lock()
	counter := i.ForeignRegistry.counters[typeName]
	i.ForeignRegistry.counters[typeName] = counter + 1
	handleName := fmt.Sprintf("%s%d", strings.ToLower(typeName), counter)
	i.ForeignRegistry.mu.Unlock()

	// Create the foreign object
	objHandle := i.NewForeign(typeName, value)

	// Update string representation to be the handle name
	if obj := i.getObject(objHandle); obj != nil {
		obj.stringVal = handleName
	}

	// Store the instance
	instance := &foreignInstance{
		typeName:   typeName,
		handleName: handleName,
		objHandle:  objHandle,
		value:      value,
	}
	i.ForeignRegistry.mu.Lock()
	i.ForeignRegistry.instances[handleName] = instance
	i.ForeignRegistry.handleToType[objHandle] = instance
	i.ForeignRegistry.mu.Unlock()

	// Register the handle as a command (object-as-command pattern)
	i.Register(handleName, func(interp *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
		return interp.foreignMethodDispatch(handleName, cmd, args)
	})

	// Return the handle name
	i.SetResultString(handleName)
	return ResultOK
}

// foreignMethodDispatch handles method calls on foreign objects.
// Called when "$handle method args..." is evaluated.
func (i *Interp) foreignMethodDispatch(handleName string, cmd FeatherObj, args []FeatherObj) FeatherResult {
	if len(args) == 0 {
		i.SetErrorString(fmt.Sprintf("wrong # args: should be \"%s method ?arg ...?\"", handleName))
		return ResultError
	}

	methodName := i.GetString(args[0])
	methodArgs := args[1:]

	// Get the instance
	i.ForeignRegistry.mu.RLock()
	instance, ok := i.ForeignRegistry.instances[handleName]
	i.ForeignRegistry.mu.RUnlock()
	if !ok {
		i.SetErrorString(fmt.Sprintf("invalid object handle \"%s\"", handleName))
		return ResultError
	}

	// Handle built-in methods
	if methodName == "destroy" {
		return i.foreignDestroy(handleName)
	}

	// Get type info
	i.ForeignRegistry.mu.RLock()
	info, ok := i.ForeignRegistry.types[instance.typeName]
	i.ForeignRegistry.mu.RUnlock()
	if !ok {
		i.SetErrorString(fmt.Sprintf("unknown foreign type \"%s\"", instance.typeName))
		return ResultError
	}

	// Look up the method
	methodFunc, ok := info.methods[methodName]
	if !ok {
		// List available methods in error message
		var methodList []string
		for name := range info.methods {
			methodList = append(methodList, name)
		}
		methodList = append(methodList, "destroy")
		i.SetErrorString(fmt.Sprintf("unknown method \"%s\": must be %s", methodName, strings.Join(methodList, ", ")))
		return ResultError
	}

	// Call the method with argument conversion
	return i.callForeignMethod(instance.value, methodFunc, methodArgs)
}

// callForeignMethod calls a method with automatic argument conversion.
func (i *Interp) callForeignMethod(receiver any, methodFunc reflect.Value, args []FeatherObj) FeatherResult {
	methodType := methodFunc.Type()
	numParams := methodType.NumIn()

	// First parameter is the receiver
	if numParams < 1 {
		i.SetErrorString("method must have at least one parameter (receiver)")
		return ResultError
	}

	// Check argument count (excluding receiver)
	expectedArgs := numParams - 1
	if len(args) != expectedArgs {
		i.SetErrorString(fmt.Sprintf("wrong # args: expected %d, got %d", expectedArgs, len(args)))
		return ResultError
	}

	// Build argument list
	callArgs := make([]reflect.Value, numParams)
	callArgs[0] = reflect.ValueOf(receiver)

	// Convert each argument
	for j := 0; j < len(args); j++ {
		paramType := methodType.In(j + 1)
		converted, err := i.convertArg(args[j], paramType)
		if err != nil {
			i.SetErrorString(fmt.Sprintf("argument %d: %v", j+1, err))
			return ResultError
		}
		callArgs[j+1] = converted
	}

	// Call the method
	results := methodFunc.Call(callArgs)

	// Process results
	return i.processResults(results, methodType)
}

// convertArg converts a FeatherObj to a Go value of the specified type.
func (i *Interp) convertArg(arg FeatherObj, targetType reflect.Type) (reflect.Value, error) {
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
		// Convert list to slice
		items, err := i.GetList(arg)
		if err != nil {
			return reflect.Value{}, err
		}
		elemType := targetType.Elem()
		slice := reflect.MakeSlice(targetType, len(items), len(items))
		for j, item := range items {
			converted, err := i.convertArg(item, elemType)
			if err != nil {
				return reflect.Value{}, fmt.Errorf("element %d: %v", j, err)
			}
			slice.Index(j).Set(converted)
		}
		return slice, nil

	case reflect.Map:
		// Convert dict to map
		if targetType.Key().Kind() != reflect.String {
			return reflect.Value{}, fmt.Errorf("map key must be string")
		}
		dictItems, dictOrder, err := i.GetDict(arg)
		if err != nil {
			return reflect.Value{}, err
		}
		elemType := targetType.Elem()
		m := reflect.MakeMap(targetType)
		for _, key := range dictOrder {
			valHandle := dictItems[key]
			converted, err := i.convertArg(valHandle, elemType)
			if err != nil {
				return reflect.Value{}, fmt.Errorf("value for key %q: %v", key, err)
			}
			m.SetMapIndex(reflect.ValueOf(key), converted)
		}
		return m, nil

	case reflect.Interface:
		// For interface{}/any, return the FeatherObj handle wrapped
		if targetType.NumMethod() == 0 {
			// It's any/interface{}
			return reflect.ValueOf(arg), nil
		}
		// Check if it's a foreign object that implements the interface
		if i.ForeignRegistry != nil {
			i.ForeignRegistry.mu.RLock()
			instance, ok := i.ForeignRegistry.handleToType[arg]
			i.ForeignRegistry.mu.RUnlock()
			if ok {
				val := reflect.ValueOf(instance.value)
				if val.Type().Implements(targetType) {
					return val, nil
				}
			}
		}
		return reflect.Value{}, fmt.Errorf("cannot convert to interface %v", targetType)

	case reflect.Ptr:
		// Check if it's a foreign object
		handleName := i.GetString(arg)
		if i.ForeignRegistry != nil {
			i.ForeignRegistry.mu.RLock()
			instance, ok := i.ForeignRegistry.instances[handleName]
			i.ForeignRegistry.mu.RUnlock()
			if ok {
				val := reflect.ValueOf(instance.value)
				if val.Type().AssignableTo(targetType) {
					return val, nil
				}
				return reflect.Value{}, fmt.Errorf("foreign object is %T, not %v", instance.value, targetType)
			}
		}
		return reflect.Value{}, fmt.Errorf("unknown foreign object %q", handleName)

	default:
		return reflect.Value{}, fmt.Errorf("unsupported parameter type: %v", targetType)
	}
}

// processResults handles the return values from a method call.
func (i *Interp) processResults(results []reflect.Value, methodType reflect.Type) FeatherResult {
	if len(results) == 0 {
		i.SetResultString("")
		return ResultOK
	}

	// Check for error return (last result is error)
	lastResult := results[len(results)-1]
	if methodType.NumOut() > 0 && methodType.Out(methodType.NumOut()-1).Implements(reflect.TypeOf((*error)(nil)).Elem()) {
		if !lastResult.IsNil() {
			err := lastResult.Interface().(error)
			i.SetErrorString(err.Error())
			return ResultError
		}
		results = results[:len(results)-1]
	}

	// Convert result to TCL
	if len(results) == 0 {
		i.SetResultString("")
		return ResultOK
	}

	result := results[0]
	return i.convertResult(result)
}

// convertResult converts a Go value back to a FeatherObj result.
func (i *Interp) convertResult(result reflect.Value) FeatherResult {
	if !result.IsValid() {
		i.SetResultString("")
		return ResultOK
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
		list := i.createList()
		for j := 0; j < result.Len(); j++ {
			elem := result.Index(j)
			var elemHandle FeatherObj
			switch elem.Kind() {
			case reflect.String:
				elemHandle = i.internString(elem.String())
			case reflect.Int, reflect.Int64:
				elemHandle = i.createInt(elem.Int())
			default:
				elemHandle = i.internString(fmt.Sprintf("%v", elem.Interface()))
			}
			list = i.listPush(list, elemHandle)
		}
		i.SetResult(list)

	case reflect.Map:
		// Convert map to dict
		dict := i.createDict()
		iter := result.MapRange()
		for iter.Next() {
			key := fmt.Sprintf("%v", iter.Key().Interface())
			val := iter.Value()
			var valHandle FeatherObj
			switch val.Kind() {
			case reflect.String:
				valHandle = i.internString(val.String())
			case reflect.Int, reflect.Int64:
				valHandle = i.createInt(val.Int())
			default:
				valHandle = i.internString(fmt.Sprintf("%v", val.Interface()))
			}
			dict = i.dictSet(dict, key, valHandle)
		}
		i.SetResult(dict)

	case reflect.Ptr, reflect.Interface:
		// Check if it's already a foreign object
		if result.IsNil() {
			i.SetResultString("")
			return ResultOK
		}
		// For pointer types, check if we should wrap as foreign
		// For now, just use string representation
		i.SetResultString(fmt.Sprintf("%v", result.Interface()))

	default:
		i.SetResultString(fmt.Sprintf("%v", result.Interface()))
	}

	return ResultOK
}

// foreignDestroy handles the "destroy" method on foreign objects.
func (i *Interp) foreignDestroy(handleName string) FeatherResult {
	i.ForeignRegistry.mu.Lock()
	instance, ok := i.ForeignRegistry.instances[handleName]
	if !ok {
		i.ForeignRegistry.mu.Unlock()
		i.SetErrorString(fmt.Sprintf("invalid object handle \"%s\"", handleName))
		return ResultError
	}

	// Get type info for destructor
	info := i.ForeignRegistry.types[instance.typeName]

	// Remove from registry
	delete(i.ForeignRegistry.instances, handleName)
	delete(i.ForeignRegistry.handleToType, instance.objHandle)
	i.ForeignRegistry.mu.Unlock()

	// Call destructor if defined
	if info != nil && info.destroy.IsValid() {
		info.destroy.Call([]reflect.Value{reflect.ValueOf(instance.value)})
	}

	// Clear the foreign object
	if obj := i.getObject(instance.objHandle); obj != nil {
		obj.foreignValue = nil
		obj.isForeign = false
		obj.foreignType = ""
	}

	// Remove the command
	delete(i.Commands, handleName)
	delete(i.globalNamespace.commands, handleName)

	i.SetResultString("")
	return ResultOK
}

// GetForeignMethods returns the method names for a foreign type.
// Used by the goForeignMethods callback.
func (i *Interp) GetForeignMethods(typeName string) []string {
	if i.ForeignRegistry == nil {
		return nil
	}
	i.ForeignRegistry.mu.RLock()
	defer i.ForeignRegistry.mu.RUnlock()

	info, ok := i.ForeignRegistry.types[typeName]
	if !ok {
		return nil
	}

	methods := make([]string, 0, len(info.methods)+1)
	for name := range info.methods {
		methods = append(methods, name)
	}
	methods = append(methods, "destroy")
	return methods
}

// GetForeignStringRep returns a custom string representation for a foreign object.
// Used by the goForeignStringRep callback.
func (i *Interp) GetForeignStringRep(obj FeatherObj) string {
	if i.ForeignRegistry == nil {
		return ""
	}
	i.ForeignRegistry.mu.RLock()
	instance, ok := i.ForeignRegistry.handleToType[obj]
	i.ForeignRegistry.mu.RUnlock()
	if !ok {
		return ""
	}

	i.ForeignRegistry.mu.RLock()
	info := i.ForeignRegistry.types[instance.typeName]
	i.ForeignRegistry.mu.RUnlock()

	if info != nil && info.stringRep.IsValid() {
		results := info.stringRep.Call([]reflect.Value{reflect.ValueOf(instance.value)})
		if len(results) > 0 {
			return results[0].String()
		}
	}

	return instance.handleName
}

// Helper methods for creating TCL values

// NewList creates an empty list object.
func (i *Interp) NewList() FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{isList: true, listItems: []FeatherObj{}}
	return id
}

// ListAppend appends an item to a list and returns the list.
// If the object is not a list, returns it unchanged.
func (i *Interp) ListAppend(list FeatherObj, item FeatherObj) FeatherObj {
	obj := i.getObject(list)
	if obj != nil && obj.isList {
		obj.listItems = append(obj.listItems, item)
		obj.stringVal = "" // invalidate string cache
	}
	return list
}

// NewInt creates an integer object.
func (i *Interp) NewInt(val int64) FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{intVal: val, isInt: true}
	return id
}

// NewDouble creates a floating-point object.
func (i *Interp) NewDouble(val float64) FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{dblVal: val, isDouble: true}
	return id
}

// NewDict creates an empty dict object.
func (i *Interp) NewDict() FeatherObj {
	id := i.nextID
	i.nextID++
	i.objects[id] = &Object{
		isDict:    true,
		dictItems: make(map[string]FeatherObj),
		dictOrder: []string{},
	}
	return id
}

// DictSet sets a key-value pair in a dict and returns the dict.
// If the object is not a dict, returns it unchanged.
func (i *Interp) DictSet(dict FeatherObj, key string, val FeatherObj) FeatherObj {
	obj := i.getObject(dict)
	if obj != nil && obj.isDict {
		if _, exists := obj.dictItems[key]; !exists {
			obj.dictOrder = append(obj.dictOrder, key)
		}
		obj.dictItems[key] = val
		obj.stringVal = "" // invalidate string cache
	}
	return dict
}

// DictGet retrieves a value from a dict by key.
// Returns the value and true if found, or 0 and false if not found.
func (i *Interp) DictGet(dict FeatherObj, key string) (FeatherObj, bool) {
	obj := i.getObject(dict)
	if obj != nil && obj.isDict {
		val, ok := obj.dictItems[key]
		return val, ok
	}
	return 0, false
}

// Type returns the native type of an object: "string", "int", "double", "list", "dict",
// or the foreign type name if it's a foreign object.
func (i *Interp) Type(h FeatherObj) string {
	obj := i.getObject(h)
	if obj == nil {
		return "string"
	}
	if obj.isForeign {
		return obj.foreignType
	}
	if obj.isDict {
		return "dict"
	}
	if obj.isList {
		return "list"
	}
	if obj.isDouble {
		return "double"
	}
	if obj.isInt {
		return "int"
	}
	return "string"
}

// Aliases for internal use (to avoid breaking existing code)

func (i *Interp) createList() FeatherObj {
	return i.NewList()
}

func (i *Interp) listPush(list FeatherObj, item FeatherObj) FeatherObj {
	return i.ListAppend(list, item)
}

func (i *Interp) createInt(val int64) FeatherObj {
	return i.NewInt(val)
}

func (i *Interp) createDict() FeatherObj {
	return i.NewDict()
}

func (i *Interp) dictSet(dict FeatherObj, key string, val FeatherObj) FeatherObj {
	return i.DictSet(dict, key, val)
}
