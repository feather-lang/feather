package wasm

import (
	"context"
	"testing"

	"github.com/tetratelabs/wazero/api"
)

func TestNewRuntime(t *testing.T) {
	ctx := context.Background()
	r, err := NewRuntime(ctx)
	if err != nil {
		t.Fatalf("NewRuntime failed: %v", err)
	}
	defer r.Close()
}

func TestNewInstance(t *testing.T) {
	ctx := context.Background()
	r, err := NewRuntime(ctx)
	if err != nil {
		t.Fatalf("NewRuntime failed: %v", err)
	}
	defer r.Close()

	inst, err := r.NewInstance(nil)
	if err != nil {
		t.Fatalf("NewInstance failed: %v", err)
	}
	defer inst.Close()

	if inst.ID() == 0 {
		t.Error("expected non-zero instance ID")
	}

	if inst.memory == nil {
		t.Error("expected memory to be non-nil")
	}
}

func TestInstanceMemoryHelpers(t *testing.T) {
	ctx := context.Background()
	r, err := NewRuntime(ctx)
	if err != nil {
		t.Fatalf("NewRuntime failed: %v", err)
	}
	defer r.Close()

	inst, err := r.NewInstance(nil)
	if err != nil {
		t.Fatalf("NewInstance failed: %v", err)
	}
	defer inst.Close()

	// Test memory size
	size := inst.MemorySize()
	if size == 0 {
		t.Error("expected non-zero memory size")
	}
	t.Logf("Memory size: %d bytes (%d pages)", size, size/65536)

	// Test write and read string
	testStr := "hello world"
	offset := uint32(1024) // Use a safe offset
	written := inst.WriteString(offset, testStr)
	if written != uint32(len(testStr)) {
		t.Errorf("WriteString: expected %d bytes, got %d", len(testStr), written)
	}

	readBack, ok := inst.ReadString(offset, written)
	if !ok {
		t.Fatal("ReadString failed")
	}
	if readBack != testStr {
		t.Errorf("ReadString: expected %q, got %q", testStr, readBack)
	}

	// Test null-terminated string
	offset2 := uint32(2048)
	written2 := inst.WriteStringNullTerminated(offset2, testStr)
	if written2 != uint32(len(testStr)+1) {
		t.Errorf("WriteStringNullTerminated: expected %d bytes, got %d", len(testStr)+1, written2)
	}

	readBack2, ok := inst.ReadStringNullTerminated(offset2)
	if !ok {
		t.Fatal("ReadStringNullTerminated failed")
	}
	if readBack2 != testStr {
		t.Errorf("ReadStringNullTerminated: expected %q, got %q", testStr, readBack2)
	}
}

func TestExportedFunctions(t *testing.T) {
	ctx := context.Background()
	r, err := NewRuntime(ctx)
	if err != nil {
		t.Fatalf("NewRuntime failed: %v", err)
	}
	defer r.Close()

	inst, err := r.NewInstance(nil)
	if err != nil {
		t.Fatalf("NewInstance failed: %v", err)
	}
	defer inst.Close()

	// Verify exported functions are available
	exports := map[string]api.Function{
		"tcl_script_eval":     inst.scriptEval,
		"tcl_interp_init":     inst.interpInit,
		"tcl_parse_init":      inst.parseInit,
		"tcl_parse_command":   inst.parseCommand,
		"tcl_command_exec":    inst.commandExec,
		"tcl_script_eval_obj": inst.scriptEvalObj,
		"tcl_subst":           inst.subst,
	}

	for name, fn := range exports {
		if fn == nil {
			t.Errorf("expected %s to be exported", name)
		}
	}
}
