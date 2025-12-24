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
