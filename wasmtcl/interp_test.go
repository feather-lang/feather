package wasmtcl

import (
	"testing"
)

func TestInterpBasics(t *testing.T) {
	// Create a mock instance (nil for now - we test Interp in isolation)
	interp := &Interp{
		instance: nil, // Not using WASM calls in this test
		objects:  make(map[TclObj]*Object),
		commands: make(map[string]*Command),
		nextID:   1,
	}

	// Initialize frames
	globalFrame := &CallFrame{
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: 0,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0
	interp.globalNS = interp.internString("::")

	// Test InternString
	h := interp.InternString("hello")
	if h == 0 {
		t.Fatal("InternString returned 0")
	}

	// Test GetString
	s := interp.GetString(h)
	if s != "hello" {
		t.Errorf("GetString: expected %q, got %q", "hello", s)
	}

	// Test CreateInt
	intH := interp.CreateInt(42)
	val, err := interp.GetInt(intH)
	if err != nil {
		t.Fatalf("GetInt failed: %v", err)
	}
	if val != 42 {
		t.Errorf("GetInt: expected 42, got %d", val)
	}

	// Test int->string shimmering
	intStr := interp.GetString(intH)
	if intStr != "42" {
		t.Errorf("int->string shimmer: expected %q, got %q", "42", intStr)
	}

	// Test CreateDouble
	dblH := interp.CreateDouble(3.14)
	dblVal, err := interp.GetDouble(dblH)
	if err != nil {
		t.Fatalf("GetDouble failed: %v", err)
	}
	if dblVal != 3.14 {
		t.Errorf("GetDouble: expected 3.14, got %f", dblVal)
	}

	// Test CreateList and list operations
	listH := interp.CreateList()
	if interp.ListLength(listH) != 0 {
		t.Error("new list should be empty")
	}

	item1 := interp.InternString("one")
	item2 := interp.InternString("two")
	interp.ListPush(listH, item1)
	interp.ListPush(listH, item2)

	if interp.ListLength(listH) != 2 {
		t.Errorf("list length: expected 2, got %d", interp.ListLength(listH))
	}

	at0 := interp.ListAt(listH, 0)
	if interp.GetString(at0) != "one" {
		t.Errorf("ListAt(0): expected %q, got %q", "one", interp.GetString(at0))
	}

	// Test list->string shimmering
	listStr := interp.GetString(listH)
	if listStr != "one two" {
		t.Errorf("list->string shimmer: expected %q, got %q", "one two", listStr)
	}
}

func TestInterpVariables(t *testing.T) {
	interp := &Interp{
		instance: nil,
		objects:  make(map[TclObj]*Object),
		commands: make(map[string]*Command),
		nextID:   1,
	}

	globalFrame := &CallFrame{
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: 0,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0

	// Test SetVar/GetVar
	interp.SetVar("x", "hello")
	if interp.GetVar("x") != "hello" {
		t.Errorf("GetVar: expected %q, got %q", "hello", interp.GetVar("x"))
	}

	// Test VarExists
	xName := interp.InternString("x")
	if !interp.VarExists(xName) {
		t.Error("VarExists(x) should be true")
	}

	yName := interp.InternString("y")
	if interp.VarExists(yName) {
		t.Error("VarExists(y) should be false")
	}
}

func TestInterpFrames(t *testing.T) {
	interp := &Interp{
		instance: nil,
		objects:  make(map[TclObj]*Object),
		commands: make(map[string]*Command),
		nextID:   1,
	}

	globalFrame := &CallFrame{
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: 0,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0

	// Initial state
	if interp.FrameLevel() != 0 {
		t.Errorf("initial level: expected 0, got %d", interp.FrameLevel())
	}
	if interp.FrameSize() != 1 {
		t.Errorf("initial size: expected 1, got %d", interp.FrameSize())
	}

	// Push frame
	cmd := interp.InternString("test")
	args := interp.CreateList()
	result := interp.FramePush(cmd, args)
	if result != ResultOK {
		t.Error("FramePush failed")
	}

	if interp.FrameLevel() != 1 {
		t.Errorf("after push level: expected 1, got %d", interp.FrameLevel())
	}
	if interp.FrameSize() != 2 {
		t.Errorf("after push size: expected 2, got %d", interp.FrameSize())
	}

	// Pop frame
	result = interp.FramePop()
	if result != ResultOK {
		t.Error("FramePop failed")
	}

	if interp.FrameLevel() != 0 {
		t.Errorf("after pop level: expected 0, got %d", interp.FrameLevel())
	}
}

func TestInterpProcedures(t *testing.T) {
	interp := &Interp{
		instance: nil,
		objects:  make(map[TclObj]*Object),
		commands: make(map[string]*Command),
		nextID:   1,
	}

	globalFrame := &CallFrame{
		vars:  make(map[string]TclObj),
		links: make(map[string]varLink),
		level: 0,
	}
	interp.frames = []*CallFrame{globalFrame}
	interp.active = 0

	// Register a builtin
	setName := interp.InternString("set")
	interp.RegisterCommand(setName)

	cmdType, canonical := interp.CommandLookup(setName)
	if cmdType != CmdBuiltin {
		t.Errorf("expected CmdBuiltin, got %d", cmdType)
	}
	if canonical != "set" {
		t.Errorf("expected canonical %q, got %q", "set", canonical)
	}

	// Define a proc
	procName := interp.InternString("myproc")
	params := interp.InternString("x y")
	body := interp.InternString("expr {$x + $y}")
	interp.ProcDefine(procName, params, body)

	if !interp.ProcExists(procName) {
		t.Error("ProcExists should be true after define")
	}

	gotParams, ok := interp.ProcParams(procName)
	if !ok {
		t.Error("ProcParams failed")
	}
	if interp.GetString(gotParams) != "x y" {
		t.Errorf("ProcParams: expected %q, got %q", "x y", interp.GetString(gotParams))
	}
}
