# C Host Implementation

This directory contains the C host implementation for the TCL core.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      bin/tclc                            │
│                    (executable)                          │
└─────────────────────────────────────────────────────────┘
                          │
                          │ links
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    hosts/c/*.c                           │
│                 (host callbacks)                         │
│                                                          │
│  main.c ─────► Entry point, REPL, script execution      │
│  host.c ─────► TclHost callback implementations          │
│  object.c ───► TclObj creation and manipulation          │
│  vars.c ─────► Variable table (hash map)                 │
│  arena.c ────► Arena allocator for temporaries           │
│  channel.c ──► I/O channels (stdin/stdout/files)         │
└─────────────────────────────────────────────────────────┘
                          │
                          │ includes & calls
                          ▼
┌─────────────────────────────────────────────────────────┐
│                build/libtclc.a                           │
│              (static library from core/)                 │
│                                                          │
│  Public API: core/tclc.h                                 │
│  - tclInterpNew(), tclInterpFree()                      │
│  - tclEval(), tclEvalObj()                              │
│  - Built-in command dispatch                             │
└─────────────────────────────────────────────────────────┘
```

## Files

| File | Purpose |
|------|---------|
| `main.c` | Entry point. Reads script from file or stdin. |
| `host.c` | Implements all `TclHost` callbacks that the core calls back into. |
| `object.c` | `TclObj` implementation with string/int/double/list representations. |
| `vars.c` | Hash table for variable storage, supports arrays. |
| `arena.c` | Stack-based arena allocator for temporary memory during eval. |
| `channel.c` | File I/O channels (open, read, write, close). |

## Build

```bash
# Build just the C host
make build          # Creates bin/tclc

# Build with libraries
make build-all      # Creates bin/tclc + build/libtclc.{a,dylib}
```

## How Core and Host Connect

The core library (`build/libtclc.a`) is **allocation-free**. It relies on the host to provide:

1. **Memory** - All dynamic allocation goes through `TclHost` callbacks
2. **Objects** - `TclObj` is opaque to the core; host manages representation
3. **Variables** - Core calls `varGet`/`varSet` callbacks
4. **I/O** - Core calls channel callbacks for `puts`, `gets`, etc.
5. **Commands** - Core calls `cmdLookup` to find procs and extensions

The host registers its callbacks via `TclHost` struct passed to `tclInterpNew()`.

## Testing

```bash
# Run differential tests against tclsh oracle
make diff-all

# Run tests for a specific feature
make diff FEATURE=string
```

## Key Design Decisions

1. **No libc in core** - The C host provides string operations, memory, etc.
2. **Hash tables in host** - Core doesn't implement hash tables; host provides them via callbacks.
3. **Simple object model** - Objects shimmer between string/int/double/list representations.
4. **Arena for temporaries** - Parsing and eval use arena allocation, freed in bulk.
