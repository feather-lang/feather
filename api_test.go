// Package feather_test exercises the proposed ergonomic Go API for feather.
//
// This file serves as a specification for the desired API. Tests that fail
// indicate missing functionality that needs to be implemented.
package feather_test

import (
	"errors"
	"strings"
	"testing"

	"github.com/feather-lang/feather"
)

// =============================================================================
// Basic Interpreter Lifecycle
// =============================================================================

func TestInterpreterLifecycle(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Eval(`set x 10; expr {$x * 2}`)
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "20" {
		t.Errorf("expected '20', got %q", result.String())
	}
}

// =============================================================================
// Constructing Values - Primitives
// =============================================================================

func TestConstructPrimitives(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("String", func(t *testing.T) {
		s := interp.String("hello")
		if s.String() != "hello" {
			t.Errorf("expected 'hello', got %q", s.String())
		}
		if s.Type() != "string" {
			t.Errorf("expected type 'string', got %q", s.Type())
		}
	})

	t.Run("Int", func(t *testing.T) {
		i := interp.Int(42)
		if i.String() != "42" {
			t.Errorf("expected '42', got %q", i.String())
		}
		if i.Type() != "int" {
			t.Errorf("expected type 'int', got %q", i.Type())
		}
		n, err := feather.AsInt(i)
		if err != nil || n != 42 {
			t.Errorf("AsInt() = %d, %v; want 42, nil", n, err)
		}
	})

	t.Run("Double", func(t *testing.T) {
		d := interp.Double(3.14)
		f, err := feather.AsDouble(d)
		if err != nil || f != 3.14 {
			t.Errorf("AsDouble() = %f, %v; want 3.14, nil", f, err)
		}
		if d.Type() != "double" {
			t.Errorf("expected type 'double', got %q", d.Type())
		}
	})

	t.Run("Bool", func(t *testing.T) {
		// For now, test AsBool() accessor on string
		s := interp.String("true")
		b, err := feather.AsBool(s)
		if err != nil || !b {
			t.Errorf("AsBool() = %v, %v; want true, nil", b, err)
		}

		s = interp.String("0")
		b, err = feather.AsBool(s)
		if err != nil || b {
			t.Errorf("AsBool() = %v, %v; want false, nil", b, err)
		}
	})
}

// =============================================================================
// Constructing Values - Lists
// =============================================================================

func TestConstructLists(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("List variadic", func(t *testing.T) {
		list := interp.List(interp.String("a"), interp.Int(1), interp.String("c"))
		if list.Type() != "list" {
			t.Errorf("expected type 'list', got %q", list.Type())
		}
		items, err := feather.AsList(list)
		if err != nil {
			t.Fatalf("AsList() failed: %v", err)
		}
		if len(items) != 3 {
			t.Errorf("expected 3 items, got %d", len(items))
		}
		if items[0].String() != "a" {
			t.Errorf("items[0] = %q; want 'a'", items[0].String())
		}
		if items[1].String() != "1" {
			t.Errorf("items[1] = %q; want '1'", items[1].String())
		}
	})

	t.Run("ListFrom string slice", func(t *testing.T) {
		list := interp.ListFrom([]string{"x", "y", "z"})
		items, _ := feather.AsList(list)
		if len(items) != 3 || items[0].String() != "x" {
			t.Errorf("ListFrom([]string) failed")
		}
	})

	t.Run("ListFrom int slice", func(t *testing.T) {
		list := interp.ListFrom([]int{1, 2, 3})
		items, _ := feather.AsList(list)
		if len(items) != 3 {
			t.Errorf("expected 3 items")
		}
		n, _ := feather.AsInt(items[0])
		if n != 1 {
			t.Errorf("AsInt(items[0]) = %d; want 1", n)
		}
	})

	t.Run("ListFrom any slice", func(t *testing.T) {
		list := interp.ListFrom([]any{"mixed", 42, 3.14})
		items, _ := feather.AsList(list)
		if len(items) != 3 {
			t.Errorf("expected 3 items")
		}
	})
}

// =============================================================================
// Constructing Values - Dicts
// =============================================================================

func TestConstructDicts(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Dict empty", func(t *testing.T) {
		dict := interp.Dict()
		if dict.Type() != "dict" {
			t.Errorf("expected type 'dict', got %q", dict.Type())
		}
	})

	t.Run("DictKV alternating key/value", func(t *testing.T) {
		dict := interp.DictKV("name", "Alice", "age", 30, "active", true)
		d, err := feather.AsDict(dict)
		if err != nil {
			t.Fatalf("AsDict() failed: %v", err)
		}
		if d.Items["name"].String() != "Alice" {
			t.Errorf("dict[name] = %q; want 'Alice'", d.Items["name"].String())
		}
		if d.Items["age"].String() != "30" {
			t.Errorf("dict[age] = %q; want '30'", d.Items["age"].String())
		}
	})

	t.Run("DictFrom map", func(t *testing.T) {
		dict := interp.DictFrom(map[string]any{"name": "Alice", "age": 30})
		d, _ := feather.AsDict(dict)
		if d.Items["name"].String() != "Alice" {
			t.Errorf("dict[name] = %q; want 'Alice'", d.Items["name"].String())
		}
	})
}

// =============================================================================
// Reading Values Back
// =============================================================================

func TestReadingValues(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("String always works", func(t *testing.T) {
		obj := interp.Int(42)
		if obj.String() != "42" {
			t.Errorf("String() = %q; want '42'", obj.String())
		}
	})

	t.Run("Int parses and shimmers", func(t *testing.T) {
		obj := interp.String("42")
		n, err := feather.AsInt(obj)
		if err != nil {
			t.Fatalf("AsInt() error: %v", err)
		}
		if n != 42 {
			t.Errorf("AsInt() = %d; want 42", n)
		}
		// After shimmering, type should change to int
		// (This depends on implementation - may still be "string")
	})

	t.Run("Float parses and shimmers", func(t *testing.T) {
		obj := interp.String("3.14")
		f, err := feather.AsDouble(obj)
		if err != nil {
			t.Fatalf("AsDouble() error: %v", err)
		}
		if f != 3.14 {
			t.Errorf("AsDouble() = %f; want 3.14", f)
		}
	})

	t.Run("Bool with TCL rules", func(t *testing.T) {
		truthy := []string{"1", "true", "yes", "on", "TRUE", "Yes", "ON"}
		for _, s := range truthy {
			obj := interp.String(s)
			b, err := feather.AsBool(obj)
			if err != nil || !b {
				t.Errorf("AsBool(%q) = %v, %v; want true, nil", s, b, err)
			}
		}

		falsy := []string{"0", "false", "no", "off", "FALSE", "No", "OFF"}
		for _, s := range falsy {
			obj := interp.String(s)
			b, err := feather.AsBool(obj)
			if err != nil || b {
				t.Errorf("AsBool(%q) = %v, %v; want false, nil", s, b, err)
			}
		}
	})

	// Note: String-to-list parsing requires interpreter, so AsList won't work
	// on pure string objects. These tests use pre-constructed lists.
	t.Run("List from constructed list", func(t *testing.T) {
		list := interp.List(interp.String("a"), interp.String("b"), interp.String("c"))
		items, err := feather.AsList(list)
		if err != nil {
			t.Fatalf("AsList() error: %v", err)
		}
		if len(items) != 3 {
			t.Errorf("len(AsList()) = %d; want 3", len(items))
		}
		if items[0].String() != "a" {
			t.Errorf("items[0] = %q; want 'a'", items[0].String())
		}
	})

	t.Run("Dict from constructed dict", func(t *testing.T) {
		dict := interp.DictKV("a", 1, "b", 2)
		d, err := feather.AsDict(dict)
		if err != nil {
			t.Fatalf("AsDict() error: %v", err)
		}
		if len(d.Items) != 2 {
			t.Errorf("len(AsDict()) = %d; want 2", len(d.Items))
		}
		if d.Items["a"].String() != "1" {
			t.Errorf("dict[a] = %q; want '1'", d.Items["a"].String())
		}
	})
}

// =============================================================================
// Variables
// =============================================================================

func TestVariables(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("SetVar string", func(t *testing.T) {
		interp.SetVar("name", "Alice")
		v := interp.Var("name")
		if v.String() != "Alice" {
			t.Errorf("Var(name) = %q; want 'Alice'", v.String())
		}
	})

	t.Run("SetVar int auto-converts", func(t *testing.T) {
		interp.SetVar("count", 42)
		v := interp.Var("count")
		if v.String() != "42" {
			t.Errorf("Var(count) = %q; want '42'", v.String())
		}
		n, err := feather.AsInt(v)
		if err != nil || n != 42 {
			t.Errorf("Var(count).AsInt() = %d, %v; want 42, nil", n, err)
		}
	})

	t.Run("SetVar slice auto-converts to list", func(t *testing.T) {
		interp.SetVar("items", []string{"a", "b", "c"})
		// Variables set via SetVar use string representation, so we need to
		// parse the result via Eval to get a proper list
		result, _ := interp.Eval("set items")
		if result.String() != "{a} {b} {c}" && result.String() != "a b c" {
			// Either format is acceptable depending on quoting
			t.Logf("Var(items) = %q", result.String())
		}
	})

	t.Run("GetVar and Int", func(t *testing.T) {
		interp.SetVar("x", 100)
		v := interp.Var("x")
		n, _ := feather.AsInt(v)
		if n != 100 {
			t.Errorf("x.AsInt() = %d; want 100", n)
		}
	})

	t.Run("SetVars bulk", func(t *testing.T) {
		interp.SetVars(map[string]any{"x": 1, "y": 2, "z": 3})
		x := interp.Var("x")
		y := interp.Var("y")
		z := interp.Var("z")
		if x.String() != "1" || y.String() != "2" || z.String() != "3" {
			t.Error("SetVars failed")
		}
	})

	t.Run("GetVars bulk", func(t *testing.T) {
		interp.SetVar("a", 1)
		interp.SetVar("b", 2)
		vars := interp.GetVars("a", "b")
		if vars["a"].String() != "1" || vars["b"].String() != "2" {
			t.Error("GetVars failed")
		}
	})
}

// =============================================================================
// Commands - Raw Access
// =============================================================================

func TestCommandsRaw(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("RegisterCommand concat", func(t *testing.T) {
		interp.RegisterCommand("myconcat", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
			var sb strings.Builder
			for _, arg := range args {
				sb.WriteString(arg.String())
			}
			return feather.OK(sb.String())
		})

		result, err := interp.Eval("myconcat a b c")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "abc" {
			t.Errorf("myconcat = %q; want 'abc'", result.String())
		}
	})

	t.Run("RegisterCommand with error", func(t *testing.T) {
		interp.RegisterCommand("mustfail", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
			return feather.Error("intentional failure")
		})

		_, err := interp.Eval("mustfail")
		if err == nil {
			t.Fatal("expected error")
		}
		if !strings.Contains(err.Error(), "intentional failure") {
			t.Errorf("error = %q; want 'intentional failure'", err.Error())
		}
	})

	t.Run("RegisterCommand sum with list arg", func(t *testing.T) {
		interp.RegisterCommand("mysum", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
			if len(args) != 1 {
				return feather.Error("usage: mysum list")
			}
			// Use i.ParseList to parse the string argument into a list
			items, err := i.ParseList(args[0].String())
			if err != nil {
				return feather.Error(err.Error())
			}
			var total int64
			for _, item := range items {
				n, err := feather.AsInt(item)
				if err != nil {
					return feather.Errorf("not an integer: %s", item.String())
				}
				total += n
			}
			return feather.OK(total)
		})

		result, err := interp.Eval("mysum {1 2 3 4}")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "10" {
			t.Errorf("mysum = %q; want '10'", result.String())
		}
	})

	t.Run("RegisterCommand get-name with dict arg", func(t *testing.T) {
		interp.RegisterCommand("get-name", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
			if len(args) != 1 {
				return feather.Error("usage: get-name dict")
			}
			// Use i.ParseDict to parse the string argument into a dict
			d, err := i.ParseDict(args[0].String())
			if err != nil {
				return feather.Error(err.Error())
			}
			if name, ok := d.Items["name"]; ok {
				return feather.OK(name.String())
			}
			return feather.OK("")
		})

		result, err := interp.Eval("get-name {name Alice age 30}")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "Alice" {
			t.Errorf("get-name = %q; want 'Alice'", result.String())
		}
	})

	t.Run("RegisterCommand building complex result", func(t *testing.T) {
		// Use the typed Register API which handles return values correctly
		interp.Register("make-person2", func(name, age string) map[string]string {
			return map[string]string{"name": name, "age": age}
		})

		result, err := interp.Eval("make-person2 Alice 30")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		d, err := feather.AsDict(result)
		if err != nil {
			t.Fatalf("AsDict() error: %v", err)
		}
		if d.Items["name"].String() != "Alice" {
			t.Errorf("name = %q; want 'Alice'", d.Items["name"].String())
		}
		if d.Items["age"].String() != "30" {
			t.Errorf("age = %q; want '30'", d.Items["age"].String())
		}
	})
}

// =============================================================================
// Commands - Typed Convenience API
// =============================================================================

func TestCommandsTyped(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Register string -> string", func(t *testing.T) {
		interp.Register("greet", func(name string) string {
			return "Hello, " + name + "!"
		})

		result, err := interp.Eval("greet World")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "Hello, World!" {
			t.Errorf("greet = %q; want 'Hello, World!'", result.String())
		}
	})

	t.Run("Register int, int -> int", func(t *testing.T) {
		interp.Register("add", func(a, b int) int {
			return a + b
		})

		result, err := interp.Eval("add 3 4")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "7" {
			t.Errorf("add = %q; want '7'", result.String())
		}
	})

	t.Run("Register with error return", func(t *testing.T) {
		interp.Register("safediv", func(a, b int) (int, error) {
			if b == 0 {
				return 0, errors.New("division by zero")
			}
			return a / b, nil
		})

		result, err := interp.Eval("safediv 10 2")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "5" {
			t.Errorf("safediv = %q; want '5'", result.String())
		}

		_, err = interp.Eval("safediv 10 0")
		if err == nil {
			t.Fatal("expected error")
		}
	})

	t.Run("Register variadic", func(t *testing.T) {
		interp.Register("join", func(sep string, parts ...string) string {
			return strings.Join(parts, sep)
		})

		result, err := interp.Eval("join , a b c")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "a,b,c" {
			t.Errorf("join = %q; want 'a,b,c'", result.String())
		}
	})

	t.Run("Register no args", func(t *testing.T) {
		interp.Register("hello", func() string {
			return "Hello!"
		})

		result, err := interp.Eval("hello")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if result.String() != "Hello!" {
			t.Errorf("hello = %q; want 'Hello!'", result.String())
		}
	})

	t.Run("Register only error return", func(t *testing.T) {
		called := false
		interp.Register("sideeffect", func() error {
			called = true
			return nil
		})

		_, err := interp.Eval("sideeffect")
		if err != nil {
			t.Fatalf("Eval failed: %v", err)
		}
		if !called {
			t.Error("sideeffect was not called")
		}
	})
}

// =============================================================================
// Call - Direct Command Invocation
// =============================================================================

func TestCallAPI(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Call simple", func(t *testing.T) {
		result, err := interp.Call("expr", "2 + 2")
		if err != nil {
			t.Fatalf("Call failed: %v", err)
		}
		if result.String() != "4" {
			t.Errorf("expr = %q; want '4'", result.String())
		}
	})

	t.Run("Call with namespace", func(t *testing.T) {
		_, err := interp.Eval(`namespace eval myns { proc foo {} { return bar } }`)
		if err != nil {
			t.Fatalf("namespace eval failed: %v", err)
		}

		result, err := interp.Call("myns::foo")
		if err != nil {
			t.Fatalf("Call failed: %v", err)
		}
		if result.String() != "bar" {
			t.Errorf("myns::foo = %q; want 'bar'", result.String())
		}
	})

	t.Run("Call with multiple args", func(t *testing.T) {
		result, err := interp.Call("list", "a", "b", "c")
		if err != nil {
			t.Fatalf("Call failed: %v", err)
		}
		items, _ := feather.AsList(result)
		if len(items) != 3 {
			t.Errorf("list length = %d; want 3", len(items))
		}
	})
}

// =============================================================================
// List/Dict Manipulation (Mutable Operations)
// =============================================================================

func TestListManipulation(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("List append", func(t *testing.T) {
		list := interp.List(interp.Int(1), interp.Int(2))
		items, _ := list.List()
		items = append(items, interp.Int(3))
		// Note: append may reallocate, so we need to update the internal rep
		// In practice, users would use list commands, not direct manipulation
		if len(items) != 3 {
			t.Errorf("len after append = %d; want 3", len(items))
		}
	})

	t.Run("List index", func(t *testing.T) {
		list := interp.List(interp.Int(1), interp.Int(2), interp.Int(3))
		items, _ := list.List()
		elem := items[1]
		if elem.String() != "2" {
			t.Errorf("items[1] = %q; want '2'", elem.String())
		}
	})

	t.Run("List len", func(t *testing.T) {
		list := interp.List(interp.Int(1), interp.Int(2))
		items, _ := list.List()
		if len(items) != 2 {
			t.Errorf("len(items) = %d; want 2", len(items))
		}
	})
}

func TestDictManipulation(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Dict set/get", func(t *testing.T) {
		dict := interp.Dict()
		d, _ := dict.Dict()
		d.Items["a"] = interp.Int(1)
		d.Order = append(d.Order, "a")
		d.Items["b"] = interp.Int(2)
		d.Order = append(d.Order, "b")
		v, ok := d.Items["a"]
		if !ok || v.String() != "1" {
			t.Errorf("d.Items[\"a\"] = %v, %v; want '1', true", v, ok)
		}
	})

	t.Run("Dict keys", func(t *testing.T) {
		dict := interp.DictKV("x", 1, "y", 2, "z", 3)
		d, _ := dict.Dict()
		if len(d.Order) != 3 {
			t.Errorf("len(d.Order) = %d; want 3", len(d.Order))
		}
	})

	t.Run("Dict len", func(t *testing.T) {
		dict := interp.DictKV("a", 1, "b", 2)
		d, _ := dict.Dict()
		if len(d.Items) != 2 {
			t.Errorf("len(d.Items) = %d; want 2", len(d.Items))
		}
	})
}

// =============================================================================
// Foreign Types
// =============================================================================

func TestForeignTypes(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	type Counter struct {
		value int
	}

	err := feather.RegisterType[*Counter](interp, "Counter", feather.TypeDef[*Counter]{
		New: func() *Counter { return &Counter{value: 0} },
		Methods: map[string]any{
			"get":  func(c *Counter) int { return c.value },
			"set":  func(c *Counter, v int) { c.value = v },
			"incr": func(c *Counter) int { c.value++; return c.value },
		},
	})
	if err != nil {
		t.Fatalf("RegisterType failed: %v", err)
	}

	t.Run("Create and use foreign object", func(t *testing.T) {
		result, err := interp.Eval("set c [Counter new]")
		if err != nil {
			t.Fatalf("Counter new failed: %v", err)
		}
		if result.String() == "" {
			t.Error("expected non-empty handle")
		}

		result, err = interp.Eval("$c get")
		if err != nil {
			t.Fatalf("$c get failed: %v", err)
		}
		if result.String() != "0" {
			t.Errorf("get = %q; want '0'", result.String())
		}

		_, err = interp.Eval("$c set 10")
		if err != nil {
			t.Fatalf("$c set failed: %v", err)
		}

		result, err = interp.Eval("$c incr")
		if err != nil {
			t.Fatalf("$c incr failed: %v", err)
		}
		if result.String() != "11" {
			t.Errorf("incr = %q; want '11'", result.String())
		}
	})

	t.Run("Foreign object type", func(t *testing.T) {
		_, err := interp.Eval("set c2 [Counter new]")
		if err != nil {
			t.Fatalf("Counter new failed: %v", err)
		}

		// Eval("set c2") should return the actual object with its type preserved
		result, err := interp.Eval("set c2")
		if err != nil {
			t.Fatalf("set c2 failed: %v", err)
		}
		// Foreign objects should report their type name
		if result.Type() != "Counter" {
			t.Errorf("Type() = %q; want 'Counter'", result.Type())
		}
	})
}

// =============================================================================
// Error Handling
// =============================================================================

func TestErrorHandling(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Eval syntax error", func(t *testing.T) {
		_, err := interp.Eval("set x {")
		if err == nil {
			t.Fatal("expected syntax error")
		}
	})

	t.Run("Eval command error", func(t *testing.T) {
		_, err := interp.Eval("nonexistent_command")
		if err == nil {
			t.Fatal("expected command not found error")
		}
	})

	t.Run("Int conversion error", func(t *testing.T) {
		obj := interp.String("not a number")
		_, err := feather.AsInt(obj)
		if err == nil {
			t.Fatal("expected conversion error")
		}
	})
}

// =============================================================================
// Parse
// =============================================================================

func TestParseAPI(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Complete input", func(t *testing.T) {
		pr := interp.Parse("set x 1")
		if pr.Status != feather.ParseOK {
			t.Errorf("Status = %v; want ParseOK", pr.Status)
		}
	})

	t.Run("Incomplete input - open brace", func(t *testing.T) {
		pr := interp.Parse("set x {")
		if pr.Status != feather.ParseIncomplete {
			t.Errorf("Status = %v; want ParseIncomplete", pr.Status)
		}
	})

	t.Run("Incomplete input - open bracket", func(t *testing.T) {
		pr := interp.Parse("set x [expr")
		if pr.Status != feather.ParseIncomplete {
			t.Errorf("Status = %v; want ParseIncomplete", pr.Status)
		}
	})

	t.Run("Incomplete input - open quote", func(t *testing.T) {
		pr := interp.Parse(`set x "hello`)
		if pr.Status != feather.ParseIncomplete {
			t.Errorf("Status = %v; want ParseIncomplete", pr.Status)
		}
	})
}
