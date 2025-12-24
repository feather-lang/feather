package feather_test

import (
	"errors"
	"testing"

	"github.com/feather-lang/feather"
)

func TestNew(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Eval("expr {2 + 2}")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "4" {
		t.Errorf("expected '4', got %q", result.String())
	}
}

func TestSetVar(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	interp.SetVar("name", "World")
	result, err := interp.Eval(`set greeting "Hello, $name!"`)
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "Hello, World!" {
		t.Errorf("expected 'Hello, World!', got %q", result.String())
	}
}

func TestVar(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	interp.SetVar("x", "42")
	v := interp.Var("x")
	if v.String() != "42" {
		t.Errorf("expected '42', got %q", v.String())
	}

	n, err := v.Int()
	if err != nil {
		t.Fatalf("Int() failed: %v", err)
	}
	if n != 42 {
		t.Errorf("expected 42, got %d", n)
	}
}

func TestRegisterSimple(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	interp.Register("double", func(x int) int {
		return x * 2
	})

	result, err := interp.Eval("double 21")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "42" {
		t.Errorf("expected '42', got %q", result.String())
	}
}

func TestRegisterWithError(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	interp.Register("divide", func(a, b int) (int, error) {
		if b == 0 {
			return 0, errors.New("division by zero")
		}
		return a / b, nil
	})

	// Test successful division
	result, err := interp.Eval("divide 10 2")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "5" {
		t.Errorf("expected '5', got %q", result.String())
	}

	// Test division by zero
	_, err = interp.Eval("divide 10 0")
	if err == nil {
		t.Fatal("expected error for division by zero")
	}
	if err.Error() != "division by zero" {
		t.Errorf("expected 'division by zero', got %q", err.Error())
	}
}

func TestRegisterStringFunc(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	interp.Register("greet", func(name string) string {
		return "Hello, " + name + "!"
	})

	result, err := interp.Eval("greet World")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "Hello, World!" {
		t.Errorf("expected 'Hello, World!', got %q", result.String())
	}
}

func TestValueList(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Eval("list 1 2 3")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}

	list, err := result.List()
	if err != nil {
		t.Fatalf("List() failed: %v", err)
	}
	if len(list) != 3 {
		t.Errorf("expected 3 items, got %d", len(list))
	}

	for i, expected := range []string{"1", "2", "3"} {
		if list[i].String() != expected {
			t.Errorf("item %d: expected %q, got %q", i, expected, list[i].String())
		}
	}
}

func TestValueDict(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Eval("dict create a 1 b 2")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}

	dict, err := result.Dict()
	if err != nil {
		t.Fatalf("Dict() failed: %v", err)
	}
	if len(dict) != 2 {
		t.Errorf("expected 2 items, got %d", len(dict))
	}

	if dict["a"].String() != "1" {
		t.Errorf("a: expected '1', got %q", dict["a"].String())
	}
	if dict["b"].String() != "2" {
		t.Errorf("b: expected '2', got %q", dict["b"].String())
	}
}

func TestForeignType(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	type Counter struct {
		value int
	}

	err := feather.Register[*Counter](interp, "Counter", feather.TypeDef[*Counter]{
		New: func() *Counter { return &Counter{value: 0} },
		Methods: map[string]any{
			"get":  func(c *Counter) int { return c.value },
			"set":  func(c *Counter, v int) { c.value = v },
			"incr": func(c *Counter) int { c.value++; return c.value },
		},
	})
	if err != nil {
		t.Fatalf("Register failed: %v", err)
	}

	// Create instance
	result, err := interp.Eval("set c [Counter new]")
	if err != nil {
		t.Fatalf("Counter new failed: %v", err)
	}
	if result.String() == "" {
		t.Error("expected non-empty handle")
	}

	// Call methods
	result, err = interp.Eval("$c get")
	if err != nil {
		t.Fatalf("$c get failed: %v", err)
	}
	if result.String() != "0" {
		t.Errorf("expected '0', got %q", result.String())
	}

	result, err = interp.Eval("$c set 10")
	if err != nil {
		t.Fatalf("$c set failed: %v", err)
	}

	result, err = interp.Eval("$c incr")
	if err != nil {
		t.Fatalf("$c incr failed: %v", err)
	}
	if result.String() != "11" {
		t.Errorf("expected '11', got %q", result.String())
	}
}

func TestCall(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Call("expr", "2 + 2")
	if err != nil {
		t.Fatalf("Call failed: %v", err)
	}
	if result.String() != "4" {
		t.Errorf("expected '4', got %q", result.String())
	}
}

func TestParse(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	// Complete input
	pr := interp.Parse("set x 1")
	if pr.Status != feather.ParseOK {
		t.Errorf("expected ParseOK, got %v", pr.Status)
	}

	// Incomplete input
	pr = interp.Parse("set x {")
	if pr.Status != feather.ParseIncomplete {
		t.Errorf("expected ParseIncomplete, got %v", pr.Status)
	}
}

func TestEvalReturnsTypedValues(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	// Integer result
	result, err := interp.Eval("expr {2 + 2}")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.Type() != "int" {
		t.Errorf("expr result type: expected 'int', got %q", result.Type())
	}
	n, err := result.Int()
	if err != nil || n != 4 {
		t.Errorf("Int() = %d, %v; want 4, nil", n, err)
	}

	// List result
	result, err = interp.Eval("list a b c")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.Type() != "list" {
		t.Errorf("list result type: expected 'list', got %q", result.Type())
	}
	items, err := result.List()
	if err != nil || len(items) != 3 {
		t.Errorf("List() = %v, %v; want 3 items", items, err)
	}

	// Dict result
	result, err = interp.Eval("dict create x 1 y 2")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.Type() != "dict" {
		t.Errorf("dict result type: expected 'dict', got %q", result.Type())
	}
	m, err := result.Dict()
	if err != nil || len(m) != 2 {
		t.Errorf("Dict() = %v, %v; want 2 items", m, err)
	}

	// String result (from set)
	result, err = interp.Eval("set x hello")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.Type() != "string" {
		t.Errorf("set result type: expected 'string', got %q", result.Type())
	}
}

func TestValueTypes(t *testing.T) {
	t.Run("intValue", func(t *testing.T) {
		v := feather.NewInt(42)
		if v.Type() != "int" {
			t.Errorf("expected type 'int', got %q", v.Type())
		}
		if v.String() != "42" {
			t.Errorf("expected '42', got %q", v.String())
		}
		n, err := v.Int()
		if err != nil || n != 42 {
			t.Errorf("Int() = %d, %v; want 42, nil", n, err)
		}
		f, err := v.Float()
		if err != nil || f != 42.0 {
			t.Errorf("Float() = %f, %v; want 42.0, nil", f, err)
		}
		_, err = v.Bool()
		if err == nil {
			t.Error("Bool() on 42 should fail")
		}
		// 0 and 1 should work for Bool
		v0 := feather.NewInt(0)
		b, err := v0.Bool()
		if err != nil || b != false {
			t.Errorf("Bool() on 0 = %v, %v; want false, nil", b, err)
		}
		v1 := feather.NewInt(1)
		b, err = v1.Bool()
		if err != nil || b != true {
			t.Errorf("Bool() on 1 = %v, %v; want true, nil", b, err)
		}
	})

	t.Run("floatValue", func(t *testing.T) {
		v := feather.NewFloat(3.14)
		if v.Type() != "double" {
			t.Errorf("expected type 'double', got %q", v.Type())
		}
		f, err := v.Float()
		if err != nil || f != 3.14 {
			t.Errorf("Float() = %f, %v; want 3.14, nil", f, err)
		}
		n, err := v.Int()
		if err != nil || n != 3 {
			t.Errorf("Int() = %d, %v; want 3, nil", n, err)
		}
	})

	t.Run("listValue", func(t *testing.T) {
		v := feather.NewList(feather.NewInt(1), feather.NewString("two"), feather.NewFloat(3.0))
		if v.Type() != "list" {
			t.Errorf("expected type 'list', got %q", v.Type())
		}
		if v.String() != "1 two 3" {
			t.Errorf("String() = %q; want '1 two 3'", v.String())
		}
		items, err := v.List()
		if err != nil || len(items) != 3 {
			t.Errorf("List() = %v, %v; want 3 items", items, err)
		}
		if items[0].Type() != "int" {
			t.Errorf("items[0].Type() = %q; want 'int'", items[0].Type())
		}

		// Single-element list can shimmer to scalar
		single := feather.NewList(feather.NewInt(42))
		n, err := single.Int()
		if err != nil || n != 42 {
			t.Errorf("single.Int() = %d, %v; want 42, nil", n, err)
		}
	})

	t.Run("dictValue", func(t *testing.T) {
		v := feather.NewDict(
			feather.NewString("a"), feather.NewInt(1),
			feather.NewString("b"), feather.NewString("two"),
		)
		if v.Type() != "dict" {
			t.Errorf("expected type 'dict', got %q", v.Type())
		}
		m, err := v.Dict()
		if err != nil || len(m) != 2 {
			t.Errorf("Dict() = %v, %v; want 2 items", m, err)
		}
		if m["a"].String() != "1" {
			t.Errorf("m['a'] = %q; want '1'", m["a"].String())
		}

		// Dict to list
		items, err := v.List()
		if err != nil || len(items) != 4 {
			t.Errorf("List() = %v, %v; want 4 items", items, err)
		}
	})

	t.Run("foreignValue", func(t *testing.T) {
		v := feather.NewForeign("Mux", "mux1")
		if v.Type() != "Mux" {
			t.Errorf("expected type 'Mux', got %q", v.Type())
		}
		if v.String() != "mux1" {
			t.Errorf("String() = %q; want 'mux1'", v.String())
		}
		_, err := v.Int()
		if err == nil {
			t.Error("Int() on foreign should fail")
		}
	})
}
