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

---

# Plan: Eliminate C String Pointers (BYTEOPS.md)

## Goal

Replace `const char*` returns in host ops with byte-at-a-time access, so C code
never holds pointers into host memory. All string content stays in the host (Go/JS),
managed by the host's garbage collector.

**Current state:**

- `ops->string.get()` returns `const char*` pointing into host memory
- Go host must cache `cstr *C.char` on each Object to keep memory valid
- JS host must copy strings into WASM linear memory for C to read
- C code iterates over byte pointers for parsing: `while (*p == ' ') p++;`
- 143 call sites in C use `ops->string.get()`

**Desired end state:**

- `ops->string.get()` is removed entirely
- New `ops->string.byte_at(interp, obj, index)` returns single byte as int (-1 for OOB)
- New `ops->string.byte_length(interp, obj)` returns byte count
- New `ops->string.slice(interp, obj, start, end)` returns new handle for substring
- C parser uses byte-at-a-time access with C-local character class checks
- Go host has no `cstr` field — all memory managed by Go GC
- JS host has no string copying to WASM memory — just integer returns
- Only integers and handles cross the host boundary

**Character classes stay in C:**

```c
// src/charclass.h - pure integer logic, no host calls
static inline int is_whitespace(int ch) { return ch == ' ' || ch == '\t'; }
static inline int is_varname_char(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}
static inline int is_digit(int ch) { return ch >= '0' && ch <= '9'; }
static inline int is_hex_digit(int ch) {
    return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}
```

**Files involved:**

- `src/feather.h` — update `FeatherStringOps` struct
- `src/charclass.h` — already exists, may need additions
- `src/parse.c` — major rewrite: byte-pointer iteration → byte_at() calls
- `src/builtin_expr.c` — same pattern
- `src/builtin_subst.c` — same pattern
- `src/builtin_format.c` — same pattern
- All other C files using `ops->string.get()` — update to new API
- `interp/callbacks.go` — implement new ops, remove `cstr` caching
- `interp/obj.go` — remove `cstr *C.char` field
- `js/feather.js` — implement new ops, remove string-to-WASM copying

**Benefits:**

- Zero C string management in hosts
- All string memory in host GC (Go/JS)
- Clean WASM boundary — only i32 values cross
- No lifetime management for string pointers
- Simpler host implementation

---

## B1: Define new FeatherStringOps

Update `src/feather.h` with the new string operations API.

**Tasks:**

1. Replace `FeatherStringOps` with new definition:

   ```c
   typedef struct FeatherStringOps {
       /**
        * byte_at returns the byte at the given index, or -1 if out of bounds.
        * Index is 0-based. This is the primary way C accesses string content.
        */
       int (*byte_at)(FeatherInterp interp, FeatherObj str, size_t index);

       /**
        * byte_length returns the length of the string in bytes.
        */
       size_t (*byte_length)(FeatherInterp interp, FeatherObj str);

       /**
        * slice returns a new string object containing bytes [start, end).
        * Returns empty string if start >= end or start >= length.
        */
       FeatherObj (*slice)(FeatherInterp interp, FeatherObj str, size_t start, size_t end);

       /**
        * concat returns a new object whose string value is
        * the concatenation of two objects.
        */
       FeatherObj (*concat)(FeatherInterp interp, FeatherObj a, FeatherObj b);

       /**
        * compare compares two strings using Unicode ordering.
        * Returns <0 if a < b, 0 if a == b, >0 if a > b.
        */
       int (*compare)(FeatherInterp interp, FeatherObj a, FeatherObj b);

       /**
        * equal returns 1 if strings are byte-equal, 0 otherwise.
        * More efficient than compare when only equality is needed.
        */
       int (*equal)(FeatherInterp interp, FeatherObj a, FeatherObj b);

       /**
        * match tests if string matches a glob pattern.
        * Returns 1 if matches, 0 otherwise.
        */
       int (*match)(FeatherInterp interp, FeatherObj pattern, FeatherObj str, int nocase);

       /**
        * regex_match tests if a string matches a regular expression pattern.
        * Returns TCL_OK and sets *result to 1/0. Returns TCL_ERROR if invalid pattern.
        */
       FeatherResult (*regex_match)(FeatherInterp interp, FeatherObj pattern,
                                    FeatherObj string, int *result);
   } FeatherStringOps;
   ```

2. Remove `intern` — host creates strings internally, returns handles
3. Remove `get` — no more `const char*` returns

**Verification:** Header compiles. Old code will fail to compile (expected).

---

## B2: Add string builder to host ops

C needs a way to construct new strings without `const char*`. Add a builder API.

**Tasks:**

1. Add `FeatherStringBuilder` operations to `FeatherStringOps`:

   ```c
   typedef struct FeatherStringOps {
       /* ... existing from B1 ... */

       /**
        * builder_new creates a new string builder with optional initial capacity.
        */
       FeatherObj (*builder_new)(FeatherInterp interp, size_t capacity);

       /**
        * builder_append_byte appends a single byte to the builder.
        */
       void (*builder_append_byte)(FeatherInterp interp, FeatherObj builder, int byte);

       /**
        * builder_append_obj appends another string object's bytes to the builder.
        */
       void (*builder_append_obj)(FeatherInterp interp, FeatherObj builder, FeatherObj str);

       /**
        * builder_finish converts builder to immutable string, returns handle.
        * The builder handle becomes invalid after this call.
        */
       FeatherObj (*builder_finish)(FeatherInterp interp, FeatherObj builder);
   } FeatherStringOps;
   ```

**Verification:** Header compiles with builder ops.

---

## B3: Update charclass.h

Ensure all character class functions needed by the parser are available.

**Tasks:**

1. Review `src/charclass.h` and add any missing functions:

   ```c
   static inline int is_whitespace(int ch) { return ch == ' ' || ch == '\t'; }
   static inline int is_newline(int ch) { return ch == '\n' || ch == '\r'; }
   static inline int is_command_terminator(int ch) {
       return ch == '\n' || ch == '\r' || ch == '\0' || ch == ';' || ch < 0;
   }
   static inline int is_word_terminator(int ch) {
       return is_whitespace(ch) || is_command_terminator(ch);
   }
   static inline int is_digit(int ch) { return ch >= '0' && ch <= '9'; }
   static inline int is_hex_digit(int ch) {
       return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
   }
   static inline int is_varname_char(int ch) {
       return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_';
   }
   static inline int is_alpha(int ch) {
       return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
   }
   static inline int hex_value(int ch) {
       if (ch >= '0' && ch <= '9') return ch - '0';
       if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
       if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
       return -1;
   }
   ```

2. All functions take `int ch` and handle `-1` (EOF) gracefully

**Verification:** Header compiles. Functions handle ch=-1 correctly.

---

## B4: Create parsing helper macros/functions

Add convenience macros for common parsing patterns.

**Tasks:**

1. Create `src/parse_helpers.h`:

   ```c
   #ifndef FEATHER_PARSE_HELPERS_H
   #define FEATHER_PARSE_HELPERS_H

   #include "feather.h"
   #include "charclass.h"

   // Get byte at position, -1 if past end
   #define BYTE_AT(ops, interp, obj, pos) \
       ((ops)->string.byte_at((interp), (obj), (pos)))

   // Skip whitespace, return new position
   static inline size_t skip_whitespace(const FeatherHostOps *ops,
                                        FeatherInterp interp,
                                        FeatherObj str, size_t pos) {
       int ch;
       while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 && is_whitespace(ch)) {
           pos++;
       }
       return pos;
   }

   // Skip while character class matches
   static inline size_t skip_while(const FeatherHostOps *ops,
                                   FeatherInterp interp,
                                   FeatherObj str, size_t pos,
                                   int (*predicate)(int)) {
       int ch;
       while ((ch = ops->string.byte_at(interp, str, pos)) >= 0 && predicate(ch)) {
           pos++;
       }
       return pos;
   }

   // Check if at end of string
   #define AT_END(ops, interp, obj, pos) \
       ((ops)->string.byte_at((interp), (obj), (pos)) < 0)

   #endif
   ```

**Verification:** Header compiles and can be included in parse.c.

---

## B5: Update parse.c - core parsing

Rewrite the main parser to use byte-at-a-time access.

**Tasks:**

1. Replace all `const char *p` iteration with position-based access:

   ```c
   // Before:
   const char *str = ops->string.get(interp, obj, &len);
   while (p < end && *p == ' ') p++;

   // After:
   size_t len = ops->string.byte_length(interp, obj);
   size_t pos = 0;
   pos = skip_whitespace(ops, interp, obj, pos);
   ```

2. Update `process_backslash()` to use byte_at:

   ```c
   static size_t process_backslash(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj str, size_t pos, size_t end,
                                   FeatherObj builder) {
       int ch = ops->string.byte_at(interp, str, pos);
       if (ch < 0) return pos;

       switch (ch) {
           case 'n': ops->string.builder_append_byte(interp, builder, '\n'); return pos + 1;
           case 't': ops->string.builder_append_byte(interp, builder, '\t'); return pos + 1;
           // ... etc
       }
   }
   ```

3. Update word parsing, brace parsing, quote parsing similarly

4. Use `ops->string.slice()` to extract substrings instead of pointer arithmetic

**Verification:** `mise test` passes with basic parsing tests.

---

## B6: Update builtin_expr.c

Rewrite expression parser to use byte-at-a-time access.

**Tasks:**

1. Update `ExprParser` struct:

   ```c
   typedef struct {
       const FeatherHostOps *ops;
       FeatherInterp interp;
       FeatherObj expr;      // Expression as handle (not const char*)
       size_t expr_len;
       size_t pos;           // Current position (not pointer)
       int has_error;
       FeatherObj error_msg;
       int skip_mode;
   } ExprParser;
   ```

2. Replace all pointer-based scanning with byte_at() calls

3. Update number parsing, operator scanning, string literal parsing

**Verification:** `mise test` passes with expr tests.

---

## B7: Update builtin_subst.c

Rewrite substitution to use byte-at-a-time access.

**Tasks:**

1. Update variable substitution to use byte_at() for scanning `$varname`
2. Update command substitution scanning for `[...]`
3. Update backslash processing
4. Use string builder for output construction

**Verification:** `mise test` passes with subst tests.

---

## B8: Update builtin_format.c

Rewrite format string parsing.

**Tasks:**

1. Update format specifier parsing to use byte_at()
2. Update width/precision extraction
3. Use string builder for output

**Verification:** `mise test` passes with format tests.

---

## B9: Update remaining C files

Update all other files that use `ops->string.get()`.

**Tasks:**

1. Files to update (grep for `ops->string.get`):
   - `builtin_catch.c`
   - `builtin_variable.c`
   - `builtin_upvar.c`
   - `builtin_string.c`
   - `builtin_unset.c`
   - `builtin_join.c`
   - `builtin_uplevel.c`
   - `builtin_lindex.c`
   - `builtin_concat.c`
   - `builtin_namespace.c`
   - `builtin_for.c`
   - `builtin_apply.c`
   - `builtin_global.c`
   - `builtin_incr.c`
   - `builtin_lsort.c`
   - `builtin_dict.c`
   - `builtin_info.c`
   - `builtin_rename.c`
   - `builtin_return.c`
   - `builtin_if.c`
   - `index_parse.c`
   - `namespace_util.c`
   - `resolve.c`
   - `eval.c`

2. Pattern for simple "check prefix" cases:

   ```c
   // Before:
   const char *str = ops->string.get(interp, obj, &len);
   if (len > 0 && str[0] == '-') { ... }

   // After:
   if (ops->string.byte_at(interp, obj, 0) == '-') { ... }
   ```

3. Pattern for string comparison - use ops->string.equal() or compare()

**Verification:** All C files compile. `mise test` passes.

---

## B10: Update Go host - implement new ops

Implement the new string operations in the Go host.

**Tasks:**

1. Update `interp/callbacks.go`:

   ```go
   //export goStringByteAt
   func goStringByteAt(interp C.FeatherInterp, obj C.FeatherObj, index C.size_t) C.int {
       i := getInterp(interp)
       str := i.GetString(FeatherObj(obj))
       if int(index) >= len(str) {
           return -1
       }
       return C.int(str[index])
   }

   //export goStringByteLength
   func goStringByteLength(interp C.FeatherInterp, obj C.FeatherObj) C.size_t {
       i := getInterp(interp)
       str := i.GetString(FeatherObj(obj))
       return C.size_t(len(str))
   }

   //export goStringSlice
   func goStringSlice(interp C.FeatherInterp, obj C.FeatherObj, start C.size_t, end C.size_t) C.FeatherObj {
       i := getInterp(interp)
       str := i.GetString(FeatherObj(obj))
       s, e := int(start), int(end)
       if s >= len(str) { s = len(str) }
       if e > len(str) { e = len(str) }
       if s >= e { return C.FeatherObj(i.internString("")) }
       return C.FeatherObj(i.internString(str[s:e]))
   }

   //export goStringEqual
   func goStringEqual(interp C.FeatherInterp, a C.FeatherObj, b C.FeatherObj) C.int {
       i := getInterp(interp)
       if i.GetString(FeatherObj(a)) == i.GetString(FeatherObj(b)) {
           return 1
       }
       return 0
   }
   ```

2. Implement builder operations:

   ```go
   //export goStringBuilderNew
   func goStringBuilderNew(interp C.FeatherInterp, capacity C.size_t) C.FeatherObj {
       i := getInterp(interp)
       builder := &strings.Builder{}
       builder.Grow(int(capacity))
       return C.FeatherObj(i.storeBuilder(builder))
   }

   //export goStringBuilderAppendByte
   func goStringBuilderAppendByte(interp C.FeatherInterp, builder C.FeatherObj, b C.int) {
       i := getInterp(interp)
       bldr := i.getBuilder(FeatherObj(builder))
       if bldr != nil {
           bldr.WriteByte(byte(b))
       }
   }

   //export goStringBuilderAppendObj
   func goStringBuilderAppendObj(interp C.FeatherInterp, builder C.FeatherObj, str C.FeatherObj) {
       i := getInterp(interp)
       bldr := i.getBuilder(FeatherObj(builder))
       if bldr != nil {
           bldr.WriteString(i.GetString(FeatherObj(str)))
       }
   }

   //export goStringBuilderFinish
   func goStringBuilderFinish(interp C.FeatherInterp, builder C.FeatherObj) C.FeatherObj {
       i := getInterp(interp)
       bldr := i.getBuilder(FeatherObj(builder))
       if bldr == nil {
           return C.FeatherObj(i.internString(""))
       }
       result := bldr.String()
       i.releaseBuilder(FeatherObj(builder))
       return C.FeatherObj(i.internString(result))
   }
   ```

3. Remove old `goStringGet` and `goStringIntern`

4. Remove `cstr *C.char` field from Object struct

5. Remove C string caching logic

**Verification:** `mise test` passes with Go host.

---

## B11: Update JS host - implement new ops

Implement the new string operations in the JavaScript host.

**Tasks:**

1. Update `js/feather.js` host functions:

   ```javascript
   feather_host_string_byte_at: (interpId, obj, index) => {
       const interp = interpreters.get(interpId);
       const str = interp.getString(obj);
       if (index >= str.length) return -1;
       return str.charCodeAt(index);
   },

   feather_host_string_byte_length: (interpId, obj) => {
       const interp = interpreters.get(interpId);
       const str = interp.getString(obj);
       // Return byte length, not char length (for UTF-8)
       return new TextEncoder().encode(str).length;
   },

   feather_host_string_slice: (interpId, obj, start, end) => {
       const interp = interpreters.get(interpId);
       const str = interp.getString(obj);
       const bytes = new TextEncoder().encode(str);
       if (start >= bytes.length) return interp.store({ type: 'string', value: '' });
       const sliced = bytes.slice(start, end);
       const result = new TextDecoder().decode(sliced);
       return interp.store({ type: 'string', value: result });
   },

   feather_host_string_equal: (interpId, a, b) => {
       const interp = interpreters.get(interpId);
       return interp.getString(a) === interp.getString(b) ? 1 : 0;
   },

   feather_host_string_match: (interpId, pattern, str, nocase) => {
       const interp = interpreters.get(interpId);
       const p = interp.getString(pattern);
       const s = interp.getString(str);
       return globMatch(nocase ? p.toLowerCase() : p,
                        nocase ? s.toLowerCase() : s) ? 1 : 0;
   },
   ```

2. Implement builder operations:

   ```javascript
   feather_host_string_builder_new: (interpId, capacity) => {
       const interp = interpreters.get(interpId);
       return interp.store({ type: 'builder', bytes: [] });
   },

   feather_host_string_builder_append_byte: (interpId, builder, byte) => {
       const interp = interpreters.get(interpId);
       const obj = interp.get(builder);
       if (obj?.type === 'builder') {
           obj.bytes.push(byte);
       }
   },

   feather_host_string_builder_append_obj: (interpId, builder, str) => {
       const interp = interpreters.get(interpId);
       const obj = interp.get(builder);
       if (obj?.type === 'builder') {
           const bytes = new TextEncoder().encode(interp.getString(str));
           obj.bytes.push(...bytes);
       }
   },

   feather_host_string_builder_finish: (interpId, builder) => {
       const interp = interpreters.get(interpId);
       const obj = interp.get(builder);
       if (obj?.type !== 'builder') {
           return interp.store({ type: 'string', value: '' });
       }
       const result = new TextDecoder().decode(new Uint8Array(obj.bytes));
       return interp.store({ type: 'string', value: result });
   },
   ```

3. Remove old `feather_host_string_get` and `feather_host_string_intern`

4. Remove any WASM memory string copying code

**Verification:** `mise test:js` passes.

---

## B12: Remove string.intern from C API

Update C code that creates strings to use the builder API.

**Tasks:**

1. Search for remaining uses of creating strings from C:
   - Error message construction
   - Result string building
2. Replace with builder pattern:

   ```c
   // Before (if this pattern existed):
   // result = ops->string.intern(interp, "error: ", 7);

   // After:
   FeatherObj builder = ops->string.builder_new(interp, 64);
   ops->string.builder_append_obj(interp, builder, prefix);
   ops->string.builder_append_obj(interp, builder, message);
   result = ops->string.builder_finish(interp, builder);
   ```

3. For literal strings, hosts should pre-intern common strings

**Verification:** No C code creates strings via intern. All string creation via builder.

---

## B13: Update host.h extern declarations

Update the extern function declarations for WASM imports.

**Tasks:**

1. Update `src/host.h` with new string function externs:

   ```c
   // String operations - byte access
   extern int feather_host_string_byte_at(FeatherInterp interp, FeatherObj str, size_t index);
   extern size_t feather_host_string_byte_length(FeatherInterp interp, FeatherObj str);
   extern FeatherObj feather_host_string_slice(FeatherInterp interp, FeatherObj str,
                                                size_t start, size_t end);
   extern FeatherObj feather_host_string_concat(FeatherInterp interp, FeatherObj a, FeatherObj b);
   extern int feather_host_string_compare(FeatherInterp interp, FeatherObj a, FeatherObj b);
   extern int feather_host_string_equal(FeatherInterp interp, FeatherObj a, FeatherObj b);
   extern int feather_host_string_match(FeatherInterp interp, FeatherObj pattern,
                                        FeatherObj str, int nocase);
   extern FeatherResult feather_host_string_regex_match(FeatherInterp interp, FeatherObj pattern,
                                                        FeatherObj string, int *result);

   // String builder operations
   extern FeatherObj feather_host_string_builder_new(FeatherInterp interp, size_t capacity);
   extern void feather_host_string_builder_append_byte(FeatherInterp interp,
                                                       FeatherObj builder, int byte);
   extern void feather_host_string_builder_append_obj(FeatherInterp interp,
                                                      FeatherObj builder, FeatherObj str);
   extern FeatherObj feather_host_string_builder_finish(FeatherInterp interp, FeatherObj builder);
   ```

2. Remove old `feather_host_string_get` and `feather_host_string_intern`

**Verification:** Header compiles. WASM build has correct imports.

---

## B14: Clean up and documentation

Final cleanup and documentation updates.

**Tasks:**

1. Remove dead code:
   - Old `goStringGet`, `goStringIntern` in Go
   - Old string memory management in JS
   - `cstr` field from Go Object struct

2. Update documentation:
   - `WASM.md` — document new string ops
   - `src/feather.h` — update comments
   - This `ROADMAP.md` — mark plan complete

3. Update `interp/AGENTS.md`:
   - Remove references to `cstr` caching
   - Document new string handling approach

**Verification:** All docs accurate. No dead code.

---

## B15: Full test suite verification

Run complete test suite on both hosts.

**Tasks:**

1. `mise build` — all builds succeed
2. `mise test` — Go host passes all tests (1320+)
3. `mise test:js` — JS/WASM host passes all tests
4. Browser test via `js/index.html`
5. Performance sanity check — parsing not catastrophically slower

**Verification:** All tests pass. Both hosts work correctly.

---

## Performance Notes

The byte-at-a-time approach adds function call overhead per byte during parsing.
Mitigation strategies if needed:

1. **Batch reading**: Add `read_bytes(obj, start, buf, len)` to read multiple bytes
2. **Caching in parser**: Keep a small local buffer for lookahead
3. **Specialized scanners**: Add `find_byte(obj, byte, start)` for common searches

These can be added in future milestones if profiling shows parsing is a bottleneck.
