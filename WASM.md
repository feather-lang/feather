# WASM Design

This document describes how to build feather as a WebAssembly module that can be embedded in any WASM runtime. The design uses direct WASM imports for host functions, providing a simple and portable interface.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    WASM Runtime                             │
│         (Node.js, Python/wasmer, Wasmtime, Browser)         │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┴─────────────────────┐
        │              WASM imports                 │
        ▼                                           ▼
┌───────────────────┐                    ┌───────────────────┐
│  feather.wasm     │                    │  Host             │
│  (C → WASM)       │                    │  (JS or Python)   │
│                   │                    │                   │
│  extern functions │──── provided by ──→│  feather_host_*   │
│  (100 imports)    │                    │  implementations  │
│                   │                    │                   │
│  feather_script_eval  │←─── called by ─────│  Host code    │
└───────────────────┘                    └───────────────────┘
```

## Key Design Decisions

### 1. Import-Based Host Interface

Host functions are declared as `extern` in C and become WASM imports. The host provides implementations in its imports object during instantiation:

```javascript
const imports = {
    env: {
        feather_host_frame_push: (interp, cmd, args) => { /* ... */ },
        feather_host_string_intern: (interp, ptr, len) => { /* ... */ },
        // ... all 100 host functions
        memory: wasmMemory,
    }
};
const instance = await WebAssembly.instantiate(module, imports);
```

**Benefits:**
- Works in all browsers without experimental flags
- No runtime WASM code generation
- Simple, declarative host interface
- Universal WASM compatibility

### 2. Objects Live in the Host

`FeatherObj` handles are opaque integers. The C code never dereferences them—it passes them to host functions. All object storage and garbage collection happens in the host:

```
C code sees:     FeatherObj handle = 42
Host has:        objects[42] = { type: "string", value: "hello" }
```

This means:
- **No GC in C code** — host language's GC manages everything
- **No complex memory management** — just integer handles
- **Strings passed to C** go in WASM linear memory temporarily

### 3. NULL ops for Default Behavior

Public API functions accept `NULL` for the `ops` parameter:

```javascript
// Pass 0 (NULL) for ops - uses import-based host functions
instance.exports.feather_script_eval(0, interpId, srcPtr, len, 0);
```

For WASM builds, `feather_get_ops(NULL)` returns a static `default_ops` struct populated with the `feather_host_*` function pointers that resolve to WASM imports.

### 4. Native Build Compatibility

Native builds can still provide a custom `FeatherHostOps` struct:

```c
// Native host provides all callbacks
FeatherHostOps ops = {
    .frame = { .push = my_frame_push, ... },
    .string = { .intern = my_string_intern, ... },
    // ...
};
feather_script_eval(&ops, interp, src, len, 0);
```

The same C source compiles for both native and WASM targets.

## WASM Module Interface

### Exports

| Export | Signature | Description |
|--------|-----------|-------------|
| `memory` | Memory | Linear memory (imported, growable) |
| `feather_interp_init` | `(ops: i32, interp: i32) → void` | Initialize with host ops (pass 0) |
| `feather_script_eval` | `(ops: i32, interp: i32, src: i32, len: i32, flags: i32) → i32` | Evaluate script |
| `feather_script_eval_obj` | `(ops: i32, interp: i32, script: i32, flags: i32) → i32` | Evaluate script object |
| `feather_command_exec` | `(ops: i32, interp: i32, cmd: i32, flags: i32) → i32` | Execute parsed command |
| `feather_parse_init` | `(ops: i32, interp: i32, src: i32, len: i32) → void` | Initialize parser |
| `feather_parse_command` | `(ops: i32, interp: i32, cmd: i32) → i32` | Parse next command |
| `feather_subst` | `(ops: i32, interp: i32, obj: i32, flags: i32, result: i32) → i32` | Perform substitution |
| `feather_get_ops` | `(ops: i32) → i32` | Get ops (returns default_ops if NULL) |
| `alloc` | `(size: i32) → i32` | Allocate WASM memory |
| `free` | `(ptr: i32) → void` | Free WASM memory |

### Imports (100 functions in `env` namespace)

The host must provide implementations for all `feather_host_*` functions. See [src/host.h](src/host.h) for the complete list with signatures.

Functions are grouped by category:

| Category | Count | Example |
|----------|-------|---------|
| Frame | 8 | `feather_host_frame_push` |
| Variable | 7 | `feather_host_var_get` |
| Proc | 9 | `feather_host_proc_define` |
| Namespace | 18 | `feather_host_ns_create` |
| String | 5 | `feather_host_string_intern` |
| Rune | 6 | `feather_host_rune_length` |
| List | 13 | `feather_host_list_push` |
| Dict | 10 | `feather_host_dict_get` |
| Integer | 2 | `feather_host_integer_create` |
| Double | 5 | `feather_host_dbl_create`, `feather_host_dbl_classify`, `feather_host_dbl_format`, `feather_host_dbl_math` |
| Interp | 7 | `feather_host_interp_get_result` |
| Bind | 1 | `feather_host_bind_unknown` |
| Trace | 3 | `feather_host_trace_add` |
| Foreign | 6 | `feather_host_foreign_invoke` |

## Host Implementation: JavaScript

See [js/feather.js](js/feather.js) for a complete implementation.

### Minimal Example

```javascript
import { readFileSync } from 'fs';

// Object store for interpreter
const interpreters = new Map();
let nextInterpId = 1;

// WASM memory (imported by the module)
const wasmMemory = new WebAssembly.Memory({ initial: 16, maximum: 256 });

// Memory helpers
const readString = (ptr, len) => {
    const bytes = new Uint8Array(wasmMemory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
};

const writeString = (str, allocFn) => {
    const bytes = new TextEncoder().encode(str);
    const ptr = allocFn(bytes.length + 1);
    new Uint8Array(wasmMemory.buffer, ptr, bytes.length).set(bytes);
    new Uint8Array(wasmMemory.buffer)[ptr + bytes.length] = 0;
    return [ptr, bytes.length];
};

// Host function implementations
const hostImports = {
    feather_host_string_intern: (interpId, ptr, len) => {
        const interp = interpreters.get(interpId);
        const str = readString(ptr, len);
        return interp.store({ type: 'string', value: str });
    },

    feather_host_string_get: (interpId, handle, lenPtr) => {
        const interp = interpreters.get(interpId);
        const str = interp.getString(handle);
        const bytes = new TextEncoder().encode(str);
        const ptr = wasmInstance.exports.alloc(bytes.length + 1);
        new Uint8Array(wasmMemory.buffer, ptr, bytes.length).set(bytes);
        new Uint8Array(wasmMemory.buffer)[ptr + bytes.length] = 0;
        new DataView(wasmMemory.buffer).setUint32(lenPtr, bytes.length, true);
        return ptr;
    },

    feather_host_interp_set_result: (interpId, result) => {
        interpreters.get(interpId).result = result;
        return 0;
    },

    feather_host_interp_get_result: (interpId) => {
        return interpreters.get(interpId).result;
    },

    // ... implement all 97 functions
};

// Load and instantiate
const wasmBytes = readFileSync('./feather.wasm');
const wasmModule = await WebAssembly.compile(wasmBytes);
const wasmInstance = await WebAssembly.instantiate(wasmModule, {
    env: { memory: wasmMemory, ...hostImports }
});

// Create interpreter
const interpId = nextInterpId++;
interpreters.set(interpId, new Interp(interpId));
wasmInstance.exports.feather_interp_init(0, interpId);  // 0 = NULL ops

// Evaluate script
const [srcPtr, srcLen] = writeString('set x 42', wasmInstance.exports.alloc);
const result = wasmInstance.exports.feather_script_eval(0, interpId, srcPtr, srcLen, 0);
wasmInstance.exports.free(srcPtr);
```

## Host Implementation: Python (wasmer)

```python
from wasmer import engine, Store, Module, Instance, ImportObject, Function, Memory, MemoryType
import struct

class Interp:
    def __init__(self, id):
        self.id = id
        self.objects = {}
        self.next_handle = 1
        self.result = 0
        self.frames = [{'vars': {}, 'cmd': 0, 'args': 0, 'ns': '::'}]

    def store(self, obj):
        handle = self.next_handle
        self.next_handle += 1
        self.objects[handle] = obj
        return handle

    def get_string(self, handle):
        if handle == 0:
            return ''
        obj = self.objects.get(handle)
        if isinstance(obj, dict) and obj.get('type') == 'string':
            return obj['value']
        return str(obj) if obj else ''

class FeatherHost:
    def __init__(self, wasm_path):
        self.store = Store()
        self.interpreters = {}
        self.next_interp_id = 1

        # Create memory
        self.memory = Memory(self.store, MemoryType(minimum=16, maximum=256))

        # Build imports
        import_object = ImportObject()
        import_object.register("env", {
            "memory": self.memory,
            "feather_host_string_intern": Function(self.store, self._string_intern),
            "feather_host_string_get": Function(self.store, self._string_get),
            "feather_host_interp_set_result": Function(self.store, self._interp_set_result),
            "feather_host_interp_get_result": Function(self.store, self._interp_get_result),
            # ... all 97 functions
        })

        # Load module
        with open(wasm_path, 'rb') as f:
            wasm_bytes = f.read()
        module = Module(self.store, wasm_bytes)
        self.instance = Instance(module, import_object)

    def _string_intern(self, interp_id, ptr, length):
        interp = self.interpreters[interp_id]
        data = bytes(self.memory.uint8_view(ptr)[:length])
        s = data.decode('utf-8')
        return interp.store({'type': 'string', 'value': s})

    def create(self):
        interp_id = self.next_interp_id
        self.next_interp_id += 1
        self.interpreters[interp_id] = Interp(interp_id)
        self.instance.exports.feather_interp_init(0, interp_id)  # 0 = NULL ops
        return interp_id

    def eval(self, interp_id, script):
        # Write script to memory
        encoded = script.encode('utf-8')
        ptr = self.instance.exports.alloc(len(encoded) + 1)
        view = self.memory.uint8_view(ptr)
        for i, b in enumerate(encoded):
            view[i] = b
        view[len(encoded)] = 0

        # Evaluate
        result = self.instance.exports.feather_script_eval(0, interp_id, ptr, len(encoded), 0)
        self.instance.exports.free(ptr)

        interp = self.interpreters[interp_id]
        if result != 0:
            raise RuntimeError(interp.get_string(interp.result))
        return interp.get_string(interp.result)

# Usage
feather = FeatherHost('./feather.wasm')
interp = feather.create()
print(feather.eval(interp, 'expr {2 + 2}'))  # "4"
```

## Build Process

### Building with Zig

The JS host uses Zig's WASM linker:

```bash
cd js
mise run build
```

This compiles all C sources with `-DFEATHER_WASM_BUILD` and links with `--allow-undefined` to treat `feather_host_*` functions as imports.

### Build Flags

| Flag | Purpose |
|------|---------|
| `-DFEATHER_WASM_BUILD` | Enable WASM-specific code paths |
| `--allow-undefined` | Treat extern functions as imports |
| `--import-memory` | Import memory from host |
| `--export=<name>` | Export public API functions |

### Verifying Imports

```bash
wasm-objdump -x feather.wasm | grep -A 200 "Import\["
```

Should show all `feather_host_*` functions in the `env` module.

## Floating-Point Support

The double operations provide IEEE 754 compliant floating-point support:

| Function | Purpose |
|----------|---------|
| `feather_host_dbl_create` | Create a double object |
| `feather_host_dbl_get` | Extract double value from object |
| `feather_host_dbl_classify` | Detect special values (Inf, -Inf, NaN, Zero, Normal) |
| `feather_host_dbl_format` | Format double to string with specifier (%e, %f, %g) |
| `feather_host_dbl_math` | Transcendental math operations (sin, cos, sqrt, pow, etc.) |

The `classify` operation returns a `FeatherDoubleClass` enum:

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | `FEATHER_DBL_NORMAL` | Finite, non-zero |
| 1 | `FEATHER_DBL_ZERO` | Positive or negative zero |
| 2 | `FEATHER_DBL_INF` | Positive infinity |
| 3 | `FEATHER_DBL_NEG_INF` | Negative infinity |
| 4 | `FEATHER_DBL_NAN` | Not a number |

The `math` operation uses a `FeatherMathOp` enum to select the operation. Unary operations use only the `a` parameter; binary operations use both `a` and `b`:

**Unary:** `sqrt`, `exp`, `log`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `floor`, `ceil`, `round`, `abs`

**Binary:** `pow`, `atan2`, `fmod`, `hypot`

## Known Limitations

The import-based approach cannot call WASM function pointers directly, which affects:

1. **lsort with custom comparison**: `-command` option for custom sort functions doesn't work
2. **Callback-based APIs**: Any host operation expecting a WASM callback

These are acceptable tradeoffs for the simplification benefits (no runtime WASM generation, universal browser support).

## Comparison of Approaches

| Aspect | WASM Imports (current) | Function Table | Component Model |
|--------|------------------------|----------------|-----------------|
| C code changes | Minimal | Minimal | Heavy (WIT bindings) |
| Browser support | **Yes** | Yes (needs flags) | No |
| Node.js support | **Yes** | Yes (needs flags) | No |
| Runtime code gen | **None** | Required | None |
| Host complexity | Low | High | Low (auto-marshal) |
| Callback support | No | Yes | Yes |

**Import-based approach is the right choice for feather because:**

1. **Universal compatibility** — works in all browsers and Node.js without experimental flags
2. **Simplicity** — host just provides an imports object, no trampolines or table manipulation
3. **No runtime WASM generation** — safer, faster startup
4. **Easy to understand** — straightforward mapping from C extern to JS function
