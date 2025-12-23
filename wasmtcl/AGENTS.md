# wasmtcl - WASM-based TCL Host

This package provides a pure-Go TCL interpreter host using WebAssembly.
It wraps the tclc C interpreter compiled to WASM via wazero.

## Architecture

```
┌─────────────────────────────────────────────┐
│               Go Host (wasmtcl)             │
│  ┌─────────────────────────────────────┐   │
│  │    Interp (state management)         │   │
│  │  - Objects with shimmering           │   │
│  │  - Call frames & variables           │   │
│  │  - User-defined procedures           │   │
│  └──────────────┬──────────────────────┘   │
│                 │                           │
│  ┌──────────────▼──────────────────────┐   │
│  │    wazero Runtime                    │   │
│  │  - tclc.wasm module                  │   │
│  │  - TclHostOps imports (callbacks)    │   │
│  └──────────────┬──────────────────────┘   │
│                 │                           │
└─────────────────┼───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│           tclc.wasm (WASM module)           │
│  - tcl_script_eval                          │
│  - tcl_interp_init                          │
│  - tcl_parse_command                        │
│  - (calls back to host via TclHostOps)      │
└─────────────────────────────────────────────┘
```

## Directory Structure

- `wasm/` - WASM runtime setup and memory helpers
- `extensions/` - Extension module loader for .wasm plugins
- `cmd/wgcl/` - CLI wrapper (mirrors cmd/gcl)

## Building

```bash
mise run build:wasm      # Compile tclc.wasm
mise run build:wgcl      # Build wgcl CLI
mise run test:wgcl       # Run test harness against wgcl
```

## Shimmering Rules

Same rules as interp/AGENTS.md apply here:

| Method | Conversion |
|--------|------------|
| `GetString(h TclObj)` | int/list → string |
| `GetInt(h TclObj)` | string → int |
| `GetList(h TclObj)` | string → list |

All shimmering logic MUST be centralized in `*Interp` methods.

## TclHostOps Callbacks

The WASM module imports functions matching `TclHostOps` from src/tclc.h.
These are implemented in `wasm/imports.go` and delegate to `*Interp` methods.

Import function naming convention: `tclc.{category}_{operation}`
Example: `tclc.string_intern`, `tclc.list_push`, `tclc.var_get`

## Extension Modules

Extensions are separate .wasm files that can register custom commands.

Extension ABI:
- Export: `tcl_register(interp i32)` - called on load
- Import: `env.register_command(name_ptr, name_len, cmd_id i32)`
- Export: `tcl_cmd_<id>(interp, cmd, args i32) -> i32` per command

## Differences from interp/ (CGo host)

1. No CGo - pure Go using wazero
2. Memory management via WASM linear memory
3. String passing requires explicit copy to/from WASM memory
4. All callbacks are WASM import functions
