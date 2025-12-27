# Overview

This repository holds feather, a small, embeddable interpreter for a subset of TCL.

The structure is this:

src/ contains the interpreter proper. It is written in stdlib-less C:

- all allocation is managed by the embedder,
- so are all lifetimes, I/O and datastructures.

When the interpreter needs to do something,
it expresses this need through the FeatherHostOps interface.

The harness/ directory contains the test harness for
actually driving this interpreter through a host program.
Multiple host programs are included in this repository, each confined to a subdirectory.

Due to Go's packaging system, the Go host and library are
defined in the toplevel environment.

The @ROADMAP.md document details project milestones and implementation status.

This is the second iteration of this idea, and git history before
commit 6117fd9 MUST be ignored.

The initial-consultation.md document contains a record of architectural decisions
during the design phase.

## Go Package Structure

The root `feather` package is the public API. Key types:

- `*Interp` - the interpreter instance
- `*Obj` - a TCL value (supports shimmering between representations)
- `ObjType` - interface for custom shimmering types
- `Result` - command return type (OK or Error)

Users can implement custom `ObjType` for shimmering:

```go
type MyType struct { data string }

func (t *MyType) Name() string { return "mytype" }
func (t *MyType) UpdateString(o *feather.Obj) { o.SetString(t.data) }
func (t *MyType) Dup() feather.ObjType { return &MyType{data: t.data} }

obj := feather.NewObj(&MyType{data: "hello"})
```

## Shimmering Rules

TCL objects can have multiple representations (string, int, list, etc.).
"Shimmering" is the lazy conversion between these representations.

**All shimmering logic is centralized in free-standing conversion functions:**

| Function | Conversion |
|----------|------------|
| `AsString(o *Obj)` | any → string |
| `AsInt(o *Obj)` | string → int |
| `AsList(o *Obj)` | (existing list rep only) |
| `AsDict(o *Obj)` | (existing dict rep only) |
| `Interp.ParseList(s)` | string → list (requires parsing) |
| `Interp.ParseDict(s)` | string → dict (requires parsing) |

**Rules:**

1. Use `As*` functions to convert between representations
2. String-to-list/dict parsing requires the interpreter (use `Interp.ParseList`/`ParseDict`)
3. Custom types implement `ObjType` interface for shimmering support
4. Conversion interfaces (`IntoInt`, `IntoDouble`, etc.) allow custom types to participate in shimmering

## Tooling

This project uses `mise` to install dependencies, run tasks and manage the environment.

All internal tools like the test harness are written in Go,
and compiled binaries are found in bin/.

The bin/ directory is put onto PATH by mise and .gitignored

Important `mise` tasks:

- `mise build` builds all binaries and puts them into `bin/`
- `mise test` runs the test harness which ensures correct behavior

## The Test Harness

The test harness is data-driven and tests are described as XML/HTML files.

Since we will end up with a large test suite that needs
to work with multiple implementations, tests must be modeled as data.

## Working Process

Before you start any task, you must review the last commit in full
using `git show HEAD`.

src/feather.h is the authoritative source of what the interpreter expects from the host and defines the public API.

All internal forward declarations necessary should go into src/internal.h

If you encounter a situation where the provided primitives
in FeatherHostOps are not sufficient to implement the desired functionality
YOU MUST STOP IMMEDIATELY and inform me, your operator.

Only proceed in this situation after you have received formal approval
and directions from me.

At the end of an increment you must commit to git.
The commit message MUST contain all of your thinking,
important learnings, and the current state of the code.

After committing, your colleague will continue the work,
but the commit message is the only piece of information
they have access to.

## Running tests

`mise test` runs the test harness against the Go reference implementation.

`mise test:js` runs the test harness against the JavaScript/WASM host.
