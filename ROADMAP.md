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

---
