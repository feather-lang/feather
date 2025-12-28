# Compiling Go Feather Host to WASM

## Investigation Summary

This document details our investigation into compiling a Go-based Feather host (like `cmd/feather-tester`) to WebAssembly using **both TinyGo and the standard Go toolchain**.

## TL;DR - Both Paths Blocked ❌

| Toolchain | CGO Support | Blocker |
|-----------|-------------|---------|
| **Standard Go** | ❌ **NO** | CGO not supported for any WASM target |
| **TinyGo** | ✅ Partial | Missing `runtime/cgo.Handle` for WASM |
| **Result** | ❌ | **Cannot compile Go host to WASM** |

## Current Architecture

### Working WASM Implementation
- **C interpreter** → compiled to WASM using **Zig**
- **JavaScript host** (`js/tester.js`) → provides FeatherHostOps in JavaScript/Node.js
- **Test results**: 66+ tests passing (eval.html, foreign.html, etc.)

### Go Host Architecture
- **Go package** (`github.com/feather-lang/feather`) → wraps C interpreter via CGO
- **C source** (`src/*.c`, `src/*.h`) → stdlib-less C interpreter
- **Go host** (`cmd/feather-tester`) → implements test commands and Counter type in Go

## Standard Go Toolchain Investigation

### GOOS=js GOARCH=wasm (Browser Target) ❌

The standard Go compiler's `js/wasm` target **does NOT support CGO**:

```bash
GOOS=js GOARCH=wasm go build ./cmd/feather-tester
# ❌ Requires CGO_ENABLED=0
# Cannot compile packages using import "C"
```

**Limitation**: The `js/wasm` port generates an ABI deeply tied to JavaScript and does not support C bindings at all. You must build with `CGO_ENABLED=0`.

**Reference**: [Go Wiki: WebAssembly](https://go.dev/wiki/WebAssembly)

### GOOS=wasip1 GOARCH=wasm (WASI Target) ❌

The WASI preview 1 target added in Go 1.21 **also does NOT support CGO**:

```bash
GOOS=wasip1 GOARCH=wasm go build ./cmd/feather-tester
# ❌ CGO not supported for wasm targets
```

**Limitation**: While `wasip1` provides better system interface support than `js`, it still lacks CGO functionality. Go 1.24 (2025) added `go:wasmexport` and `-buildmode=c-shared` for WASI, but CGO remains unsupported.

**References**:
- [WASI support in Go](https://go.dev/blog/wasi)
- [Extensible Wasm Applications with Go](https://go.dev/blog/wasmexport)

### Conclusion: Standard Go ❌

**CGO is fundamentally unsupported for all Go WASM targets.** This is a design decision - the standard Go compiler does not include CGO support for WebAssembly.

---

## TinyGo Investigation

### What Works ✅

1. **Basic CGO to WASM**
   - TinyGo can compile simple CGO programs to WASM
   - Example: `import "C"` with inline C code compiles successfully

2. **Header Inclusion**
   - Headers in the **same directory** as .go files: ✅ Works
   - **Relative paths** like `#include "src/feather.h"`: ✅ Works
   - `#cgo CFLAGS: -I${SRCDIR}/src` directive: ❌ **Not respected**

3. **TinyGo Version**
   - Installed: v0.40.1 (supports Go 1.24+)
   - LLVM: 20.1.1
   - WASM targets: `wasm`, `wasm-unknown`

### What Doesn't Work ❌

1. **runtime/cgo.Handle**
   - The feather package uses `runtime/cgo.Handle` for C↔Go callbacks
   - TinyGo does not support `cgo.Handle` for WASM targets
   - Error: `undefined: cgo.NewHandle`, `undefined: cgo.Handle`
   - Used in: `interp_core.go:198, 209, 1058`

2. **#cgo Directives**
   - `#cgo CFLAGS` not properly respected by TinyGo
   - Must use relative paths directly in `#include` statements

## Fundamental Limitations

### Why Standard Go Fails ❌

**Standard Go does not support CGO for any WASM target** (`js/wasm` or `wasip1/wasm`). The feather package requires CGO, so compilation is impossible.

### Why TinyGo Fails ❌

**The Go feather package cannot be compiled to WASM with TinyGo** because:

1. It uses `runtime/cgo.Handle` to pass Go interpreter instances to C callbacks
2. This is essential for the Go↔C interop architecture
3. TinyGo WASM doesn't implement `cgo.Handle`
4. Removing `cgo.Handle` would require a complete redesign of the Go/C bridge

## Alternative Approaches

### Option 1: JavaScript Host (Current ✅)
Keep using the current architecture:
- C interpreter → Zig → WASM
- JavaScript provides host operations
- Works well, 66+ tests passing

### Option 2: Pure-Go Interpreter
Write a pure-Go TCL interpreter:
- No CGO dependencies
- Can compile to WASM with TinyGo
- Significant effort (rewrite entire C interpreter in Go)

### Option 3: Hybrid WASM Approach
- Compile C interpreter to WASM (current Zig approach)
- Write Go code that interfaces with WASM module
- Use `wazero` or similar to run WASM from Go
- Still requires JavaScript for browser or Node for server

### Option 4: Wait for TinyGo Support
Monitor TinyGo development for:
- `runtime/cgo.Handle` support in WASM
- Better #cgo directive support
- Issue tracker: https://github.com/tinygo-org/tinygo/issues

## Technical Details

### Test Results

```bash
# Simple CGO to WASM
tinygo build -target=wasm test-cgo.go
# ✅ Success: 134KB WASM file

# Header in same directory
tinygo build -target=wasm test-same-dir.go
# ✅ Success: finds "test-feather-header.h"

# Relative include path
tinygo build -target=wasm test-relative.go
# ✅ Success: finds "src/feather.h"

# Feather-tester with cgo.Handle
tinygo build -target=wasm ./cmd/feather-tester
# ❌ Error: undefined: cgo.NewHandle
```

### Code Locations Using cgo.Handle

```go
// interp_core.go:198
interp.handle = FeatherInterp(cgo.NewHandle(interp))

// interp_core.go:209
cgo.Handle(i.handle).Delete()

// interp_core.go:1058
return cgo.Handle(h).Value().(*InternalInterp)
```

## Recommendation

**Continue with the current JavaScript/WASM architecture** because:

1. ✅ It works well (66+ tests passing)
2. ✅ Leverages existing C interpreter
3. ✅ No fundamental limitations
4. ✅ JavaScript host is clean and maintainable
5. ❌ **Standard Go**: No CGO support for WASM (by design)
6. ❌ **TinyGo**: Missing `cgo.Handle` for WASM

**Both Go toolchains are blocked from compiling the Go host to WASM.**

## References

### Standard Go
- [Go Wiki: WebAssembly](https://go.dev/wiki/WebAssembly)
- [WASI support in Go](https://go.dev/blog/wasi)
- [Extensible Wasm Applications with Go (go:wasmexport)](https://go.dev/blog/wasmexport)
- [Go 1.24 WASM Improvements](https://cloud.google.com/blog/products/application-development/go-1-24-expands-support-for-wasm)

### TinyGo
- [TinyGo GitHub](https://github.com/tinygo-org/tinygo)
- [TinyGo Lang Support](https://tinygo.org/docs/reference/lang-support/)
- [TinyGo CGO Improvements](https://aykevl.nl/2021/11/cgo-tinygo/)
- [Using WASM | TinyGo](https://tinygo.org/docs/guides/webassembly/wasm/)

## Date

2025-12-28
