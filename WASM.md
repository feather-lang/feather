# WASM Design

This document describes how to build tclc as a WebAssembly module that can be embedded in any WASM runtime. The design preserves the existing `FeatherHostOps` interface unchanged, using WASM's function table mechanism for indirect calls.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    WASM Runtime                             │
│         (Node.js, Python/wasmer, Wasmtime, Browser)         │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┴─────────────────────┐
        │           function table + memory         │
        ▼                                           ▼
┌───────────────────┐                    ┌───────────────────┐
│  tclc.wasm        │                    │  Host             │
│  (C → WASM)       │                    │  (JS or Python)   │
│                   │                    │                   │
│  FeatherHostOps*  ────│──── points to ────→│  Function table   │
│  (unchanged)      │                    │  indices          │
│                   │                    │                   │
│  call_indirect ───│──── dispatches ───→│  Host functions   │
│                   │                    │                   │
│  feather_script_eval  │←─── called by ─────│  Host code        │
└───────────────────┘                    └───────────────────┘
```

## Key Design Decisions

### 1. FeatherHostOps Struct Remains Unchanged

The C code continues to use `FeatherHostOps*` with function pointers. When compiled to WASM:

- Function pointers become indices into the WASM function table
- `call_indirect` dispatches through these indices
- The host populates the table with its implementations

```c
// This C code works identically in native and WASM builds:
result = ops->string.intern(interp, s, len);

// In WASM, this compiles to:
//   i32.load (get function index from struct)
//   call_indirect (dispatch through table)
```

**No conditional compilation. No `#ifdef TCLC_WASM`. The same C source builds for both targets.**

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

### 3. Host Responsibilities

The host must:
1. Load the WASM module
2. Create functions matching `FeatherHostOps` signatures
3. Add those functions to the WASM function table
4. Allocate `FeatherHostOps` struct in WASM linear memory
5. Write function table indices into the struct
6. Pass the struct pointer to `feather_interp_init`

### 4. Why Function Tables, Not Component Model

The WASM Component Model with WIT (WebAssembly Interface Types) would simplify host implementations by auto-marshaling strings, lists, and records. However, as of late 2025:

| Environment | Component Model Support |
|-------------|------------------------|
| Wasmtime | Full support |
| NGINX Unit | Partial (wasi:http) |
| Wasmer | Catching up |
| Node.js | **Not supported** |
| Browsers | **Not supported** |

The Component Model is production-ready for server-side runtimes (Wasmtime, cloud functions) but **not yet available in browsers or Node.js**, which are primary targets for tclc.

**Function tables provide:**
- Universal compatibility (works everywhere WASM runs)
- No C code changes (same source for native and WASM)
- Runtime flexibility (can modify functions after load)
- Battle-tested mechanism (used since WASM 1.0)

**Trade-off:** Hosts must manually marshal data (strings via ptr/len, structs laid out in memory). This is more code but ensures tclc works in any WASM environment today.

**Future option:** When Component Model reaches browsers, a WIT interface could be added alongside the function-table approach for hosts that support it.

## WASM Module Exports

The compiled `tclc.wasm` exports:

| Export | Signature | Description |
|--------|-----------|-------------|
| `memory` | Memory | Linear memory (64KB pages) |
| `__indirect_function_table` | Table | Function table for `call_indirect` |
| `feather_interp_init` | `(ops: i32, interp: i32) → void` | Initialize with host ops |
| `feather_script_eval` | `(ops: i32, interp: i32, src: i32, len: i32, flags: i32) → i32` | Evaluate script |
| `feather_script_eval_obj` | `(ops: i32, interp: i32, script: i32, flags: i32) → i32` | Evaluate script object |
| `feather_command_exec` | `(ops: i32, interp: i32, cmd: i32, flags: i32) → i32` | Execute parsed command |
| `alloc` | `(size: i32) → i32` | Allocate WASM memory |
| `free` | `(ptr: i32) → void` | Free WASM memory |

## FeatherHostOps Memory Layout

The host must lay out the struct in WASM memory exactly as C expects. Each function pointer is a 4-byte table index (i32).

```
Offset  Field                          Type
──────  ─────                          ────
Frame operations (FeatherFrameOps):
0x000   frame.push                     i32 (table index)
0x004   frame.pop                      i32
0x008   frame.level                    i32
0x00C   frame.set_active               i32
0x010   frame.size                     i32
0x014   frame.info                     i32
0x018   frame.set_namespace            i32
0x01C   frame.get_namespace            i32

Variable operations (FeatherVarOps):
0x020   var.get                        i32
0x024   var.set                        i32
0x028   var.unset                      i32
0x02C   var.exists                     i32
0x030   var.link                       i32
0x034   var.link_ns                    i32
0x038   var.names                      i32

Proc operations (FeatherProcOps):
0x03C   proc.define                    i32
0x040   proc.exists                    i32
0x044   proc.params                    i32
0x048   proc.body                      i32
0x04C   proc.names                     i32
0x050   proc.resolve_namespace         i32
0x054   proc.register_builtin          i32
0x058   proc.lookup                    i32
0x05C   proc.rename                    i32

Namespace operations (FeatherNamespaceOps):
0x060   ns.create                      i32
0x064   ns.delete                      i32
0x068   ns.exists                      i32
0x06C   ns.current                     i32
0x070   ns.parent                      i32
0x074   ns.children                    i32
0x078   ns.get_var                     i32
0x07C   ns.set_var                     i32
0x080   ns.var_exists                  i32
0x084   ns.unset_var                   i32

String operations (FeatherStringOps):
0x088   string.intern                  i32
0x08C   string.get                     i32
0x090   string.concat                  i32
0x094   string.compare                 i32

List operations (FeatherListOps):
0x098   list.is_nil                    i32
0x09C   list.create                    i32
0x0A0   list.from                      i32
0x0A4   list.push                      i32
0x0A8   list.pop                       i32
0x0AC   list.unshift                   i32
0x0B0   list.shift                     i32
0x0B4   list.length                    i32
0x0B8   list.at                        i32

Integer operations (FeatherIntOps):
0x0BC   integer.create                 i32
0x0C0   integer.get                    i32

Double operations (FeatherDoubleOps):
0x0C4   dbl.create                     i32
0x0C8   dbl.get                        i32

Interpreter operations (FeatherInterpOps):
0x0CC   interp.set_result              i32
0x0D0   interp.get_result              i32
0x0D4   interp.reset_result            i32
0x0D8   interp.set_return_options      i32
0x0DC   interp.get_return_options      i32
0x0E0   interp.get_script              i32
0x0E4   interp.set_script              i32

Bind operations (FeatherBindOps):
0x0E8   bind.unknown                   i32

Trace operations (FeatherTraceOps):
0x0EC   trace.add                      i32
0x0F0   trace.remove                   i32
0x0F4   trace.info                     i32

Total size: 0x0F8 (248 bytes)
```

## Function Signatures

Each host function must match the expected WASM signature for `call_indirect` type checking.

### Frame Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `frame.push` | `(i32, i32, i32) → i32` | `(interp, cmd, args) → result` |
| `frame.pop` | `(i32) → i32` | `(interp) → result` |
| `frame.level` | `(i32) → i32` | `(interp) → level` |
| `frame.set_active` | `(i32, i32) → i32` | `(interp, level) → result` |
| `frame.size` | `(i32) → i32` | `(interp) → size` |
| `frame.info` | `(i32, i32, i32, i32, i32) → i32` | `(interp, level, cmd*, args*, ns*) → result` |
| `frame.set_namespace` | `(i32, i32) → i32` | `(interp, ns) → result` |
| `frame.get_namespace` | `(i32) → i32` | `(interp) → ns` |

### Variable Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `var.get` | `(i32, i32) → i32` | `(interp, name) → value` |
| `var.set` | `(i32, i32, i32) → void` | `(interp, name, value)` |
| `var.unset` | `(i32, i32) → void` | `(interp, name)` |
| `var.exists` | `(i32, i32) → i32` | `(interp, name) → result` |
| `var.link` | `(i32, i32, i32, i32) → void` | `(interp, local, level, target)` |
| `var.link_ns` | `(i32, i32, i32, i32) → void` | `(interp, local, ns, name)` |
| `var.names` | `(i32, i32) → i32` | `(interp, ns) → list` |

### String Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `string.intern` | `(i32, i32, i32) → i32` | `(interp, ptr, len) → handle` |
| `string.get` | `(i32, i32, i32) → i32` | `(interp, handle, len*) → ptr` |
| `string.concat` | `(i32, i32, i32) → i32` | `(interp, a, b) → handle` |
| `string.compare` | `(i32, i32, i32) → i32` | `(interp, a, b) → ordering` |

### List Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `list.is_nil` | `(i32, i32) → i32` | `(interp, obj) → bool` |
| `list.create` | `(i32) → i32` | `(interp) → handle` |
| `list.from` | `(i32, i32) → i32` | `(interp, obj) → handle` |
| `list.push` | `(i32, i32, i32) → i32` | `(interp, list, item) → list` |
| `list.pop` | `(i32, i32) → i32` | `(interp, list) → item` |
| `list.unshift` | `(i32, i32, i32) → i32` | `(interp, list, item) → list` |
| `list.shift` | `(i32, i32) → i32` | `(interp, list) → item` |
| `list.length` | `(i32, i32) → i32` | `(interp, list) → length` |
| `list.at` | `(i32, i32, i32) → i32` | `(interp, list, index) → item` |

### Integer/Double Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `integer.create` | `(i32, i64) → i32` | `(interp, value) → handle` |
| `integer.get` | `(i32, i32, i32) → i32` | `(interp, handle, out*) → result` |
| `dbl.create` | `(i32, f64) → i32` | `(interp, value) → handle` |
| `dbl.get` | `(i32, i32, i32) → i32` | `(interp, handle, out*) → result` |

### Proc Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `proc.define` | `(i32, i32, i32, i32) → void` | `(interp, name, params, body)` |
| `proc.exists` | `(i32, i32) → i32` | `(interp, name) → bool` |
| `proc.params` | `(i32, i32, i32) → i32` | `(interp, name, result*) → result` |
| `proc.body` | `(i32, i32, i32) → i32` | `(interp, name, result*) → result` |
| `proc.names` | `(i32, i32) → i32` | `(interp, ns) → list` |
| `proc.resolve_namespace` | `(i32, i32, i32) → i32` | `(interp, path, result*) → result` |
| `proc.register_builtin` | `(i32, i32, i32) → void` | `(interp, name, fn)` |
| `proc.lookup` | `(i32, i32, i32) → i32` | `(interp, name, fn*) → cmd_type` |
| `proc.rename` | `(i32, i32, i32) → i32` | `(interp, old, new) → result` |

### Namespace Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `ns.create` | `(i32, i32) → i32` | `(interp, path) → result` |
| `ns.delete` | `(i32, i32) → i32` | `(interp, path) → result` |
| `ns.exists` | `(i32, i32) → i32` | `(interp, path) → bool` |
| `ns.current` | `(i32) → i32` | `(interp) → path` |
| `ns.parent` | `(i32, i32, i32) → i32` | `(interp, ns, result*) → result` |
| `ns.children` | `(i32, i32) → i32` | `(interp) → list` |
| `ns.get_var` | `(i32, i32, i32) → i32` | `(interp, ns, name) → value` |
| `ns.set_var` | `(i32, i32, i32, i32) → void` | `(interp, ns, name, value)` |
| `ns.var_exists` | `(i32, i32, i32) → i32` | `(interp, ns, name) → bool` |
| `ns.unset_var` | `(i32, i32, i32) → void` | `(interp, ns, name)` |

### Interpreter Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `interp.set_result` | `(i32, i32) → i32` | `(interp, result) → status` |
| `interp.get_result` | `(i32) → i32` | `(interp) → result` |
| `interp.reset_result` | `(i32, i32) → i32` | `(interp, result) → status` |
| `interp.set_return_options` | `(i32, i32) → i32` | `(interp, options) → status` |
| `interp.get_return_options` | `(i32, i32) → i32` | `(interp, code) → options` |
| `interp.get_script` | `(i32) → i32` | `(interp) → path` |
| `interp.set_script` | `(i32, i32) → void` | `(interp, path)` |

### Bind Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `bind.unknown` | `(i32, i32, i32, i32) → i32` | `(interp, cmd, args, value*) → result` |

### Trace Operations
| Function | WASM Signature | C Signature |
|----------|---------------|-------------|
| `trace.add` | `(i32, i32, i32, i32, i32) → i32` | `(interp, kind, name, ops, script) → result` |
| `trace.remove` | `(i32, i32, i32, i32, i32) → i32` | `(interp, kind, name, ops, script) → result` |
| `trace.info` | `(i32, i32, i32) → i32` | `(interp, kind, name) → list` |

## Host Implementation: Node.js

### Complete Example

```javascript
import { readFileSync } from 'fs';

class FeatherInterp {
  constructor(id) {
    this.id = id;
    this.objects = new Map();
    this.nextHandle = 1;
    this.result = 0;
    this.frames = [{ vars: new Map(), cmd: 0, args: 0, ns: '::' }];
    this.procs = new Map();
    this.builtins = new Map();
    this.hostCommands = new Map();
  }

  store(obj) {
    const handle = this.nextHandle++;
    this.objects.set(handle, obj);
    return handle;
  }

  get(handle) {
    return this.objects.get(handle);
  }

  getString(handle) {
    if (handle === 0) return '';
    const obj = this.get(handle);
    if (!obj) return '';
    if (typeof obj === 'string') return obj;
    if (obj.type === 'string') return obj.value;
    if (obj.type === 'int') return String(obj.value);
    if (obj.type === 'list') return obj.items.map(h => this.getString(h)).join(' ');
    return String(obj);
  }

  getList(handle) {
    if (handle === 0) return [];
    const obj = this.get(handle);
    if (!obj) return [];
    if (obj.type === 'list') return obj.items;
    // Parse string as list
    return this.getString(handle).split(/\s+/).filter(s => s);
  }
}

async function createFeatherc(wasmPath) {
  const interpreters = new Map();
  let nextInterpId = 1;
  let wasmMemory;
  let wasmTable;
  let wasmInstance;
  let heapPtr = 0;

  // Memory helpers
  const readString = (ptr, len) => {
    const bytes = new Uint8Array(wasmMemory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
  };

  const writeString = (str) => {
    const bytes = new TextEncoder().encode(str);
    const ptr = wasmInstance.exports.alloc(bytes.length + 1);
    new Uint8Array(wasmMemory.buffer, ptr, bytes.length).set(bytes);
    new Uint8Array(wasmMemory.buffer)[ptr + bytes.length] = 0;
    return [ptr, bytes.length];
  };

  const writeI32 = (ptr, value) => {
    new DataView(wasmMemory.buffer).setInt32(ptr, value, true);
  };

  const readI32 = (ptr) => {
    return new DataView(wasmMemory.buffer).getInt32(ptr, true);
  };

  const writeI64 = (ptr, value) => {
    new DataView(wasmMemory.buffer).setBigInt64(ptr, BigInt(value), true);
  };

  const readI64 = (ptr) => {
    return Number(new DataView(wasmMemory.buffer).getBigInt64(ptr, true));
  };

  // Add function to table and return its index
  const addToTable = (fn, signature) => {
    const wasmFn = new WebAssembly.Function(signature, fn);
    const index = wasmTable.length;
    wasmTable.grow(1);
    wasmTable.set(index, wasmFn);
    return index;
  };

  // Host function implementations
  const hostFunctions = {
    // Frame operations
    frame_push: (interpId, cmd, args) => {
      const interp = interpreters.get(interpId);
      interp.frames.push({ vars: new Map(), cmd, args, ns: interp.frames[interp.frames.length - 1].ns });
      return 0;
    },
    frame_pop: (interpId) => {
      const interp = interpreters.get(interpId);
      if (interp.frames.length > 1) interp.frames.pop();
      return 0;
    },
    frame_level: (interpId) => {
      return interpreters.get(interpId).frames.length - 1;
    },
    frame_set_active: (interpId, level) => {
      // Simplified: just validate level
      const interp = interpreters.get(interpId);
      if (level >= interp.frames.length) return 1;
      return 0;
    },
    frame_size: (interpId) => {
      return interpreters.get(interpId).frames.length;
    },
    frame_info: (interpId, level, cmdPtr, argsPtr, nsPtr) => {
      const interp = interpreters.get(interpId);
      if (level >= interp.frames.length) return 1;
      const frame = interp.frames[level];
      writeI32(cmdPtr, frame.cmd);
      writeI32(argsPtr, frame.args);
      const nsHandle = interp.store({ type: 'string', value: frame.ns });
      writeI32(nsPtr, nsHandle);
      return 0;
    },
    frame_set_namespace: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      interp.frames[interp.frames.length - 1].ns = interp.getString(ns);
      return 0;
    },
    frame_get_namespace: (interpId) => {
      const interp = interpreters.get(interpId);
      const ns = interp.frames[interp.frames.length - 1].ns;
      return interp.store({ type: 'string', value: ns });
    },

    // Variable operations
    var_get: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.frames[interp.frames.length - 1];
      return frame.vars.get(varName) || 0;
    },
    var_set: (interpId, name, value) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.frames[interp.frames.length - 1];
      frame.vars.set(varName, value);
    },
    var_unset: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.frames[interp.frames.length - 1];
      frame.vars.delete(varName);
    },
    var_exists: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.frames[interp.frames.length - 1];
      return frame.vars.has(varName) ? 0 : 1;
    },
    var_link: (interpId, local, targetLevel, target) => {
      // Simplified: create alias
      const interp = interpreters.get(interpId);
      const localName = interp.getString(local);
      const targetName = interp.getString(target);
      const frame = interp.frames[interp.frames.length - 1];
      const targetFrame = interp.frames[targetLevel];
      // Store link info
      frame.vars.set(localName, { link: { level: targetLevel, name: targetName } });
    },
    var_link_ns: (interpId, local, ns, name) => {
      // Simplified implementation
    },
    var_names: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const frame = interp.frames[interp.frames.length - 1];
      const names = [...frame.vars.keys()];
      const list = { type: 'list', items: names.map(n => interp.store({ type: 'string', value: n })) };
      return interp.store(list);
    },

    // String operations
    string_intern: (interpId, ptr, len) => {
      const interp = interpreters.get(interpId);
      const str = readString(ptr, len);
      return interp.store({ type: 'string', value: str });
    },
    string_get: (interpId, handle, lenPtr) => {
      const interp = interpreters.get(interpId);
      const str = interp.getString(handle);
      const [ptr, len] = writeString(str);
      writeI32(lenPtr, len);
      return ptr;
    },
    string_concat: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const result = interp.getString(a) + interp.getString(b);
      return interp.store({ type: 'string', value: result });
    },
    string_compare: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const strA = interp.getString(a);
      const strB = interp.getString(b);
      return strA < strB ? -1 : strA > strB ? 1 : 0;
    },

    // List operations
    list_is_nil: (interpId, obj) => obj === 0 ? 1 : 0,
    list_create: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'list', items: [] });
    },
    list_from: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const items = interp.getList(obj);
      return interp.store({ type: 'list', items: [...items] });
    },
    list_push: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj && listObj.type === 'list') {
        listObj.items.push(item);
      }
      return list;
    },
    list_pop: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj && listObj.type === 'list' && listObj.items.length > 0) {
        return listObj.items.pop();
      }
      return 0;
    },
    list_unshift: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj && listObj.type === 'list') {
        listObj.items.unshift(item);
      }
      return list;
    },
    list_shift: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj && listObj.type === 'list' && listObj.items.length > 0) {
        return listObj.items.shift();
      }
      return 0;
    },
    list_length: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj && listObj.type === 'list') {
        return listObj.items.length;
      }
      return 0;
    },
    list_at: (interpId, list, index) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj && listObj.type === 'list' && index < listObj.items.length) {
        return listObj.items[index];
      }
      return 0;
    },

    // Integer operations
    int_create: (interpId, value) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'int', value: Number(value) });
    },
    int_get: (interpId, handle, outPtr) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(handle);
      if (obj && obj.type === 'int') {
        writeI64(outPtr, obj.value);
        return 0;
      }
      // Try parsing string
      const str = interp.getString(handle);
      const num = parseInt(str, 10);
      if (!isNaN(num)) {
        writeI64(outPtr, num);
        return 0;
      }
      return 1;
    },

    // Double operations
    dbl_create: (interpId, value) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'double', value });
    },
    dbl_get: (interpId, handle, outPtr) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(handle);
      if (obj && obj.type === 'double') {
        new DataView(wasmMemory.buffer).setFloat64(outPtr, obj.value, true);
        return 0;
      }
      const str = interp.getString(handle);
      const num = parseFloat(str);
      if (!isNaN(num)) {
        new DataView(wasmMemory.buffer).setFloat64(outPtr, num, true);
        return 0;
      }
      return 1;
    },

    // Proc operations
    proc_define: (interpId, name, params, body) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      interp.procs.set(procName, { params, body });
    },
    proc_exists: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      return interp.procs.has(procName) ? 1 : 0;
    },
    proc_params: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      const proc = interp.procs.get(procName);
      if (proc) {
        writeI32(resultPtr, proc.params);
        return 0;
      }
      return 1;
    },
    proc_body: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      const proc = interp.procs.get(procName);
      if (proc) {
        writeI32(resultPtr, proc.body);
        return 0;
      }
      return 1;
    },
    proc_names: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const names = [...interp.procs.keys(), ...interp.builtins.keys()];
      const list = { type: 'list', items: names.map(n => interp.store({ type: 'string', value: n })) };
      return interp.store(list);
    },
    proc_resolve_namespace: (interpId, path, resultPtr) => {
      const interp = interpreters.get(interpId);
      writeI32(resultPtr, path || interp.store({ type: 'string', value: '::' }));
      return 0;
    },
    proc_register_builtin: (interpId, name, fn) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      interp.builtins.set(procName, fn);
    },
    proc_lookup: (interpId, name, fnPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      if (interp.builtins.has(procName)) {
        writeI32(fnPtr, interp.builtins.get(procName));
        return 1; // TCL_CMD_BUILTIN
      }
      if (interp.procs.has(procName)) {
        writeI32(fnPtr, 0);
        return 2; // TCL_CMD_PROC
      }
      writeI32(fnPtr, 0);
      return 0; // TCL_CMD_NONE
    },
    proc_rename: (interpId, oldName, newName) => {
      const interp = interpreters.get(interpId);
      const oldN = interp.getString(oldName);
      const newN = interp.getString(newName);
      if (interp.procs.has(oldN)) {
        const proc = interp.procs.get(oldN);
        interp.procs.delete(oldN);
        if (newN) interp.procs.set(newN, proc);
        return 0;
      }
      return 1;
    },

    // Namespace operations (simplified)
    ns_create: (interpId, path) => 0,
    ns_delete: (interpId, path) => 0,
    ns_exists: (interpId, path) => 1,
    ns_current: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: '::' });
    },
    ns_parent: (interpId, ns, resultPtr) => {
      const interp = interpreters.get(interpId);
      writeI32(resultPtr, interp.store({ type: 'string', value: '' }));
      return 0;
    },
    ns_children: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'list', items: [] });
    },
    ns_get_var: (interpId, ns, name) => 0,
    ns_set_var: (interpId, ns, name, value) => {},
    ns_var_exists: (interpId, ns, name) => 0,
    ns_unset_var: (interpId, ns, name) => {},

    // Interpreter operations
    interp_set_result: (interpId, result) => {
      interpreters.get(interpId).result = result;
      return 0;
    },
    interp_get_result: (interpId) => {
      return interpreters.get(interpId).result;
    },
    interp_reset_result: (interpId, result) => {
      const interp = interpreters.get(interpId);
      interp.result = result;
      return 0;
    },
    interp_set_return_options: (interpId, options) => 0,
    interp_get_return_options: (interpId, code) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'list', items: [] });
    },
    interp_get_script: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: '' });
    },
    interp_set_script: (interpId, path) => {},

    // Bind operations
    bind_unknown: (interpId, cmd, args, valuePtr) => {
      const interp = interpreters.get(interpId);
      const cmdName = interp.getString(cmd);
      const hostFn = interp.hostCommands.get(cmdName);
      if (!hostFn) return 1; // TCL_ERROR

      const argList = interp.getList(args).map(h => interp.getString(h));
      try {
        const result = hostFn(argList);
        const handle = interp.store({ type: 'string', value: String(result) });
        writeI32(valuePtr, handle);
        return 0;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: e.message });
        return 1;
      }
    },

    // Trace operations (simplified)
    trace_add: (interpId, kind, name, ops, script) => 0,
    trace_remove: (interpId, kind, name, ops, script) => 0,
    trace_info: (interpId, kind, name) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'list', items: [] });
    },
  };

  // Function signatures for WebAssembly.Function
  const signatures = {
    frame_push: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    frame_pop: { parameters: ['i32'], results: ['i32'] },
    frame_level: { parameters: ['i32'], results: ['i32'] },
    frame_set_active: { parameters: ['i32', 'i32'], results: ['i32'] },
    frame_size: { parameters: ['i32'], results: ['i32'] },
    frame_info: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    frame_set_namespace: { parameters: ['i32', 'i32'], results: ['i32'] },
    frame_get_namespace: { parameters: ['i32'], results: ['i32'] },

    var_get: { parameters: ['i32', 'i32'], results: ['i32'] },
    var_set: { parameters: ['i32', 'i32', 'i32'], results: [] },
    var_unset: { parameters: ['i32', 'i32'], results: [] },
    var_exists: { parameters: ['i32', 'i32'], results: ['i32'] },
    var_link: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    var_link_ns: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    var_names: { parameters: ['i32', 'i32'], results: ['i32'] },

    string_intern: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_concat: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_compare: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    list_is_nil: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_create: { parameters: ['i32'], results: ['i32'] },
    list_from: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_push: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    list_pop: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_unshift: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    list_shift: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_length: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_at: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    int_create: { parameters: ['i32', 'i64'], results: ['i32'] },
    int_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    dbl_create: { parameters: ['i32', 'f64'], results: ['i32'] },
    dbl_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    proc_define: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    proc_exists: { parameters: ['i32', 'i32'], results: ['i32'] },
    proc_params: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_body: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_names: { parameters: ['i32', 'i32'], results: ['i32'] },
    proc_resolve_namespace: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_register_builtin: { parameters: ['i32', 'i32', 'i32'], results: [] },
    proc_lookup: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_rename: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    ns_create: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_delete: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_exists: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_current: { parameters: ['i32'], results: ['i32'] },
    ns_parent: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_children: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_get_var: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_set_var: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    ns_var_exists: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_unset_var: { parameters: ['i32', 'i32', 'i32'], results: [] },

    interp_set_result: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_get_result: { parameters: ['i32'], results: ['i32'] },
    interp_reset_result: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_set_return_options: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_get_return_options: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_get_script: { parameters: ['i32'], results: ['i32'] },
    interp_set_script: { parameters: ['i32', 'i32'], results: [] },

    bind_unknown: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },

    trace_add: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    trace_remove: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    trace_info: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
  };

  // Load WASM module
  const wasmBytes = readFileSync(wasmPath);
  const wasmModule = await WebAssembly.compile(wasmBytes);

  // Create initial table (will be grown as needed)
  wasmTable = new WebAssembly.Table({ initial: 1, element: 'anyfunc' });
  wasmMemory = new WebAssembly.Memory({ initial: 16 }); // 1MB

  // Instantiate with minimal imports (table and memory)
  wasmInstance = await WebAssembly.instantiate(wasmModule, {
    env: {
      memory: wasmMemory,
      __indirect_function_table: wasmTable,
    }
  });

  // Get exports
  wasmMemory = wasmInstance.exports.memory || wasmMemory;
  wasmTable = wasmInstance.exports.__indirect_function_table || wasmTable;

  // Build FeatherHostOps struct and populate table
  const buildHostOps = () => {
    const STRUCT_SIZE = 0x0F8; // 248 bytes
    const opsPtr = wasmInstance.exports.alloc(STRUCT_SIZE);

    // Order must match FeatherHostOps struct layout
    const fields = [
      // FeatherFrameOps
      'frame_push', 'frame_pop', 'frame_level', 'frame_set_active',
      'frame_size', 'frame_info', 'frame_set_namespace', 'frame_get_namespace',
      // FeatherVarOps
      'var_get', 'var_set', 'var_unset', 'var_exists', 'var_link', 'var_link_ns', 'var_names',
      // FeatherProcOps
      'proc_define', 'proc_exists', 'proc_params', 'proc_body', 'proc_names',
      'proc_resolve_namespace', 'proc_register_builtin', 'proc_lookup', 'proc_rename',
      // FeatherNamespaceOps
      'ns_create', 'ns_delete', 'ns_exists', 'ns_current', 'ns_parent', 'ns_children',
      'ns_get_var', 'ns_set_var', 'ns_var_exists', 'ns_unset_var',
      // FeatherStringOps
      'string_intern', 'string_get', 'string_concat', 'string_compare',
      // FeatherListOps
      'list_is_nil', 'list_create', 'list_from', 'list_push', 'list_pop',
      'list_unshift', 'list_shift', 'list_length', 'list_at',
      // FeatherIntOps
      'int_create', 'int_get',
      // FeatherDoubleOps
      'dbl_create', 'dbl_get',
      // FeatherInterpOps
      'interp_set_result', 'interp_get_result', 'interp_reset_result',
      'interp_set_return_options', 'interp_get_return_options',
      'interp_get_script', 'interp_set_script',
      // FeatherBindOps
      'bind_unknown',
      // FeatherTraceOps
      'trace_add', 'trace_remove', 'trace_info',
    ];

    let offset = 0;
    for (const name of fields) {
      const fn = hostFunctions[name];
      const sig = signatures[name];
      const index = addToTable(fn, sig);
      writeI32(opsPtr + offset, index);
      offset += 4;
    }

    return opsPtr;
  };

  const opsPtr = buildHostOps();

  return {
    create() {
      const id = nextInterpId++;
      interpreters.set(id, new FeatherInterp(id));
      wasmInstance.exports.feather_interp_init(opsPtr, id);
      return id;
    },

    register(interpId, name, fn) {
      interpreters.get(interpId).hostCommands.set(name, fn);
    },

    eval(interpId, script) {
      const [ptr, len] = writeString(script);
      const result = wasmInstance.exports.feather_script_eval(opsPtr, interpId, ptr, len, 0);
      wasmInstance.exports.free(ptr);

      const interp = interpreters.get(interpId);
      if (result !== 0) {
        throw new Error(interp.getString(interp.result));
      }
      return interp.getString(interp.result);
    },

    getResult(interpId) {
      const interp = interpreters.get(interpId);
      return interp.getString(interp.result);
    },

    destroy(interpId) {
      interpreters.delete(interpId);
    }
  };
}

// Usage example
async function main() {
  const tclc = await createFeatherc('./tclc.wasm');
  const interp = tclc.create();

  // Register host commands
  tclc.register(interp, 'add', (args) => Number(args[0]) + Number(args[1]));
  tclc.register(interp, 'multiply', (args) => Number(args[0]) * Number(args[1]));
  tclc.register(interp, 'greet', (args) => `Hello, ${args[0]}!`);

  // Evaluate TCL scripts
  console.log(tclc.eval(interp, 'set x 10'));           // "10"
  console.log(tclc.eval(interp, 'set y [add $x 5]'));   // "15"
  console.log(tclc.eval(interp, 'expr {$x * $y}'));     // "150"
  console.log(tclc.eval(interp, 'greet World'));        // "Hello, World!"

  tclc.destroy(interp);
}

export { createFeatherc };
```

## Host Implementation: Python

Using the `wasmer` package for WASM runtime:

```python
#!/usr/bin/env python3
"""
tclc WASM host implementation in Python using wasmer.

Install: pip install wasmer wasmer-compiler-cranelift
"""

from wasmer import Store, Module, Instance, Memory, Table, Function, FunctionType, Type
from wasmer import engine, wasi
import struct


class FeatherInterp:
    """Per-interpreter state."""

    def __init__(self, interp_id):
        self.id = interp_id
        self.objects = {}
        self.next_handle = 1
        self.result = 0
        self.frames = [{'vars': {}, 'cmd': 0, 'args': 0, 'ns': '::'}]
        self.procs = {}
        self.builtins = {}
        self.host_commands = {}

    def store(self, obj):
        handle = self.next_handle
        self.next_handle += 1
        self.objects[handle] = obj
        return handle

    def get(self, handle):
        return self.objects.get(handle)

    def get_string(self, handle):
        if handle == 0:
            return ''
        obj = self.get(handle)
        if obj is None:
            return ''
        if isinstance(obj, str):
            return obj
        if isinstance(obj, dict):
            if obj.get('type') == 'string':
                return obj['value']
            if obj.get('type') == 'int':
                return str(obj['value'])
            if obj.get('type') == 'list':
                return ' '.join(self.get_string(h) for h in obj['items'])
        return str(obj)

    def get_list(self, handle):
        if handle == 0:
            return []
        obj = self.get(handle)
        if obj is None:
            return []
        if isinstance(obj, dict) and obj.get('type') == 'list':
            return obj['items']
        # Parse string as list
        s = self.get_string(handle)
        return [x for x in s.split() if x]


class FeathercHost:
    """WASM host for tclc interpreter."""

    def __init__(self, wasm_path):
        self.interpreters = {}
        self.next_interp_id = 1
        self.store = Store()
        self.memory = None
        self.table = None
        self.instance = None
        self.ops_ptr = None

        self._load_wasm(wasm_path)

    def _load_wasm(self, wasm_path):
        with open(wasm_path, 'rb') as f:
            wasm_bytes = f.read()

        module = Module(self.store, wasm_bytes)
        self.instance = Instance(module)

        # Get exports
        self.memory = self.instance.exports.memory
        self.table = self.instance.exports.__indirect_function_table

        # Build host ops struct
        self.ops_ptr = self._build_host_ops()

    def _read_string(self, ptr, length):
        """Read UTF-8 string from WASM memory."""
        data = self.memory.uint8_view(ptr)
        return bytes(data[0:length]).decode('utf-8')

    def _write_string(self, s):
        """Write UTF-8 string to WASM memory, return (ptr, len)."""
        data = s.encode('utf-8')
        ptr = self.instance.exports.alloc(len(data) + 1)
        view = self.memory.uint8_view(ptr)
        for i, b in enumerate(data):
            view[i] = b
        view[len(data)] = 0
        return ptr, len(data)

    def _write_i32(self, ptr, value):
        """Write i32 to WASM memory."""
        view = self.memory.uint8_view(ptr)
        packed = struct.pack('<i', value)
        for i, b in enumerate(packed):
            view[i] = b

    def _read_i32(self, ptr):
        """Read i32 from WASM memory."""
        view = self.memory.uint8_view(ptr)
        return struct.unpack('<i', bytes(view[0:4]))[0]

    def _write_i64(self, ptr, value):
        """Write i64 to WASM memory."""
        view = self.memory.uint8_view(ptr)
        packed = struct.pack('<q', value)
        for i, b in enumerate(packed):
            view[i] = b

    def _read_i64(self, ptr):
        """Read i64 from WASM memory."""
        view = self.memory.uint8_view(ptr)
        return struct.unpack('<q', bytes(view[0:8]))[0]

    def _add_to_table(self, fn, param_types, result_types):
        """Add function to WASM table, return index."""
        func_type = FunctionType(param_types, result_types)
        wasm_fn = Function(self.store, fn, func_type)
        index = self.table.size
        self.table.grow(1)
        self.table.set(index, wasm_fn)
        return index

    def _build_host_ops(self):
        """Build FeatherHostOps struct in WASM memory."""
        STRUCT_SIZE = 0x0F8  # 248 bytes
        ops_ptr = self.instance.exports.alloc(STRUCT_SIZE)

        # Define all host functions
        functions = self._create_host_functions()

        # Write function indices to struct
        offset = 0
        for name, (fn, params, results) in functions.items():
            index = self._add_to_table(fn, params, results)
            self._write_i32(ops_ptr + offset, index)
            offset += 4

        return ops_ptr

    def _create_host_functions(self):
        """Create all host function implementations."""
        host = self
        I32 = Type.I32
        I64 = Type.I64
        F64 = Type.F64

        def frame_push(interp_id, cmd, args):
            interp = host.interpreters[interp_id]
            parent_ns = interp.frames[-1]['ns']
            interp.frames.append({'vars': {}, 'cmd': cmd, 'args': args, 'ns': parent_ns})
            return 0

        def frame_pop(interp_id):
            interp = host.interpreters[interp_id]
            if len(interp.frames) > 1:
                interp.frames.pop()
            return 0

        def frame_level(interp_id):
            return len(host.interpreters[interp_id].frames) - 1

        def frame_set_active(interp_id, level):
            interp = host.interpreters[interp_id]
            return 1 if level >= len(interp.frames) else 0

        def frame_size(interp_id):
            return len(host.interpreters[interp_id].frames)

        def frame_info(interp_id, level, cmd_ptr, args_ptr, ns_ptr):
            interp = host.interpreters[interp_id]
            if level >= len(interp.frames):
                return 1
            frame = interp.frames[level]
            host._write_i32(cmd_ptr, frame['cmd'])
            host._write_i32(args_ptr, frame['args'])
            ns_handle = interp.store({'type': 'string', 'value': frame['ns']})
            host._write_i32(ns_ptr, ns_handle)
            return 0

        def frame_set_namespace(interp_id, ns):
            interp = host.interpreters[interp_id]
            interp.frames[-1]['ns'] = interp.get_string(ns)
            return 0

        def frame_get_namespace(interp_id):
            interp = host.interpreters[interp_id]
            ns = interp.frames[-1]['ns']
            return interp.store({'type': 'string', 'value': ns})

        def var_get(interp_id, name):
            interp = host.interpreters[interp_id]
            var_name = interp.get_string(name)
            frame = interp.frames[-1]
            return frame['vars'].get(var_name, 0)

        def var_set(interp_id, name, value):
            interp = host.interpreters[interp_id]
            var_name = interp.get_string(name)
            frame = interp.frames[-1]
            frame['vars'][var_name] = value

        def var_unset(interp_id, name):
            interp = host.interpreters[interp_id]
            var_name = interp.get_string(name)
            frame = interp.frames[-1]
            frame['vars'].pop(var_name, None)

        def var_exists(interp_id, name):
            interp = host.interpreters[interp_id]
            var_name = interp.get_string(name)
            frame = interp.frames[-1]
            return 0 if var_name in frame['vars'] else 1

        def var_link(interp_id, local, target_level, target):
            pass  # Simplified

        def var_link_ns(interp_id, local, ns, name):
            pass  # Simplified

        def var_names(interp_id, ns):
            interp = host.interpreters[interp_id]
            frame = interp.frames[-1]
            names = list(frame['vars'].keys())
            items = [interp.store({'type': 'string', 'value': n}) for n in names]
            return interp.store({'type': 'list', 'items': items})

        def string_intern(interp_id, ptr, length):
            interp = host.interpreters[interp_id]
            s = host._read_string(ptr, length)
            return interp.store({'type': 'string', 'value': s})

        def string_get(interp_id, handle, len_ptr):
            interp = host.interpreters[interp_id]
            s = interp.get_string(handle)
            ptr, length = host._write_string(s)
            host._write_i32(len_ptr, length)
            return ptr

        def string_concat(interp_id, a, b):
            interp = host.interpreters[interp_id]
            result = interp.get_string(a) + interp.get_string(b)
            return interp.store({'type': 'string', 'value': result})

        def string_compare(interp_id, a, b):
            interp = host.interpreters[interp_id]
            str_a = interp.get_string(a)
            str_b = interp.get_string(b)
            if str_a < str_b:
                return -1
            elif str_a > str_b:
                return 1
            return 0

        def list_is_nil(interp_id, obj):
            return 1 if obj == 0 else 0

        def list_create(interp_id):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'list', 'items': []})

        def list_from(interp_id, obj):
            interp = host.interpreters[interp_id]
            items = list(interp.get_list(obj))
            return interp.store({'type': 'list', 'items': items})

        def list_push(interp_id, lst, item):
            interp = host.interpreters[interp_id]
            list_obj = interp.get(lst)
            if list_obj and list_obj.get('type') == 'list':
                list_obj['items'].append(item)
            return lst

        def list_pop(interp_id, lst):
            interp = host.interpreters[interp_id]
            list_obj = interp.get(lst)
            if list_obj and list_obj.get('type') == 'list' and list_obj['items']:
                return list_obj['items'].pop()
            return 0

        def list_unshift(interp_id, lst, item):
            interp = host.interpreters[interp_id]
            list_obj = interp.get(lst)
            if list_obj and list_obj.get('type') == 'list':
                list_obj['items'].insert(0, item)
            return lst

        def list_shift(interp_id, lst):
            interp = host.interpreters[interp_id]
            list_obj = interp.get(lst)
            if list_obj and list_obj.get('type') == 'list' and list_obj['items']:
                return list_obj['items'].pop(0)
            return 0

        def list_length(interp_id, lst):
            interp = host.interpreters[interp_id]
            list_obj = interp.get(lst)
            if list_obj and list_obj.get('type') == 'list':
                return len(list_obj['items'])
            return 0

        def list_at(interp_id, lst, index):
            interp = host.interpreters[interp_id]
            list_obj = interp.get(lst)
            if list_obj and list_obj.get('type') == 'list':
                if index < len(list_obj['items']):
                    return list_obj['items'][index]
            return 0

        def int_create(interp_id, value):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'int', 'value': value})

        def int_get(interp_id, handle, out_ptr):
            interp = host.interpreters[interp_id]
            obj = interp.get(handle)
            if obj and obj.get('type') == 'int':
                host._write_i64(out_ptr, obj['value'])
                return 0
            # Try parsing string
            s = interp.get_string(handle)
            try:
                host._write_i64(out_ptr, int(s))
                return 0
            except ValueError:
                return 1

        def dbl_create(interp_id, value):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'double', 'value': value})

        def dbl_get(interp_id, handle, out_ptr):
            interp = host.interpreters[interp_id]
            obj = interp.get(handle)
            if obj and obj.get('type') == 'double':
                view = host.memory.uint8_view(out_ptr)
                packed = struct.pack('<d', obj['value'])
                for i, b in enumerate(packed):
                    view[i] = b
                return 0
            s = interp.get_string(handle)
            try:
                view = host.memory.uint8_view(out_ptr)
                packed = struct.pack('<d', float(s))
                for i, b in enumerate(packed):
                    view[i] = b
                return 0
            except ValueError:
                return 1

        def proc_define(interp_id, name, params, body):
            interp = host.interpreters[interp_id]
            proc_name = interp.get_string(name)
            interp.procs[proc_name] = {'params': params, 'body': body}

        def proc_exists(interp_id, name):
            interp = host.interpreters[interp_id]
            proc_name = interp.get_string(name)
            return 1 if proc_name in interp.procs else 0

        def proc_params(interp_id, name, result_ptr):
            interp = host.interpreters[interp_id]
            proc_name = interp.get_string(name)
            proc = interp.procs.get(proc_name)
            if proc:
                host._write_i32(result_ptr, proc['params'])
                return 0
            return 1

        def proc_body(interp_id, name, result_ptr):
            interp = host.interpreters[interp_id]
            proc_name = interp.get_string(name)
            proc = interp.procs.get(proc_name)
            if proc:
                host._write_i32(result_ptr, proc['body'])
                return 0
            return 1

        def proc_names(interp_id, ns):
            interp = host.interpreters[interp_id]
            names = list(interp.procs.keys()) + list(interp.builtins.keys())
            items = [interp.store({'type': 'string', 'value': n}) for n in names]
            return interp.store({'type': 'list', 'items': items})

        def proc_resolve_namespace(interp_id, path, result_ptr):
            interp = host.interpreters[interp_id]
            if path:
                host._write_i32(result_ptr, path)
            else:
                host._write_i32(result_ptr, interp.store({'type': 'string', 'value': '::'}))
            return 0

        def proc_register_builtin(interp_id, name, fn):
            interp = host.interpreters[interp_id]
            proc_name = interp.get_string(name)
            interp.builtins[proc_name] = fn

        def proc_lookup(interp_id, name, fn_ptr):
            interp = host.interpreters[interp_id]
            proc_name = interp.get_string(name)
            if proc_name in interp.builtins:
                host._write_i32(fn_ptr, interp.builtins[proc_name])
                return 1  # TCL_CMD_BUILTIN
            if proc_name in interp.procs:
                host._write_i32(fn_ptr, 0)
                return 2  # TCL_CMD_PROC
            host._write_i32(fn_ptr, 0)
            return 0  # TCL_CMD_NONE

        def proc_rename(interp_id, old_name, new_name):
            interp = host.interpreters[interp_id]
            old_n = interp.get_string(old_name)
            new_n = interp.get_string(new_name)
            if old_n in interp.procs:
                proc = interp.procs.pop(old_n)
                if new_n:
                    interp.procs[new_n] = proc
                return 0
            return 1

        # Namespace operations (simplified)
        def ns_create(interp_id, path):
            return 0

        def ns_delete(interp_id, path):
            return 0

        def ns_exists(interp_id, path):
            return 1

        def ns_current(interp_id):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'string', 'value': '::'})

        def ns_parent(interp_id, ns, result_ptr):
            interp = host.interpreters[interp_id]
            host._write_i32(result_ptr, interp.store({'type': 'string', 'value': ''}))
            return 0

        def ns_children(interp_id, ns):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'list', 'items': []})

        def ns_get_var(interp_id, ns, name):
            return 0

        def ns_set_var(interp_id, ns, name, value):
            pass

        def ns_var_exists(interp_id, ns, name):
            return 0

        def ns_unset_var(interp_id, ns, name):
            pass

        # Interpreter operations
        def interp_set_result(interp_id, result):
            host.interpreters[interp_id].result = result
            return 0

        def interp_get_result(interp_id):
            return host.interpreters[interp_id].result

        def interp_reset_result(interp_id, result):
            host.interpreters[interp_id].result = result
            return 0

        def interp_set_return_options(interp_id, options):
            return 0

        def interp_get_return_options(interp_id, code):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'list', 'items': []})

        def interp_get_script(interp_id):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'string', 'value': ''})

        def interp_set_script(interp_id, path):
            pass

        # Bind operations
        def bind_unknown(interp_id, cmd, args, value_ptr):
            interp = host.interpreters[interp_id]
            cmd_name = interp.get_string(cmd)
            host_fn = interp.host_commands.get(cmd_name)
            if not host_fn:
                return 1  # TCL_ERROR

            arg_list = [interp.get_string(h) for h in interp.get_list(args)]
            try:
                result = host_fn(arg_list)
                handle = interp.store({'type': 'string', 'value': str(result)})
                host._write_i32(value_ptr, handle)
                return 0
            except Exception as e:
                interp.result = interp.store({'type': 'string', 'value': str(e)})
                return 1

        # Trace operations (simplified)
        def trace_add(interp_id, kind, name, ops, script):
            return 0

        def trace_remove(interp_id, kind, name, ops, script):
            return 0

        def trace_info(interp_id, kind, name):
            interp = host.interpreters[interp_id]
            return interp.store({'type': 'list', 'items': []})

        # Return ordered dict matching struct layout
        return {
            # FeatherFrameOps
            'frame_push': (frame_push, [I32, I32, I32], [I32]),
            'frame_pop': (frame_pop, [I32], [I32]),
            'frame_level': (frame_level, [I32], [I32]),
            'frame_set_active': (frame_set_active, [I32, I32], [I32]),
            'frame_size': (frame_size, [I32], [I32]),
            'frame_info': (frame_info, [I32, I32, I32, I32, I32], [I32]),
            'frame_set_namespace': (frame_set_namespace, [I32, I32], [I32]),
            'frame_get_namespace': (frame_get_namespace, [I32], [I32]),
            # FeatherVarOps
            'var_get': (var_get, [I32, I32], [I32]),
            'var_set': (var_set, [I32, I32, I32], []),
            'var_unset': (var_unset, [I32, I32], []),
            'var_exists': (var_exists, [I32, I32], [I32]),
            'var_link': (var_link, [I32, I32, I32, I32], []),
            'var_link_ns': (var_link_ns, [I32, I32, I32, I32], []),
            'var_names': (var_names, [I32, I32], [I32]),
            # FeatherProcOps
            'proc_define': (proc_define, [I32, I32, I32, I32], []),
            'proc_exists': (proc_exists, [I32, I32], [I32]),
            'proc_params': (proc_params, [I32, I32, I32], [I32]),
            'proc_body': (proc_body, [I32, I32, I32], [I32]),
            'proc_names': (proc_names, [I32, I32], [I32]),
            'proc_resolve_namespace': (proc_resolve_namespace, [I32, I32, I32], [I32]),
            'proc_register_builtin': (proc_register_builtin, [I32, I32, I32], []),
            'proc_lookup': (proc_lookup, [I32, I32, I32], [I32]),
            'proc_rename': (proc_rename, [I32, I32, I32], [I32]),
            # FeatherNamespaceOps
            'ns_create': (ns_create, [I32, I32], [I32]),
            'ns_delete': (ns_delete, [I32, I32], [I32]),
            'ns_exists': (ns_exists, [I32, I32], [I32]),
            'ns_current': (ns_current, [I32], [I32]),
            'ns_parent': (ns_parent, [I32, I32, I32], [I32]),
            'ns_children': (ns_children, [I32, I32], [I32]),
            'ns_get_var': (ns_get_var, [I32, I32, I32], [I32]),
            'ns_set_var': (ns_set_var, [I32, I32, I32, I32], []),
            'ns_var_exists': (ns_var_exists, [I32, I32, I32], [I32]),
            'ns_unset_var': (ns_unset_var, [I32, I32, I32], []),
            # FeatherStringOps
            'string_intern': (string_intern, [I32, I32, I32], [I32]),
            'string_get': (string_get, [I32, I32, I32], [I32]),
            'string_concat': (string_concat, [I32, I32, I32], [I32]),
            'string_compare': (string_compare, [I32, I32, I32], [I32]),
            # FeatherListOps
            'list_is_nil': (list_is_nil, [I32, I32], [I32]),
            'list_create': (list_create, [I32], [I32]),
            'list_from': (list_from, [I32, I32], [I32]),
            'list_push': (list_push, [I32, I32, I32], [I32]),
            'list_pop': (list_pop, [I32, I32], [I32]),
            'list_unshift': (list_unshift, [I32, I32, I32], [I32]),
            'list_shift': (list_shift, [I32, I32], [I32]),
            'list_length': (list_length, [I32, I32], [I32]),
            'list_at': (list_at, [I32, I32, I32], [I32]),
            # FeatherIntOps
            'int_create': (int_create, [I32, I64], [I32]),
            'int_get': (int_get, [I32, I32, I32], [I32]),
            # FeatherDoubleOps
            'dbl_create': (dbl_create, [I32, F64], [I32]),
            'dbl_get': (dbl_get, [I32, I32, I32], [I32]),
            # FeatherInterpOps
            'interp_set_result': (interp_set_result, [I32, I32], [I32]),
            'interp_get_result': (interp_get_result, [I32], [I32]),
            'interp_reset_result': (interp_reset_result, [I32, I32], [I32]),
            'interp_set_return_options': (interp_set_return_options, [I32, I32], [I32]),
            'interp_get_return_options': (interp_get_return_options, [I32, I32], [I32]),
            'interp_get_script': (interp_get_script, [I32], [I32]),
            'interp_set_script': (interp_set_script, [I32, I32], []),
            # FeatherBindOps
            'bind_unknown': (bind_unknown, [I32, I32, I32, I32], [I32]),
            # FeatherTraceOps
            'trace_add': (trace_add, [I32, I32, I32, I32, I32], [I32]),
            'trace_remove': (trace_remove, [I32, I32, I32, I32, I32], [I32]),
            'trace_info': (trace_info, [I32, I32, I32], [I32]),
        }

    def create(self):
        """Create a new interpreter instance."""
        interp_id = self.next_interp_id
        self.next_interp_id += 1
        self.interpreters[interp_id] = FeatherInterp(interp_id)
        self.instance.exports.feather_interp_init(self.ops_ptr, interp_id)
        return interp_id

    def register(self, interp_id, name, fn):
        """Register a Python function as a TCL command."""
        self.interpreters[interp_id].host_commands[name] = fn

    def eval(self, interp_id, script):
        """Evaluate a TCL script."""
        ptr, length = self._write_string(script)
        result = self.instance.exports.feather_script_eval(
            self.ops_ptr, interp_id, ptr, length, 0
        )
        self.instance.exports.free(ptr)

        interp = self.interpreters[interp_id]
        if result != 0:
            raise RuntimeError(interp.get_string(interp.result))
        return interp.get_string(interp.result)

    def get_result(self, interp_id):
        """Get the current result value."""
        interp = self.interpreters[interp_id]
        return interp.get_string(interp.result)

    def destroy(self, interp_id):
        """Destroy an interpreter instance."""
        del self.interpreters[interp_id]


# Usage example
if __name__ == '__main__':
    tclc = FeathercHost('./tclc.wasm')
    interp = tclc.create()

    # Register Python functions as TCL commands
    tclc.register(interp, 'add', lambda args: int(args[0]) + int(args[1]))
    tclc.register(interp, 'multiply', lambda args: int(args[0]) * int(args[1]))
    tclc.register(interp, 'greet', lambda args: f"Hello, {args[0]}!")
    tclc.register(interp, 'range', lambda args: ' '.join(str(i) for i in range(int(args[0]))))

    # Evaluate TCL scripts
    print(tclc.eval(interp, 'set x 10'))           # "10"
    print(tclc.eval(interp, 'set y [add $x 5]'))   # "15"
    print(tclc.eval(interp, 'expr {$x * $y}'))     # "150"
    print(tclc.eval(interp, 'greet World'))        # "Hello, World!"

    # Define a TCL procedure
    tclc.eval(interp, '''
        proc factorial {n} {
            if {$n <= 1} {
                return 1
            }
            expr {$n * [factorial [expr {$n - 1}]]}
        }
    ''')
    print(tclc.eval(interp, 'factorial 5'))        # "120"

    tclc.destroy(interp)
```

## Build Process

### Compiling C to WASM

The key requirements for the build:

1. **Export the function table** for host to add functions
2. **Allow table growth** so host can add functions at runtime
3. **Export memory** for string passing
4. **Export alloc/free** for memory management

```bash
# Using Emscripten (recommended)
emcc \
  -O2 \
  -s EXPORTED_FUNCTIONS='["_feather_interp_init","_feather_script_eval","_feather_script_eval_obj","_feather_command_exec","_alloc","_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  -s ALLOW_TABLE_GROWTH=1 \
  -s EXPORT_ALL=1 \
  -o tclc.wasm \
  src/*.c

# Using clang with WASI SDK
clang \
  --target=wasm32-wasi \
  --sysroot=/path/to/wasi-sdk/share/wasi-sysroot \
  -Wl,--export-table \
  -Wl,--growable-table \
  -Wl,--export=alloc \
  -Wl,--export=free \
  -O2 \
  -o tclc.wasm \
  src/*.c
```

### Required C Additions

Add to `src/tclc.c`:

```c
// Simple bump allocator for WASM builds
#ifdef __wasm__
static char wasm_heap[65536];
static size_t wasm_heap_pos = 0;

__attribute__((export_name("alloc")))
void* alloc(size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    if (wasm_heap_pos + size > sizeof(wasm_heap)) {
        return 0; // Out of memory
    }
    void* ptr = &wasm_heap[wasm_heap_pos];
    wasm_heap_pos += size;
    return ptr;
}

__attribute__((export_name("free")))
void free(void* ptr) {
    // Simple bump allocator: no-op
    // For real use, implement a proper allocator
}
#endif
```

## Comparison of Approaches

| Aspect | WASM Imports | Function Table | Component Model |
|--------|--------------|----------------|-----------------|
| C code changes | Heavy (`#ifdef`) | Minimal (allocator) | Heavy (WIT bindings) |
| FeatherHostOps | Replaced | **Preserved** | Replaced with WIT |
| Browser support | Yes | **Yes** | No |
| Node.js support | Yes | **Yes** | No |
| Wasmtime support | Yes | **Yes** | Yes |
| Host complexity | Medium | Medium | Low (auto-marshal) |
| Data marshaling | Manual | Manual | Automatic |
| Runtime flexibility | Fixed at load | Can modify | Fixed at load |

**Function table is the right choice for tclc because:**

1. **Universal compatibility** — works in browsers, Node.js, Python, and server runtimes
2. **No C code bifurcation** — same source builds for native and WASM
3. **Matches existing architecture** — hosts already implement FeatherHostOps
4. **Runtime extensibility** — can add/modify functions after module load
5. **Future-proof** — Component Model support can be added later without breaking existing hosts
