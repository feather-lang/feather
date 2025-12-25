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
