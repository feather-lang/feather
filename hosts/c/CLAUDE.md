# C Host Implementation

This directory contains the C host implementation for the TCL core using GLib-2.0.

## Dependencies

- **GLib-2.0**: Used for hash tables, memory management, string utilities, and other core data structures.
  - Install on macOS: `brew install glib`
  - Install on Debian/Ubuntu: `apt install libglib2.0-dev`

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
│  vars.c ─────► Variable table (GHashTable)               │
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
| `main.c` | Entry point. Reads script from file or stdin. Uses `g_file_get_contents()` for file I/O. |
| `host.c` | Implements all `TclHost` callbacks. Uses `GHashTable` for proc storage. |
| `object.c` | `TclObj` implementation with `g_malloc`/`g_free`, `g_strndup`, `GPtrArray`. |
| `vars.c` | `GHashTable` for variable storage, supports arrays and variable linking (upvar/global). |
| `arena.c` | Stack-based arena allocator using GLib memory functions. |
| `channel.c` | File I/O channels using `g_fopen`, `GString` for line reading. |
| `Makefile` | Local build configuration with `pkg-config` for GLib. |

## Build

```bash
# Build from hosts/c directory
cd hosts/c
make              # Creates ../../bin/tclc

# Or build from project root
make build        # Creates bin/tclc

# Show build configuration
make info
```

## GLib Usage

The host implementation uses these GLib features:

| Feature | Usage |
|---------|-------|
| `GHashTable` | Variable tables (`vars.c`), procedure storage (`host.c`) |
| `GPtrArray` | List parsing, collecting elements |
| `GString` | Building result strings, reading lines |
| `g_malloc/g_free/g_new0` | All memory allocation |
| `g_strndup/g_strdup_printf` | String manipulation |
| `g_ascii_strtoll/g_ascii_strtod` | Number parsing |
| `g_ascii_strcasecmp/g_ascii_tolower` | Case-insensitive comparisons |
| `g_file_get_contents` | Reading files |

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

# Quick test
echo 'puts "Hello!"' | bin/tclc
```

## Key Design Decisions

1. **GLib for data structures** - Uses battle-tested `GHashTable` instead of custom hash tables
2. **GLib for memory** - Uses `g_malloc`/`g_free` throughout for consistency
3. **Simple object model** - Objects shimmer between string/int/double/list representations
4. **Arena for temporaries** - Parsing and eval use arena allocation, freed in bulk
5. **pkg-config integration** - Build automatically finds GLib headers and libraries
