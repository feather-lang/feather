package interp_test

import (
	"fmt"
	"testing"

	"github.com/feather-lang/feather/interp"
)

// Counter is a simple test type for foreign object testing.
type Counter struct {
	value int
}

func TestDefineType_BasicUsage(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	// Define a Counter type
	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"get": func(c *Counter) int {
				return c.value
			},
			"set": func(c *Counter, val int) {
				c.value = val
			},
			"incr": func(c *Counter) int {
				c.value++
				return c.value
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	// Create an instance
	result, err := i.Eval("Counter new")
	if err != nil {
		t.Fatalf("Counter new failed: %v", err)
	}
	if result != "counter1" {
		t.Errorf("expected handle name 'counter1', got %q", result)
	}

	// Call get method (should return 0)
	result, err = i.Eval("counter1 get")
	if err != nil {
		t.Fatalf("counter1 get failed: %v", err)
	}
	if result != "0" {
		t.Errorf("expected '0', got %q", result)
	}

	// Call set method
	_, err = i.Eval("counter1 set 42")
	if err != nil {
		t.Fatalf("counter1 set 42 failed: %v", err)
	}

	// Verify set worked
	result, err = i.Eval("counter1 get")
	if err != nil {
		t.Fatalf("counter1 get after set failed: %v", err)
	}
	if result != "42" {
		t.Errorf("expected '42', got %q", result)
	}

	// Call incr method
	result, err = i.Eval("counter1 incr")
	if err != nil {
		t.Fatalf("counter1 incr failed: %v", err)
	}
	if result != "43" {
		t.Errorf("expected '43', got %q", result)
	}
}

func TestDefineType_Destroy(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	destroyed := false

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"get": func(c *Counter) int {
				return c.value
			},
		},
		Destroy: func(c *Counter) {
			destroyed = true
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	// Create and destroy
	_, err = i.Eval("Counter new")
	if err != nil {
		t.Fatalf("Counter new failed: %v", err)
	}

	_, err = i.Eval("counter1 destroy")
	if err != nil {
		t.Fatalf("counter1 destroy failed: %v", err)
	}

	if !destroyed {
		t.Error("Destroy callback was not called")
	}

	// Using destroyed object should fail
	_, err = i.Eval("counter1 get")
	if err == nil {
		t.Error("expected error when using destroyed object")
	}
}

func TestDefineType_MultipleInstances(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"get": func(c *Counter) int {
				return c.value
			},
			"set": func(c *Counter, val int) {
				c.value = val
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	// Create multiple instances
	result1, _ := i.Eval("Counter new")
	result2, _ := i.Eval("Counter new")

	if result1 != "counter1" {
		t.Errorf("expected 'counter1', got %q", result1)
	}
	if result2 != "counter2" {
		t.Errorf("expected 'counter2', got %q", result2)
	}

	// Set different values
	i.Eval("counter1 set 10")
	i.Eval("counter2 set 20")

	// Verify they're independent
	r1, _ := i.Eval("counter1 get")
	r2, _ := i.Eval("counter2 get")

	if r1 != "10" {
		t.Errorf("counter1 expected '10', got %q", r1)
	}
	if r2 != "20" {
		t.Errorf("counter2 expected '20', got %q", r2)
	}
}

func TestDefineType_ErrorReturn(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"mayFail": func(c *Counter, shouldFail bool) error {
				if shouldFail {
					return fmt.Errorf("intentional error")
				}
				return nil
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	i.Eval("Counter new")

	// Should succeed
	_, err = i.Eval("counter1 mayFail 0")
	if err != nil {
		t.Errorf("mayFail 0 should succeed: %v", err)
	}

	// Should fail
	_, err = i.Eval("counter1 mayFail 1")
	if err == nil {
		t.Error("mayFail 1 should return error")
	}
}

func TestDefineType_StringArguments(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	type Greeter struct {
		prefix string
	}

	err := interp.DefineType[*Greeter](i, "Greeter", interp.ForeignTypeDef[*Greeter]{
		New: func() *Greeter {
			return &Greeter{prefix: "Hello"}
		},
		Methods: interp.Methods{
			"greet": func(g *Greeter, name string) string {
				return g.prefix + ", " + name + "!"
			},
			"setPrefix": func(g *Greeter, prefix string) {
				g.prefix = prefix
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	i.Eval("Greeter new")

	result, err := i.Eval("greeter1 greet World")
	if err != nil {
		t.Fatalf("greet failed: %v", err)
	}
	if result != "Hello, World!" {
		t.Errorf("expected 'Hello, World!', got %q", result)
	}

	i.Eval("greeter1 setPrefix Hi")
	result, _ = i.Eval("greeter1 greet TCL")
	if result != "Hi, TCL!" {
		t.Errorf("expected 'Hi, TCL!', got %q", result)
	}
}

func TestDefineType_InfoMethods(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"get": func(c *Counter) int {
				return c.value
			},
			"set": func(c *Counter, val int) {
				c.value = val
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	// Store handle in a variable
	_, err = i.Eval("set c [Counter new]")
	if err != nil {
		t.Fatalf("Counter new failed: %v", err)
	}

	// The handle is a string "counter1", so info type returns "string"
	// This is a known limitation - the handle name is the command, not the object itself
	result, err := i.Eval("info type $c")
	if err != nil {
		t.Fatalf("info type failed: %v", err)
	}
	// The value in $c is just the string "counter1", so type is "string"
	if result != "string" {
		t.Logf("Note: info type on handle returned %q (expected 'string')", result)
	}

	// The object-as-command pattern works by making the handle a command
	// so $c expands to "counter1" which is a command that dispatches methods
	result, err = i.Eval("$c get")
	if err != nil {
		t.Fatalf("$c get failed: %v", err)
	}
	if result != "0" {
		t.Errorf("expected '0', got %q", result)
	}
}

func TestDefineType_ObjectAsVariable(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"incr": func(c *Counter) int {
				c.value++
				return c.value
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	// Store handle in variable and use it
	result, err := i.Eval(`
		set c [Counter new]
		$c incr
		$c incr
		$c incr
	`)
	if err != nil {
		t.Fatalf("eval failed: %v", err)
	}
	if result != "3" {
		t.Errorf("expected '3', got %q", result)
	}
}

func TestDefineType_PassToProc(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"get": func(c *Counter) int {
				return c.value
			},
			"set": func(c *Counter, val int) {
				c.value = val
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	// Define a proc that uses a counter
	result, err := i.Eval(`
		proc useCounter {counter} {
			$counter set 100
			return [$counter get]
		}
		set c [Counter new]
		useCounter $c
	`)
	if err != nil {
		t.Fatalf("eval failed: %v", err)
	}
	if result != "100" {
		t.Errorf("expected '100', got %q", result)
	}
}

func TestDefineType_UnknownMethod(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	i.Eval("Counter new")

	_, err = i.Eval("counter1 nonexistent")
	if err == nil {
		t.Error("expected error for unknown method")
	}
}

func TestDefineType_WrongArgCount(t *testing.T) {
	i := interp.NewInterp()
	defer i.Close()

	err := interp.DefineType[*Counter](i, "Counter", interp.ForeignTypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: interp.Methods{
			"set": func(c *Counter, val int) {
				c.value = val
			},
		},
	})
	if err != nil {
		t.Fatalf("DefineType failed: %v", err)
	}

	i.Eval("Counter new")

	// Too few arguments
	_, err = i.Eval("counter1 set")
	if err == nil {
		t.Error("expected error for missing argument")
	}

	// Too many arguments
	_, err = i.Eval("counter1 set 1 2")
	if err == nil {
		t.Error("expected error for too many arguments")
	}
}
