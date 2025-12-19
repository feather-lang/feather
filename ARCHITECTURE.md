# TCL Core Architecture

This document describes the architecture of TCLC, a reimplementation of the TCL 9 interpreter core in C, designed to be embedded in a Go host.

## Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              Go Host                                     │
│                                                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │   Arena     │  │   Object    │  │  Variable   │  │   Channel   │    │
│  │  Allocator  │  │   Store     │  │   Tables    │  │   Manager   │    │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘    │
│         │                │                │                │            │
│         └────────────────┴────────────────┴────────────────┘            │
│                                   │                                      │
│                          TclHost callbacks                               │
│                                   │                                      │
└───────────────────────────────────┼──────────────────────────────────────┘
                                    │
                                    │ CGO
                                    │
┌───────────────────────────────────┼──────────────────────────────────────┐
│                                   │                                      │
│                              C Core                                      │
│                                                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │   Lexer     │  │   Parser    │  │    Eval     │  │  Builtins   │    │
│  │             │  │             │  │ (trampoline)│  │             │    │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘    │
│                                                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │    Subst    │  │    Expr     │  │  Coroutine  │  │   Interp    │    │
│  │             │  │             │  │   Manager   │  │   Manager   │    │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## Design Principles

### 1. No Preprocessor Constants

The C core does not use `#define` for constants because preprocessor macros are invisible to the compiler and make generating bindings (e.g., for Go via CGO) difficult. Instead:

- **Use `enum` for integer constants** - Enums are proper C symbols visible to tooling
- **Use `static const` for typed constants** - When a specific type is needed
- **Const enums work for array sizing** - `enum { BUFFER_SIZE = 256 };` is valid for array declarations

```c
// BAD - invisible to binding generators
#define TCL_OK 0
#define TCL_ERROR 1
#define MAX_WORDS 128

// GOOD - visible to compilers and binding tools
typedef enum {
    TCL_OK = 0,
    TCL_ERROR = 1,
    TCL_RETURN = 2,
    TCL_BREAK = 3,
    TCL_CONTINUE = 4,
} TclResult;

enum { MAX_WORDS = 128 };
```

### 2. No Allocation in C

The C core does not call `malloc`, `free`, or any standard library allocation functions. All dynamic memory is managed by the Go host through callbacks:

- **Arena allocation** for parse/eval temporaries (LIFO, bulk free)
- **GC-managed objects** for TCL values (strings, lists, dicts)
- **Persistent frames** for activation records (coroutine support)

This simplifies memory management and allows Go's GC to handle all object lifetimes.

### 3. No Standard Library in C

The C core avoids libc dependencies except for:
- `<stddef.h>` - `size_t`, `NULL`
- `<stdint.h>` - Fixed-width integers
- `<math.h>` - Math functions for `expr`

All I/O, string operations, and system calls go through host callbacks.

### 4. Host-Managed Data Structures

The host provides all complex data structures:
- **TclObj** - Opaque value type with type-specific operations
- **Variable tables** - Hash maps for variable storage
- **Command tables** - Proc and extension registration
- **Channels** - Buffered I/O with encoding support

The C core only sees opaque handles and calls through function pointers.

### 5. Trampoline Evaluation

The eval loop is non-recursive to support coroutines:

```c
typedef enum {
    EVAL_CONTINUE,   // More work to do
    EVAL_DONE,       // Result ready
    EVAL_YIELD,      // Coroutine yielding
} EvalStatus;

while (1) {
    EvalStatus status = evalStep(interp, &state);
    switch (status) {
        case EVAL_CONTINUE: continue;
        case EVAL_DONE:     return result;
        case EVAL_YIELD:    saveState(); return;
    }
}
```

This allows coroutine state to be captured and resumed without C stack manipulation.

## Component Details

### Lexer (`core/lexer.c`)

Tokenizes TCL source into words.

**Input:** UTF-8 string  
**Output:** Word boundaries and types

**Key responsibilities:**
- Track quoting context (none, braces, double quotes)
- Handle backslash-newline continuation
- Detect command boundaries (newline, semicolon)
- Identify comments

**No allocation required** - operates on input buffer, outputs indices.

### Parser (`core/parser.c`)

Builds command structure from tokens.

**Input:** Token stream  
**Output:** Command array

**Key responsibilities:**
- Group words into commands
- Track source locations for error messages
- Handle nested structures

**Allocation:** Uses arena for temporary parse structures.

### Substitution (`core/subst.c`)

Performs `$var`, `[cmd]`, and `\x` substitution.

**Key responsibilities:**
- Detect substitution markers
- Recursive eval for command substitution
- Backslash escape processing
- Unicode escape handling (`\uXXXX`)

**Allocation:** Uses arena for intermediate strings, returns host-allocated TclObj.

### Expression Evaluator (`core/builtin_expr.c`)

Parses and evaluates `expr` expressions.

**Key responsibilities:**
- Operator precedence parsing
- Type coercion (string ↔ number)
- Math function dispatch
- Short-circuit evaluation (`&&`, `||`)

**Implementation:** Pratt parser or shunting-yard algorithm.

### Eval Trampoline (`core/eval.c`)

Non-recursive command evaluation.

**State machine phases:**
1. **PARSE** - Get next command
2. **SUBST** - Substitute words
3. **LOOKUP** - Find command
4. **DISPATCH** - Call command
5. **RESULT** - Handle return code

**Key responsibilities:**
- Maintain eval state for suspend/resume
- Handle return codes (OK, ERROR, RETURN, BREAK, CONTINUE)
- Support `uplevel` by switching frames

### Builtins (`core/builtins.c`)

Static table of built-in commands.

**Organization:**
```c
static const BuiltinEntry builtins[] = {
    {"append",   cmdAppend},
    {"break",    cmdBreak},
    {"catch",    cmdCatch},
    // ... sorted alphabetically for binary search
};
```

**Dispatch:**
1. Binary search in builtin table
2. If miss, call `cmdLookup` callback
3. Invoke appropriate handler

### Coroutine Manager (`core/coro.c`)

Manages coroutine creation, suspension, and resumption.

**Coroutine state:**
```c
typedef struct Coroutine {
    TclObj    *name;         // Coroutine command name
    TclFrame  *baseFrame;    // Bottom of coroutine's stack
    TclFrame  *topFrame;     // Current frame when suspended
    EvalState  evalState;    // Saved eval position
    int        status;       // RUNNING, SUSPENDED, DONE
} Coroutine;
```

**Operations:**
- `coroutine name cmd args` - Create and start
- `yield value` - Suspend, return value to caller
- `name args` - Resume with new args

### Interpreter Manager (`core/interp.c`)

Manages child interpreters.

**Key responsibilities:**
- Create child interpreter contexts
- Route `interp eval` to child
- Manage aliases between interpreters
- Enforce safe interpreter restrictions

**Safe interpreter hidden commands:**
- `exec`, `open`, `socket` (I/O)
- `file delete`, `file rename` (destructive)
- `exit` (process control)
- `load` (native code)

## Host Interface

See `core/tclc.h` for the complete interface definition.

### Callback Categories

| Category | Purpose | Example |
|----------|---------|---------|
| Context | Per-interpreter state | `interpContextNew`, `interpContextFree` |
| Frames | Activation records | `frameAlloc`, `frameFree` |
| Objects | TCL values | `newString`, `asInt`, `listIndex` |
| Arena | Temporary allocation | `arenaPush`, `arenaAlloc`, `arenaPop` |
| Variables | Storage | `varGet`, `varSet`, `arrayNames` |
| Commands | Dispatch | `cmdLookup`, `procRegister`, `extInvoke` |
| Channels | I/O | `chanOpen`, `chanRead`, `chanGets` |
| Events | Event loop | `afterMs`, `fileeventSet`, `doOneEvent` |
| Process | Subprocess | `processSpawn`, `processWait` |
| System | OS interface | `chdir`, `getcwd`, `fileExists` |

### Object Lifecycle

```
Host creates: newString("hello", 5)
     │
     ▼
┌──────────────────┐
│     TclObj       │
│                  │
│  String rep ───────► "hello"
│  Int rep    ───────► (not computed)
│  List rep   ───────► (not computed)
│                  │
└──────────────────┘
     │
     │ C calls: asInt(obj, &val)
     ▼
┌──────────────────┐
│     TclObj       │
│                  │
│  String rep ───────► "hello"
│  Int rep    ───────► ERROR (not a number)
│                  │
└──────────────────┘
     │
     │ (No longer referenced)
     ▼
  Go GC collects
```

### Frame Lifecycle (Coroutine Support)

Normal execution:
```
eval "foo"
  │
  ├── frameAlloc() → frame1
  │     │
  │     └── varsNew() → vars1
  │
  ├── [execute foo]
  │
  ├── varsFree(vars1)
  │
  └── frameFree(frame1)
```

Coroutine suspend:
```
coroutine coro { yield 42; return 100 }
  │
  ├── frameAlloc() → coroFrame
  │
  ├── [execute until yield]
  │
  ├── SAVE coroFrame (don't free!)
  │
  └── return to caller
```

Coroutine resume:
```
coro  ;# invoke coroutine
  │
  ├── RESTORE coroFrame
  │
  ├── [continue execution]
  │
  ├── varsFree(coroFrame->vars)
  │
  └── frameFree(coroFrame)
```

## Error Handling

### Error Propagation

```c
TclResult doSomething(TclInterp *interp) {
    TclResult r = subOperation(interp);
    if (r != TCL_OK) {
        tclAddErrorInfo(interp, "\n    while doing something", -1);
        return r;
    }
    // ...
    return TCL_OK;
}
```

### Error Information

```c
struct TclInterp {
    TclObj   *result;      // Error message or return value
    TclObj   *errorInfo;   // Stack trace
    TclObj   *errorCode;   // Machine-readable: {CATEGORY DETAIL ...}
    int       errorLine;   // Source line number
};
```

### Return Codes

| Code | Value | Meaning |
|------|-------|---------|
| `TCL_OK` | 0 | Success |
| `TCL_ERROR` | 1 | Error occurred |
| `TCL_RETURN` | 2 | `return` command |
| `TCL_BREAK` | 3 | `break` command |
| `TCL_CONTINUE` | 4 | `continue` command |

## String Handling

### Encoding

All strings in the C core are UTF-8. The host handles:
- Encoding conversion at I/O boundaries
- Unicode-aware string length (code points, not bytes)
- Case folding for `string tolower/toupper`

### Shimmering

TCL objects cache multiple representations:

```go
type TclObj struct {
    stringRep   *string    // Always available
    intRep      *int64     // Computed on demand
    doubleRep   *float64   // Computed on demand
    listRep     []*TclObj  // Computed on demand
}
```

When C calls `asInt(obj, &val)`:
1. Host checks if `intRep` is cached
2. If not, parses `stringRep`
3. Caches result for future calls
4. Returns success/failure

## Testing Strategy

### Unit Tests (C)

Mock host implementation for fast iteration:

```c
// test/mock_host.c
static TclObj* mockNewString(const char *s, size_t len) {
    MockObj *obj = &objPool[objCount++];
    obj->str = strndup(s, len);
    return (TclObj*)obj;
}

static const char* mockGetStringPtr(TclObj *obj, size_t *len) {
    MockObj *m = (MockObj*)obj;
    *len = strlen(m->str);
    return m->str;
}
```

### Integration Tests (Go)

Real host with tclsh9 oracle:

```go
func TestSubstitution(t *testing.T) {
    tests := loadOracle("subst")
    interp := NewInterp()
    
    for _, tc := range tests {
        result := interp.Eval(tc.Script)
        if result != tc.Expected {
            t.Errorf("%s: got %q, want %q", tc.Name, result, tc.Expected)
        }
    }
}
```

## Directory Structure

```
tclc/
├── ARCHITECTURE.md          # This file
├── Makefile                 # Build orchestration
│
├── core/                    # C core library (no main)
│   ├── tclc.h              # Host interface (public API)
│   ├── internal.h          # Internal declarations
│   ├── lexer.c             # Tokenizer
│   ├── parser.c            # Command parser
│   ├── subst.c             # Substitution engine
│   ├── eval.c              # Evaluation trampoline
│   ├── builtins.c          # Built-in commands
│   ├── builtin_dict.c      # dict command
│   ├── builtin_expr.c      # Expression evaluator
│   ├── builtin_global.c    # global command
│   ├── builtin_string.c    # string command
│   ├── builtin_upvar.c     # upvar command
│   ├── builtin_uplevel.c   # uplevel command
│   └── builtin_chan.c      # chan command
│
├── hosts/                   # Host implementations
│   ├── c/                  # C host (primary)
│   │   ├── host.c          # TclHost callback implementations
│   │   ├── object.c        # TclObj implementation
│   │   ├── vars.c          # Variable tables
│   │   ├── arena.c         # Arena allocator
│   │   ├── channel.c       # I/O channels
│   │   └── main.c          # Entry point
│   │
│   └── go/                 # Go host (CGO wrapper)
│       ├── host.go         # TclHost callback implementations
│       ├── object.go       # TclObj implementation
│       ├── vars.go         # Variable tables
│       ├── arena.go        # Arena allocator
│       ├── channel.go      # I/O channels
│       └── main.go         # Entry point
│
├── build/                   # Build artifacts (generated)
│   ├── *.o                 # Object files
│   ├── libtclc.a           # Static library
│   └── libtclc.dylib       # Dynamic library
│
├── bin/                     # Executables (generated)
│   ├── tclc                # C host interpreter
│   ├── tclgo               # Go host interpreter
│   └── harness             # Test harness
│
├── spec/                    # Specifications
│   ├── features.yaml
│   └── features/           # Per-feature docs
│
└── harness/                 # Testing infrastructure
    ├── README.md
    ├── AGENT_PROMPT.md
    ├── FEATURES.md
    ├── oracle/             # Expected outputs from tclsh
    └── tests/              # Test scripts
```

## Future Considerations

### Performance Optimization

1. **Bytecode compilation** - Compile frequently-executed scripts
2. **Command resolution caching** - Cache lookup results per frame
3. **Object pooling** - Reduce allocation pressure for common objects
4. **Batch CGO calls** - Reduce boundary-crossing overhead

### Feature Extensions

1. **Namespaces** - Full namespace support with import/export
2. **TclOO** - Object-oriented extension (currently out of scope)
3. **Threads** - Thread extension with message passing
4. **Custom channels** - Script-defined channel types

### Platform Support

1. **WASM** - Compile C core to WebAssembly
2. **Other hosts** - Rust, Python, etc. via C FFI
3. **Embedded** - Minimal host for microcontrollers
