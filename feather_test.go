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

	n, err := feather.AsInt(v)
	if err != nil {
		t.Fatalf("AsInt() failed: %v", err)
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

func TestObjectList(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Eval("list 1 2 3")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}

	list, err := feather.AsList(result)
	if err != nil {
		t.Fatalf("AsList() failed: %v", err)
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

func TestObjectDict(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	result, err := interp.Eval("dict create a 1 b 2")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}

	dict, err := feather.AsDict(result)
	if err != nil {
		t.Fatalf("AsDict() failed: %v", err)
	}
	if len(dict.Items) != 2 {
		t.Errorf("expected 2 items, got %d", len(dict.Items))
	}

	if dict.Items["a"].String() != "1" {
		t.Errorf("a: expected '1', got %q", dict.Items["a"].String())
	}
	if dict.Items["b"].String() != "2" {
		t.Errorf("b: expected '2', got %q", dict.Items["b"].String())
	}
}

func TestForeignType(t *testing.T) {
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
	n, err := feather.AsInt(result)
	if err != nil || n != 4 {
		t.Errorf("AsInt() = %d, %v; want 4, nil", n, err)
	}

	// List result
	result, err = interp.Eval("list a b c")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.Type() != "list" {
		t.Errorf("list result type: expected 'list', got %q", result.Type())
	}
	items, err := feather.AsList(result)
	if err != nil || len(items) != 3 {
		t.Errorf("AsList() = %v, %v; want 3 items", items, err)
	}

	// Dict result
	result, err = interp.Eval("dict create x 1 y 2")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.Type() != "dict" {
		t.Errorf("dict result type: expected 'dict', got %q", result.Type())
	}
	m, err := feather.AsDict(result)
	if err != nil || len(m.Items) != 2 {
		t.Errorf("AsDict() = %v, %v; want 2 items", m, err)
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

func TestInterpObjectCreation(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	t.Run("Int", func(t *testing.T) {
		v := interp.Int(42)
		if v.Type() != "int" {
			t.Errorf("expected type 'int', got %q", v.Type())
		}
		if v.String() != "42" {
			t.Errorf("expected '42', got %q", v.String())
		}
		n, err := feather.AsInt(v)
		if err != nil || n != 42 {
			t.Errorf("AsInt() = %d, %v; want 42, nil", n, err)
		}
		f, err := feather.AsDouble(v)
		if err != nil || f != 42.0 {
			t.Errorf("AsDouble() = %f, %v; want 42.0, nil", f, err)
		}
	})

	t.Run("Double", func(t *testing.T) {
		v := interp.Double(3.14)
		if v.Type() != "double" {
			t.Errorf("expected type 'double', got %q", v.Type())
		}
		f, err := feather.AsDouble(v)
		if err != nil || f != 3.14 {
			t.Errorf("AsDouble() = %f, %v; want 3.14, nil", f, err)
		}
		n, err := feather.AsInt(v)
		if err != nil || n != 3 {
			t.Errorf("AsInt() = %d, %v; want 3, nil", n, err)
		}
	})

	t.Run("List", func(t *testing.T) {
		v := interp.List(interp.Int(1), interp.String("two"), interp.Int(3))
		if v.Type() != "list" {
			t.Errorf("expected type 'list', got %q", v.Type())
		}
		if v.String() != "1 two 3" {
			t.Errorf("String() = %q; want '1 two 3'", v.String())
		}
		items, err := feather.AsList(v)
		if err != nil || len(items) != 3 {
			t.Errorf("AsList() = %v, %v; want 3 items", items, err)
		}
		if items[0].Type() != "int" {
			t.Errorf("items[0].Type() = %q; want 'int'", items[0].Type())
		}
	})

	t.Run("String", func(t *testing.T) {
		v := interp.String("hello")
		if v.Type() != "string" {
			t.Errorf("expected type 'string', got %q", v.Type())
		}
		if v.String() != "hello" {
			t.Errorf("String() = %q; want 'hello'", v.String())
		}
	})

	t.Run("Dict", func(t *testing.T) {
		v := interp.Dict()
		if v.Type() != "dict" {
			t.Errorf("expected type 'dict', got %q", v.Type())
		}
	})
}

func TestRegisterCommand(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	interp.RegisterCommand("sum", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		if len(args) < 2 {
			return feather.Errorf("wrong # args: should be \"%s a b\"", cmd.String())
		}
		a, err := feather.AsInt(args[0])
		if err != nil {
			return feather.Error(err.Error())
		}
		b, err := feather.AsInt(args[1])
		if err != nil {
			return feather.Error(err.Error())
		}
		return feather.OK(a + b)
	})

	result, err := interp.Eval("sum 3 4")
	if err != nil {
		t.Fatalf("Eval failed: %v", err)
	}
	if result.String() != "7" {
		t.Errorf("expected '7', got %q", result.String())
	}

	// Test error case
	_, err = interp.Eval("sum 1")
	if err == nil {
		t.Fatal("expected error for wrong # args")
	}
}

func TestObjectBool(t *testing.T) {
	interp := feather.New()
	defer interp.Close()

	tests := []struct {
		input    string
		expected bool
		wantErr  bool
	}{
		{"1", true, false},
		{"0", false, false},
		{"true", true, false},
		{"false", false, false},
		{"yes", true, false},
		{"no", false, false},
		{"on", true, false},
		{"off", false, false},
		{"TRUE", true, false},
		{"FALSE", false, false},
		{"42", true, false},  // "42" parses as int, non-zero is truthy
		{"hello", false, true},
	}

	for _, tc := range tests {
		v := interp.String(tc.input)
		got, err := feather.AsBool(v)
		if tc.wantErr {
			if err == nil {
				t.Errorf("AsBool(%q): expected error, got %v", tc.input, got)
			}
		} else {
			if err != nil {
				t.Errorf("AsBool(%q): unexpected error: %v", tc.input, err)
			} else if got != tc.expected {
				t.Errorf("AsBool(%q) = %v; want %v", tc.input, got, tc.expected)
			}
		}
	}
}
