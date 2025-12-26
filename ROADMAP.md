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

---
---

# Plan: Merge interp Package into Root feather Package

## Goal

Eliminate the `interp` subpackage and wrapper types, giving users direct access
to `*Obj`, `ObjType`, and the shimmering system from the public `feather` package.

**Current state:**
- `interp/` package contains the interpreter implementation with `*Obj`, `ObjType`, etc.
- Root `feather` package wraps `interp` types with `Object` (handle wrapper)
- Users import `"github.com/feather-lang/feather"` but can't implement `ObjType`
- The `Object` wrapper prevents custom shimmering types

**Desired end state:**
- Single `feather` package with everything
- Users can implement `feather.ObjType` for custom shimmering types
- Direct access to `*feather.Obj` without handle indirection
- Import path unchanged: `"github.com/feather-lang/feather"`
- C integration details remain internal (unexported)

**Files involved:**
- `interp/*.go` → move to root package
- `interp/*.c` → move to root package
- `feather.go` → merge/simplify, remove wrapper types
- `convert.go` → merge with `interp/convert.go`
- `cmd/*` → update imports
- `api_test.go`, `feather_test.go` → update to use merged API

**Benefits:**
- Users can implement custom `ObjType` for shimmering
- Simpler API without wrapper indirection
- No `interp` package leaking into type signatures
- Single package to understand

---

## P1: Move interp Go files to root package

Move all Go source files from `interp/` to root, updating package declarations.

**Tasks:**

1. Move these files to root package:
   - `interp/obj.go` → `obj.go`
   - `interp/objtype_int.go` → `objtype_int.go`
   - `interp/objtype_double.go` → `objtype_double.go`
   - `interp/objtype_list.go` → `objtype_list.go`
   - `interp/objtype_dict.go` → `objtype_dict.go`
   - `interp/objtype_foreign.go` → `objtype_foreign.go`
   - `interp/convert.go` → `interp_convert.go` (rename to avoid conflict)
   - `interp/feather.go` → `interp.go` (rename to avoid conflict)
   - `interp/callbacks.go` → `callbacks.go`
   - `interp/foreign.go` → `foreign.go`
   - `interp/foreign_test.go` → `foreign_test.go`

2. Change `package interp` to `package feather` in all moved files

3. Remove `interp/doc.go` and `interp/AGENTS.md` (merge content if needed)

**Verification:** Files are in root package. `go build` fails (expected - imports broken).

---

## P2: Move interp C files to root package

Move C source files that are part of the Go build.

**Tasks:**

1. Move C files:
   - `interp/callbacks.c` → `callbacks.c`
   - `interp/feather_core.c` → `feather_core.c`

2. Update any `#include` paths if needed

**Verification:** C files in root. Still won't build (imports broken).

---

## P3: Remove interp import from root package

Update the original root package files to not import `interp`.

**Tasks:**

1. In `feather.go`:
   - Remove `import "github.com/feather-lang/feather/interp"`
   - Remove `Object` wrapper type (now use `*Obj` directly)
   - Remove wrapper methods that delegate to `interp`
   - Keep/adapt high-level convenience functions

2. In root `convert.go`:
   - Merge with `interp_convert.go` or remove if redundant
   - Remove `interp` imports

3. Remove `OKObj`, `ErrorObj` (just use `OK` with `*Obj`)

**Verification:** No `interp` imports remain in root package.

---

## P4: Update public API types

Replace wrapper types with direct types.

**Tasks:**

1. Replace `Object` with `*Obj` in public API:
   - `CommandFunc` signature: `func(i *Interp, cmd *Obj, args []*Obj) Result`
   - All methods that took/returned `Object` now use `*Obj`

2. Update `Result` type:
   - Store `*Obj` directly instead of string conversion
   - `OK(v any)` handles `*Obj` specially (no conversion)

3. Export necessary types:
   - `Obj` (already exported)
   - `ObjType` (already exported)
   - `IntType`, `DoubleType`, `ListType`, `DictType`, `ForeignType`
   - Conversion interfaces: `IntoInt`, `IntoDouble`, `IntoList`, `IntoDict`, `IntoBool`

4. Keep internal types unexported:
   - `Handle`, `FeatherObj`, `FeatherResult` (C interop)
   - Registry internals

**Verification:** `go build ./...` succeeds.

---

## P5: Update cmd packages

Update all command packages to use the new API.

**Tasks:**

1. `cmd/feather-tester/main.go`:
   - Remove any `interp` imports
   - Update to use `*feather.Obj` instead of `feather.Object`

2. `cmd/feather-httpd/main.go`:
   - Same updates

3. Any other `cmd/` packages

**Verification:** `go build ./cmd/...` succeeds.

---

## P6: Update tests

Update test files for the new API.

**Tasks:**

1. `api_test.go`:
   - Update to use `*Obj` instead of `Object`
   - Remove any `interp` imports

2. `feather_test.go`:
   - Same updates

3. `foreign_test.go` (moved from interp):
   - Already uses `feather` package after move

**Verification:** `go test ./...` passes.

---

## P7: Update documentation

Update docs and AGENTS.md files.

**Tasks:**

1. Update root `AGENTS.md`:
   - Remove references to `interp` package
   - Document that `*Obj` and `ObjType` are the public types

2. Merge `interp/AGENTS.md` content into root `AGENTS.md`

3. Update `README.md`:
   - Update import examples
   - Show custom `ObjType` implementation example

4. Update `doc.go`:
   - Merge with `interp/doc.go`
   - Update package documentation

**Verification:** Documentation is accurate and complete.

---

## P8: Remove interp directory

Clean up the now-empty interp directory.

**Tasks:**

1. Verify `interp/` is empty or only has files that should be deleted
2. `rm -rf interp/`
3. Update `.gitignore` if needed

**Verification:** `interp/` directory no longer exists.

---

## P9: Run full test suite

Verify everything works end-to-end.

**Tasks:**

1. `go build ./...` — all packages build
2. `go test ./...` — all Go tests pass
3. `mise test` — harness tests pass (1320+ tests)
4. `mise test:js` — JS/WASM tests pass

**Verification:** All tests pass. No regressions.

---

## P10: Update SHIMMER.md and other docs

Update design documents that reference `interp` package.

**Tasks:**

1. Update `SHIMMER.md`:
   - Change `interp/obj.go` references to `obj.go`
   - Change `interp.Obj` to `feather.Obj`

2. Update `FOREIGN.md` if it references `interp`

3. Update this `ROADMAP.md`:
   - Mark shimmering plan as superseded/completed
   - Update file paths in earlier milestones

**Verification:** All docs reference correct package/paths.
