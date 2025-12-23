# WASM Design

This document describes how to build tclc as a WebAssembly module that can be embedded in any WASM runtime, with hosts implemented in Go (via TinyGo) or Node.js.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    WASM Runtime                             │
│            (Node.js, Wasmtime, Wasmer, Browser)             │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┴─────────────────────┐
        │              imports/exports              │
        ▼                                           ▼
┌───────────────────┐                    ┌───────────────────┐
│  tclc Core        │   imports from     │  Host             │
│  (C → WASM)       │ ←───────────────── │  (JS or TinyGo)   │
│                   │                    │                   │
│  - Parser         │                    │  - Object storage │
│  - Evaluator      │                    │  - GC (automatic) │
│  - Builtins       │                    │  - Frames/vars    │
│                   │   exports to       │  - Command table  │
│  tcl_eval()  ─────│ ──────────────────→│  - Host commands  │
└───────────────────┘                    └───────────────────┘
```

## Key Design Decisions

### 1. Objects Live in the Host

`TclObj` handles are opaque integers. The C code never dereferences them—it passes them to host functions. All object storage and garbage collection happens in the host:

```
C code sees:     TclObj handle = 42
Host has:        objects[42] = { type: "string", value: "hello" }
```

This means:
- **No GC in C code** — host language's GC manages everything
- **No complex memory management** — just integer handles
- **Strings don't need to be in WASM memory** (except script input)

### 2. WASM Imports Replace TclHostOps

The current C code uses a `TclHostOps*` struct of function pointers. For WASM, these become direct imports:

```c
// Before: function pointer indirection
result = ops->string.intern(interp, s, len);

// After: WASM import
__attribute__((import_module("host"), import_name("string_intern")))
extern TclObj host_string_intern(TclInterp interp, const char* s, size_t len);

result = host_string_intern(interp, s, len);
```

### 3. Minimal WASM Memory Usage

Only these need to be in WASM linear memory:
- Script input (for parser to read)
- C string literals (error messages compiled into binary)
- Temporary buffers for C-side string building

Everything else (objects, lists, frames, variables) lives in the host.

## Host Functions (Imports)

The C WASM module imports ~35 functions from the host:

### String Operations
| Import | Signature | Description |
|--------|-----------|-------------|
| `string_intern` | `(i32, i32, i32) → i32` | interp, ptr, len → handle |
| `string_get` | `(i32, i32, i32) → i32` | interp, handle, len_ptr → ptr |
| `string_concat` | `(i32, i32, i32) → i32` | interp, a, b → handle |
| `string_compare` | `(i32, i32, i32) → i32` | interp, a, b → ordering |

### List Operations
| Import | Signature | Description |
|--------|-----------|-------------|
| `list_is_nil` | `(i32, i32) → i32` | interp, handle → bool |
| `list_create` | `(i32) → i32` | interp → handle |
| `list_from` | `(i32, i32) → i32` | interp, obj → handle |
| `list_push` | `(i32, i32, i32) → i32` | interp, list, item → list |
| `list_pop` | `(i32, i32) → i32` | interp, list → item |
| `list_shift` | `(i32, i32) → i32` | interp, list → item |
| `list_unshift` | `(i32, i32, i32) → i32` | interp, list, item → list |
| `list_length` | `(i32, i32) → i32` | interp, list → length |
| `list_at` | `(i32, i32, i32) → i32` | interp, list, index → item |

### Integer/Double Operations
| Import | Signature | Description |
|--------|-----------|-------------|
| `int_create` | `(i32, i64) → i32` | interp, value → handle |
| `int_get` | `(i32, i32, i32) → i32` | interp, handle, out_ptr → result |
| `dbl_create` | `(i32, f64) → i32` | interp, value → handle |
| `dbl_get` | `(i32, i32, i32) → i32` | interp, handle, out_ptr → result |

### Frame Operations
| Import | Signature | Description |
|--------|-----------|-------------|
| `frame_push` | `(i32, i32, i32) → i32` | interp, cmd, args → result |
| `frame_pop` | `(i32) → i32` | interp → result |
| `frame_level` | `(i32) → i32` | interp → level |
| `frame_set_active` | `(i32, i32) → i32` | interp, level → result |
| `frame_size` | `(i32) → i32` | interp → size |
| `frame_info` | `(i32, i32, i32, i32) → i32` | interp, level, cmd_ptr, args_ptr → result |

### Variable Operations
| Import | Signature | Description |
|--------|-----------|-------------|
| `var_get` | `(i32, i32) → i32` | interp, name → value |
| `var_set` | `(i32, i32, i32) → void` | interp, name, value |
| `var_unset` | `(i32, i32) → void` | interp, name |
| `var_exists` | `(i32, i32) → i32` | interp, name → result |
| `var_link` | `(i32, i32, i32, i32) → void` | interp, local, level, target |

### Proc/Command Operations
| Import | Signature | Description |
|--------|-----------|-------------|
| `proc_define` | `(i32, i32, i32, i32) → void` | interp, name, params, body |
| `proc_exists` | `(i32, i32) → i32` | interp, name → bool |
| `proc_params` | `(i32, i32, i32) → i32` | interp, name, result_ptr → result |
| `proc_body` | `(i32, i32, i32) → i32` | interp, name, result_ptr → result |
| `proc_names` | `(i32, i32) → i32` | interp, namespace → list |
| `proc_register_builtin` | `(i32, i32, i32) → void` | interp, name, fn_index |
| `proc_lookup` | `(i32, i32, i32) → i32` | interp, name, fn_ptr → cmd_type |
| `proc_rename` | `(i32, i32, i32) → i32` | interp, old, new → result |

### Interpreter State
| Import | Signature | Description |
|--------|-----------|-------------|
| `interp_set_result` | `(i32, i32) → i32` | interp, result → status |
| `interp_get_result` | `(i32) → i32` | interp → result |
| `interp_reset_result` | `(i32, i32) → i32` | interp, result → status |
| `interp_set_return_options` | `(i32, i32) → i32` | interp, options → status |
| `interp_get_return_options` | `(i32, i32) → i32` | interp, code → options |

### Unknown Handler
| Import | Signature | Description |
|--------|-----------|-------------|
| `bind_unknown` | `(i32, i32, i32, i32) → i32` | interp, cmd, args, value_ptr → result |

## WASM Module Exports

The C module exports:

| Export | Signature | Description |
|--------|-----------|-------------|
| `tcl_interp_init` | `(i32) → void` | Initialize interpreter, register builtins |
| `tcl_script_eval` | `(i32, i32, i32, i32) → i32` | interp, script_ptr, len, flags → result |
| `tcl_parse_command` | `(i32, i32) → i32` | interp, script_handle → parse_status |
| `alloc` | `(i32) → i32` | size → ptr (for host to write strings) |
| `free` | `(i32) → void` | ptr (free allocated memory) |
| `memory` | Memory | Linear memory for string passing |
| `__indirect_function_table` | Table | For function pointer dispatch |

## Host Implementation: Node.js

### Basic Structure

```javascript
async function createTclc() {
  const interpreters = new Map();
  let nextInterpId = 1;
  let wasmMemory, wasmInstance;

  // Helpers for memory access
  const readString = (ptr, len) => {
    const bytes = new Uint8Array(wasmMemory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
  };

  const writeString = (str) => {
    const bytes = new TextEncoder().encode(str);
    const ptr = wasmInstance.exports.alloc(bytes.length);
    new Uint8Array(wasmMemory.buffer, ptr, bytes.length).set(bytes);
    return [ptr, bytes.length];
  };

  // Import implementations
  const imports = {
    host: {
      string_intern(interpId, ptr, len) {
        const interp = interpreters.get(interpId);
        const str = readString(ptr, len);
        return interp.store({ type: 'string', value: str });
      },

      string_concat(interpId, a, b) {
        const interp = interpreters.get(interpId);
        const result = interp.getString(a) + interp.getString(b);
        return interp.store({ type: 'string', value: result });
      },

      list_create(interpId) {
        const interp = interpreters.get(interpId);
        return interp.store({ type: 'list', items: [] });
      },

      bind_unknown(interpId, cmdHandle, argsHandle, valuePtr) {
        const interp = interpreters.get(interpId);
        const cmdName = interp.getString(cmdHandle);
        const jsFn = interp.hostCommands.get(cmdName);

        if (!jsFn) return 1; // TCL_ERROR

        const args = interp.getListItems(argsHandle).map(h => interp.getString(h));
        try {
          const result = jsFn(args);
          const handle = interp.store({ type: 'string', value: String(result) });
          new DataView(wasmMemory.buffer).setUint32(valuePtr, handle, true);
          return 0; // TCL_OK
        } catch (e) {
          interp.result = interp.store({ type: 'string', value: e.message });
          return 1;
        }
      },

      // ... remaining ~30 functions
    }
  };

  // Load and instantiate
  const wasmBytes = await fs.readFile('./tclc.wasm');
  wasmInstance = await WebAssembly.instantiate(wasmBytes, imports);
  wasmMemory = wasmInstance.exports.memory;

  return {
    create() {
      const id = nextInterpId++;
      interpreters.set(id, new TclInterp(id));
      wasmInstance.exports.tcl_interp_init(id);
      return id;
    },

    register(interpId, name, fn) {
      interpreters.get(interpId).hostCommands.set(name, fn);
    },

    eval(interpId, script) {
      const [ptr, len] = writeString(script);
      const result = wasmInstance.exports.tcl_script_eval(interpId, ptr, len, 0);
      wasmInstance.exports.free(ptr);

      const interp = interpreters.get(interpId);
      if (result !== 0) throw new Error(interp.getString(interp.result));
      return interp.getString(interp.result);
    },

    destroy(interpId) {
      interpreters.delete(interpId);
    }
  };
}
```

### Usage

```javascript
const tclc = await createTclc();
const interp = tclc.create();

// Register JavaScript functions as TCL commands
tclc.register(interp, 'add', (args) => Number(args[0]) + Number(args[1]));
tclc.register(interp, 'fetch', async (args) => {
  const res = await fetch(args[0]);
  return await res.text();
});

// Evaluate TCL scripts
const result = tclc.eval(interp, `
  set x [add 10 20]
  expr {$x * 2}
`);
console.log(result); // "60"

tclc.destroy(interp);
```

### Function Pointer Support

For C builtins that use function pointers (via `call_indirect`), use `WebAssembly.Function`:

```javascript
// Wrap JS function for use in WASM table
const wasmFn = new WebAssembly.Function(
  { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },
  (ops, interp, cmd, args) => {
    // Implementation
    return 0; // TCL_OK
  }
);

// Add to table for call_indirect
const table = wasmInstance.exports.__indirect_function_table;
const fnIndex = table.length;
table.grow(1);
table.set(fnIndex, wasmFn);
```

## Host Implementation: Go (TinyGo)

TinyGo compiles to WASI, avoiding cgo complexity entirely.

### Why TinyGo over Standard Go

| Issue | Standard Go | TinyGo |
|-------|-------------|--------|
| Binary size | ~5-10 MB | ~200-500 KB |
| `runtime/cgo.Handle` | Required for C interop | Not needed (pure WASM) |
| WASI support | `GOOS=wasip1` (large) | Native, small |

### Architecture Options

**Option A: Two WASM Modules (Linked)**
```
┌─────────────┐      ┌─────────────┐
│ tclc.wasm   │ ←──→ │ host.wasm   │
│ (C core)    │      │ (TinyGo)    │
└─────────────┘      └─────────────┘
       ↑                    ↑
       └────────────────────┘
              Composed via WASM Component Model
              or runtime linking
```

**Option B: Single TinyGo Module (Rewrite)**

Reimplement the C core in Go, compile everything with TinyGo.

### TinyGo Host Exports

```go
package main

import "unsafe"

// Object storage
var interpreters = make(map[uint32]*Interp)
var nextInterpID uint32 = 1

//go:wasm-module host
//export string_intern
func stringIntern(interpID uint32, ptr *byte, length uint32) uint32 {
    interp := interpreters[interpID]
    str := unsafe.String(ptr, length)
    return interp.Store(&Object{Type: "string", Value: str})
}

//go:wasm-module host
//export list_create
func listCreate(interpID uint32) uint32 {
    interp := interpreters[interpID]
    return interp.Store(&Object{Type: "list", Items: []uint32{}})
}

//go:wasm-module host
//export bind_unknown
func bindUnknown(interpID, cmdHandle, argsHandle, valuePtr uint32) uint32 {
    interp := interpreters[interpID]
    cmdName := interp.GetString(cmdHandle)

    fn, ok := interp.HostCommands[cmdName]
    if !ok {
        return 1 // TCL_ERROR
    }

    args := interp.GetListStrings(argsHandle)
    result, err := fn(args)
    if err != nil {
        interp.Result = interp.Store(&Object{Type: "string", Value: err.Error()})
        return 1
    }

    handle := interp.Store(&Object{Type: "string", Value: result})
    *(*uint32)(unsafe.Pointer(uintptr(valuePtr))) = handle
    return 0 // TCL_OK
}

// ... remaining exports
```

### Module Composition

Using `wasm-tools compose`:

```bash
# Compile C core
clang --target=wasm32-wasi -o tclc-core.wasm src/tclc.c

# Compile TinyGo host
tinygo build -target=wasi -o tclc-host.wasm ./host

# Compose into single module
wasm-tools compose tclc-core.wasm -d tclc-host.wasm -o tclc.wasm
```

Or runtime linking in the embedding environment.

## C Code Changes Required

### 1. Conditional Compilation

```c
#ifdef TCLC_WASM
  // Use WASM imports
  #define HOST_STRING_INTERN(interp, s, len) host_string_intern(interp, s, len)
#else
  // Use function pointer struct
  #define HOST_STRING_INTERN(interp, s, len) ops->string.intern(interp, s, len)
#endif
```

### 2. Import Declarations

```c
#ifdef TCLC_WASM

__attribute__((import_module("host"), import_name("string_intern")))
extern TclObj host_string_intern(TclInterp interp, const char* s, size_t len);

__attribute__((import_module("host"), import_name("string_get")))
extern const char* host_string_get(TclInterp interp, TclObj obj, size_t* len);

__attribute__((import_module("host"), import_name("list_create")))
extern TclObj host_list_create(TclInterp interp);

// ... all other imports

#endif
```

### 3. Memory Allocator Export

```c
// Simple bump allocator for string passing
static char heap[65536];
static size_t heap_pos = 0;

__attribute__((export_name("alloc")))
void* wasm_alloc(size_t size) {
    void* ptr = &heap[heap_pos];
    heap_pos += size;
    return ptr;
}

__attribute__((export_name("free")))
void wasm_free(void* ptr) {
    // No-op for bump allocator, or implement properly
}
```

## Build Process

### Compiling C to WASM

```bash
# Using clang with WASI SDK
clang \
  --target=wasm32-wasi \
  --sysroot=/path/to/wasi-sdk/share/wasi-sysroot \
  -DTCLC_WASM \
  -O2 \
  -o tclc.wasm \
  src/tclc.c
```

### Build Targets

| Target | Command | Output |
|--------|---------|--------|
| WASM (standalone) | `clang --target=wasm32-wasi` | `tclc.wasm` |
| Native (with Go host) | `go build ./cmd/gcl` | `gcl` binary |
| TinyGo host | `tinygo build -target=wasi` | `host.wasm` |

## Comparison Summary

| Aspect | Node.js Host | TinyGo Host |
|--------|--------------|-------------|
| Binary size | ~50KB (C only) | ~250KB (C + TinyGo) |
| GC | JavaScript GC | TinyGo GC |
| Async support | Native Promises | Manual |
| Deployment | Single .wasm + JS | Single .wasm or composed |
| Dev experience | Familiar JS | Type-safe Go |

## Future Considerations

1. **Component Model**: As WASM Component Model matures, module composition becomes standardized
2. **WasmGC**: If C core were rewritten in a GC language, could use WasmGC for tighter integration
3. **Shared-nothing linking**: Multiple tclc instances could run in isolated memory spaces
4. **WASI Preview 2**: Future WASI versions may simplify async I/O patterns
