# Plan: Simplify WASM Host Interface

## Goal

Replace the function-table-based WASM integration with a simpler import-based approach.

**Current state:**
- The JS host must create WASM trampolines to add typed functions to a function table
- The C code uses `call_indirect` through function pointers in `FeatherHostOps`
- This requires ~150 lines of trampoline generation code in `js/feather.js`
- The complexity exists because `WebAssembly.Function` (Type Reflection) is still experimental

**Desired end state:**
- Host functions are declared as `extern` in C and provided as WASM imports
- A static `default_ops` struct is initialized with these extern function pointers
- The public API accepts `NULL` for the ops parameter, falling back to `default_ops`
- The JS host simply provides an imports object at instantiation — no trampolines
- Native builds can still provide custom `FeatherHostOps` for flexibility

**Files involved:**
- `src/host.h` (new) — public header declaring all host function externs
- `src/host.c` (new) — static `default_ops` and `get_ops()` helper
- `src/feather.h` — unchanged (keeps `FeatherHostOps` struct definition)
- `src/feather.c` and other C files — use `get_ops(ops)` at entry points
- `js/feather.js` — simplified to provide imports directly
- `js/wasm_alloc.c` — may need updates for new imports

**Benefits:**
- ~150 fewer lines in JS host
- No runtime WASM code generation
- Works in all browsers without flags
- Preserves native embedding flexibility

---

## M1: Define host.h

Create `src/host.h` with extern declarations for all host functions.

**Tasks:**

1. Create `src/host.h` with header guards
2. Include necessary types from `feather.h` (or forward-declare them)
3. Declare all host functions with `feather_host_` prefix, grouped by category:
   - Frame operations (8 functions)
   - Variable operations (7 functions)
   - Proc operations (9 functions)
   - Namespace operations (18 functions)
   - String operations (5 functions)
   - Rune operations (6 functions)
   - List operations (13 functions)
   - Dict operations (10 functions)
   - Integer operations (2 functions)
   - Double operations (2 functions)
   - Interp operations (7 functions)
   - Bind operations (1 function)
   - Trace operations (3 functions)
   - Foreign operations (6 functions)

**Naming convention:**
```c
// From ops->string.intern → feather_host_string_intern
extern FeatherObj feather_host_string_intern(FeatherInterp interp, const char *s, size_t len);

// From ops->frame.push → feather_host_frame_push  
extern FeatherResult feather_host_frame_push(FeatherInterp interp, FeatherObj cmd, FeatherObj args);
```

**Signature source:** Use the existing `FeatherHostOps` sub-structs in `src/feather.h` as the authoritative reference for function signatures.

**Verification:** The header should compile cleanly when included in a C file.

---

## M2: Create default_ops with static initializer

Create `src/host.c` containing the default ops struct and helper.

**Tasks:**

1. Create `src/host.c`
2. Include `host.h` and `feather.h`
3. Define `static FeatherHostOps default_ops` with designated initializers:
   ```c
   static FeatherHostOps default_ops = {
       .frame = {
           .push = feather_host_frame_push,
           .pop = feather_host_frame_pop,
           /* ... */
       },
       .string = {
           .intern = feather_host_string_intern,
           /* ... */
       },
       /* ... all sub-structs ... */
   };
   ```
4. Define the `get_ops()` helper function:
   ```c
   const FeatherHostOps* feather_get_ops(const FeatherHostOps *ops) {
       return ops ? ops : &default_ops;
   }
   ```
5. Declare `feather_get_ops` in `host.h` as part of the public API

**Verification:** Compiles without errors. All struct fields are populated.

---

## M3: Update C entry points to use get_ops()

Modify the public API functions to accept NULL ops.

**Tasks:**

1. Identify all public entry points that take `const FeatherHostOps *ops`:
   - `feather_interp_init`
   - `feather_script_eval`
   - `feather_script_eval_obj`
   - `feather_command_exec`
   - `feather_parse_command`
   - `feather_subst`
   - All `feather_builtin_*` functions

2. At the start of each function, normalize ops:
   ```c
   FeatherResult feather_script_eval(const FeatherHostOps *ops, FeatherInterp interp,
                                     const char *src, size_t len, FeatherEvalFlags flags) {
       ops = feather_get_ops(ops);
       /* ... rest unchanged ... */
   }
   ```

3. Internal/static functions continue receiving ops as a parameter (already normalized by caller)

**Verification:** 
- `mise test` passes with the Go host (which provides ops)
- Manual test: call `feather_script_eval(NULL, ...)` from a test harness

---

## M4: Update WASM build configuration

Ensure the WASM build exports the right symbols and expects the right imports.

**Tasks:**

1. Update the build command (in `js/mise.toml` or build script) to:
   - Export: `feather_interp_init`, `feather_script_eval`, `feather_script_eval_obj`, `feather_command_exec`, `feather_parse_init`, `feather_parse_command`, `alloc`, `free`, `memory`
   - NOT export the function table (no longer needed)

2. The ~97 `feather_host_*` functions become WASM imports automatically (unresolved externs)

3. Verify the built `feather.wasm` has the expected import section:
   ```bash
   wasm-objdump -x js/feather.wasm | grep -A 200 "Import\["
   ```

**Verification:** WASM builds successfully. Import section lists all `feather_host_*` functions.

---

## M5: Simplify js/feather.js

Remove trampoline machinery and provide imports directly.

**Tasks:**

1. Delete the trampoline-related code:
   - `trampolineCache`
   - `sigKey()`
   - `encodeU32()`, `encodeI32()`, `valType()`
   - `buildTrampolineWasm()`
   - `addToTable()` function
   - `hasWasmFunction` check
   - The `signatures` and `fields` arrays
   - `buildHostOps()` function

2. Restructure instantiation to provide imports directly:
   ```javascript
   const imports = {
       env: {
           feather_host_frame_push: (interp, cmd, args) => { /* ... */ },
           feather_host_frame_pop: (interp) => { /* ... */ },
           feather_host_string_intern: (interp, ptr, len) => { /* ... */ },
           /* ... all ~97 functions ... */
           memory: wasmMemory,
       }
   };
   
   wasmInstance = await WebAssembly.instantiate(wasmModule, imports);
   ```

3. Update all calls to pass `0` (NULL) for the ops parameter:
   ```javascript
   wasmInstance.exports.feather_script_eval(0, interpId, ptr, len, 0);
   ```

4. Remove `hostOpsPtr` and related struct-building code

**Verification:** `mise test:js` passes. Browser demo works.

---

## M6: Update documentation

**Tasks:**

1. Update `WASM.md`:
   - Remove function table sections
   - Remove struct memory layout section
   - Document the import-based approach
   - Simplify host implementation examples

2. Update `js/README.md` if it exists

3. Add a section to `src/host.h` documenting usage:
   ```c
   /**
    * Host Interface for feather WASM builds.
    *
    * WASM hosts must provide implementations for all feather_host_* functions
    * as WASM imports in the "env" namespace.
    *
    * Native hosts can either:
    * - Provide a custom FeatherHostOps struct to feather_script_eval(), or
    * - Link implementations of feather_host_* and pass NULL for ops
    */
   ```

**Verification:** Documentation accurately reflects the implementation.

---

## M7: Clean up and verify

**Tasks:**

1. Remove any dead code from the refactor
2. Ensure `mise build` succeeds
3. Ensure `mise test` passes (Go host)
4. Ensure `mise test:js` passes (JS/WASM host)
5. Test in browser via `js/index.html`
6. Verify no regressions in test coverage

**Verification:** All tests pass. Both native and WASM paths work correctly.

---
---

# Plan: Implement Shimmering (SHIMMER.md)

## Goal

Replace the current value representation in the Go host with a proper TCL-style
shimmering system where values lazily convert between representations.

**Current state:**
- `Object` struct in `interp/` has ad-hoc fields (`stringVal`, `intVal`, `isList`, `listItems`, etc.)
- Shimmering logic is scattered across `Interp.GetString()`, `Interp.GetInt()`, `Interp.GetList()`
- C string caching (`cstr *C.char`) is on the Object

**Desired end state:**
- Clean `*Obj` type with `bytes string`, `intrep ObjType`, `cstr *C.char`
- `ObjType` interface for internal representations
- Concrete types: `IntType`, `DoubleType`, `ListType`, `DictType`, `ForeignType`
- Free-standing conversion functions: `AsInt()`, `AsDouble()`, `AsList()`, `AsDict()`, `AsBool()`
- Callbacks use the new system transparently

**Files involved:**
- `interp/obj.go` (new) — `Obj`, `ObjType`, conversion interfaces
- `interp/objtype_int.go` (new) — `IntType`
- `interp/objtype_double.go` (new) — `DoubleType`
- `interp/objtype_list.go` (new) — `ListType`
- `interp/objtype_dict.go` (new) — `DictType`
- `interp/objtype_foreign.go` (new) — `ForeignType`
- `interp/convert.go` (new) — `AsInt()`, `AsList()`, etc.
- `interp/interp.go` — update `Interp` to use `*Obj`
- `interp/callbacks.go` — update callbacks to use new conversion functions

**Benefits:**
- Clean separation of concerns
- Easy to add new types (just implement `ObjType` + optional conversion interfaces)
- Direct type-to-type conversion without string round-trip
- Matches TCL semantics

---

## S1: Create Obj and ObjType foundation

Create the core types without any concrete implementations yet.

**Files:** `interp/obj.go`

**Tasks:**

1. Define `Obj` struct:
   ```go
   type Obj struct {
       bytes  string
       intrep ObjType
       cstr   *C.char
   }
   ```

2. Define `ObjType` interface:
   ```go
   type ObjType interface {
       Name() string
       UpdateString() string
       Dup() ObjType
   }
   ```

3. Define conversion interfaces:
   ```go
   type IntoInt interface { IntoInt() (int64, bool) }
   type IntoDouble interface { IntoDouble() (float64, bool) }
   type IntoList interface { IntoList() ([]*Obj, bool) }
   type IntoDict interface { IntoDict() (map[string]*Obj, []string, bool) }
   type IntoBool interface { IntoBool() (bool, bool) }
   ```

4. Implement `Obj` methods: `String()`, `Type()`, `Invalidate()`, `Copy()`

5. Implement `NewString()` constructor

**Verification:** File compiles. `NewString("hello").String()` returns `"hello"`.

---

## S2: Implement IntType

**Files:** `interp/objtype_int.go`

**Tasks:**

1. Define `IntType int64`
2. Implement `ObjType`: `Name()`, `UpdateString()`, `Dup()`
3. Implement `IntoInt`, `IntoDouble`, `IntoBool`
4. Add `NewInt(v int64) *Obj` constructor to `obj.go`

**Verification:**
```go
n := NewInt(42)
n.String() == "42"
n.Type() == "int"
```

---

## S3: Implement DoubleType

**Files:** `interp/objtype_double.go`

**Tasks:**

1. Define `DoubleType float64`
2. Implement `ObjType`: `Name()`, `UpdateString()`, `Dup()`
3. Implement `IntoInt`, `IntoDouble`
4. Add `NewDouble(v float64) *Obj` constructor

**Verification:**
```go
d := NewDouble(3.14)
d.String() == "3.14"
d.Type() == "double"
```

---

## S4: Implement ListType

**Files:** `interp/objtype_list.go`

**Tasks:**

1. Define `ListType []*Obj`
2. Implement `ObjType`: `Name()`, `UpdateString()` (TCL list formatting), `Dup()` (shallow clone)
3. Implement `IntoList`
4. Implement `IntoDict` (for even-length lists)
5. Add `NewList(items ...*Obj) *Obj` constructor

**Verification:**
```go
l := NewList(NewString("a"), NewString("b"))
l.String() == "a b"
l.Type() == "list"
```

---

## S5: Implement DictType

**Files:** `interp/objtype_dict.go`

**Tasks:**

1. Define `DictType struct { Items map[string]*Obj; Order []string }`
2. Implement `ObjType`: `Name()`, `UpdateString()`, `Dup()` (deep copy)
3. Implement `IntoDict`, `IntoList`
4. Add `NewDict() *Obj` constructor

**Verification:**
```go
d := NewDict()
// after setting k=v
d.String() == "k v"
```

---

## S6: Implement ForeignType

**Files:** `interp/objtype_foreign.go`

**Tasks:**

1. Define `ForeignType struct { TypeName string; Value any }`
2. Implement `ObjType`: `Name()`, `UpdateString()` (`<TypeName:ptr>`), `Dup()` (returns self)
3. No conversion interfaces (opaque)
4. Add `NewForeign(typeName string, value any) *Obj` constructor

**Verification:**
```go
f := NewForeign("Conn", myConn)
f.Type() == "Conn"
strings.HasPrefix(f.String(), "<Conn:") == true
```

---

## S7: Implement conversion functions

**Files:** `interp/convert.go`

**Tasks:**

1. Implement `AsInt(o *Obj) (int64, error)`:
   - Try `IntoInt` interface
   - Fallback: parse `o.String()`
   - Shimmer: set `o.intrep = IntType(v)`

2. Implement `AsDouble(o *Obj) (float64, error)`:
   - Try `IntoDouble` interface
   - Fallback: parse `o.String()`

3. Implement `AsList(o *Obj) ([]*Obj, error)`:
   - Try `IntoList` interface
   - Fallback: parse `o.String()` as TCL list

4. Implement `AsDict(o *Obj) (*DictType, error)`:
   - Try `IntoDict` interface
   - Fallback: parse as list, convert to dict

5. Implement `AsBool(o *Obj) (bool, error)`:
   - Try `IntoBool` interface
   - Fallback: try `AsInt`, then string truthiness

**Verification:**
```go
s := NewString("42")
v, _ := AsInt(s)  // v == 42, s.intrep is now IntType(42)
s.Type() == "int"
```

---

## S8: Implement list/dict helper functions

**Files:** `interp/convert.go` (append to)

**Tasks:**

1. `ListLen(o *Obj) int`
2. `ListAt(o *Obj, i int) *Obj`
3. `ListAppend(o *Obj, elem *Obj)`
4. `DictGet(o *Obj, key string) (*Obj, bool)`
5. `DictSet(o *Obj, key string, val *Obj)`

**Verification:** Basic list/dict operations work and invalidate string rep.

---

## S9: Update Interp to use *Obj

**Files:** `interp/interp.go`

**Tasks:**

1. Replace `Object` with `*Obj` in handle maps
2. Update `internString()` to return handle for `*Obj`
3. Update `getObject()` to return `*Obj`
4. Remove old `GetString()`, `GetInt()`, `GetList()` methods from `Interp`
   (these are now free functions)
5. Keep handle registration/lookup logic

**Verification:** `mise build` succeeds. Interp can create and lookup objects.

---

## S10: Update callbacks to use new system

**Files:** `interp/callbacks.go`

**Tasks:**

1. Update `goStringGet` to use `o.String()` and cache `cstr`
2. Update `goStringIntern` to use `NewString()`
3. Update `goIntCreate` to use `NewInt()`
4. Update `goIntGet` to use `AsInt()`
5. Update `goDoubleCreate` to use `NewDouble()`
6. Update `goDoubleGet` to use `AsDouble()`
7. Update `goListCreate` to use `NewList()`
8. Update `goListFrom` to use `AsList()`
9. Update `goListLength`, `goListAt`, `goListPush`, etc. to use `AsList()` and list helpers
10. Update `goDictCreate` to use `NewDict()`
11. Update `goDictGet`, `goDictSet`, etc. to use `AsDict()` and dict helpers
12. Update foreign object callbacks to use `NewForeign()` and type checks

**Verification:** `mise test` passes. All existing tests continue to work.

---

## S11: Clean up and remove old code

**Tasks:**

1. Remove old `Object` struct definition
2. Remove old shimmering methods from `Interp`
3. Update `interp/AGENTS.md` to reflect new shimmering architecture
4. Verify no dead code remains

**Verification:** `mise test` passes. Code is clean.
