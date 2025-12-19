# Go Host Implementation

This directory contains the Go host implementation for the TCL core, using CGO to interface with the C library.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      bin/tclgo                           │
│                    (executable)                          │
└─────────────────────────────────────────────────────────┘
                          │
                          │ compiled from
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    hosts/go/*.go                         │
│                   (Go host code)                         │
│                                                          │
│  main.go ────► Entry point, REPL, script execution      │
│  host.go ────► TclHost callback wrappers                 │
│  object.go ──► TclObj with Go-native representations     │
│  vars.go ────► Variable storage using Go maps            │
│  arena.go ───► Arena using Go slices                     │
│  channel.go ─► I/O channels wrapping os.File             │
└─────────────────────────────────────────────────────────┘
                          │
                          │ CGO
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    host_c.c                              │
│            (CGO bridge / callback shims)                 │
│                                                          │
│  Exports C functions that call back into Go              │
│  Required because Go cannot export function pointers     │
└─────────────────────────────────────────────────────────┘
                          │
                          │ links
                          ▼
┌─────────────────────────────────────────────────────────┐
│                build/libtclc.a                           │
│              (static library from core/)                 │
│                                                          │
│  Public API: core/tclc.h                                 │
└─────────────────────────────────────────────────────────┘
```

## Files

| File | Purpose |
|------|---------|
| `main.go` | Entry point. Handles CLI flags, runs REPL or scripts. |
| `host.go` | Go-side `TclHost` callback implementations. |
| `host_c.c` | C shims that bridge between C function pointers and Go. |
| `object.go` | `TclObj` using Go strings, slices, and interfaces. |
| `vars.go` | Variable storage using `map[string]*TclObj`. |
| `arena.go` | Arena allocator backed by Go slices (GC-managed). |
| `channel.go` | I/O channels wrapping `*os.File` and `bufio`. |

## Build

```bash
# Build the Go host (requires libtclc.a to exist)
make build-go       # Creates bin/tclgo

# Build everything
make build-all      # Creates bin/tclc, bin/tclgo, and libraries
```

## CGO Bridge

Go cannot directly export function pointers to C. The `host_c.c` file provides:

1. **Static C functions** that match `TclHost` callback signatures
2. **Go exports** called via `//export` comments in Go files
3. **Handle management** to map C pointers to Go objects

Example flow for `varGet`:
```
C core calls host->varGet(ctx, name)
    → host_c.c: goVarGet(ctx, name)
        → Go: //export goVarGet
            → vars.go: interp.vars[name]
```

## Advantages Over C Host

1. **Garbage Collection** - No manual memory management for objects
2. **Native strings** - Go strings handle Unicode properly
3. **Maps** - Variable tables use Go's built-in hash maps
4. **Error handling** - Can use Go's panic/recover for errors
5. **Concurrency** - Future support for goroutine-based interpreters

## Status

The Go host is a work in progress. See `IMPROVEMENTS.md` for planned enhancements.

## Testing

The Go host can be tested against the same oracle as the C host:

```bash
# Run differential tests with Go host
TCLC_INTERP=bin/tclgo make diff-all
```
