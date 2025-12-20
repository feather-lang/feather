# Overview

This repository holds tclc, a small, embeddable interpreter for a subset of TCL.

The structure is this:

src/ contains the interpreter proper. It is written in stdlib-less C:

- all allocation is managed by the embedder,
- so are all lifetimes, I/O and datastructures.

When the interpreter needs to do something,
it expresses this need through the TclHostOps interface.

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

@src/tclc.h is the authoritative source of what the interpreter expects from the host and defines the public API.

All internal forward declarations necessary should go into src/internal.h

If you encounter a situation where the provided primitives
in TclHostOps are not sufficient to implement the desired functionality
YOU MUST STOP IMMEDIATELY and inform me, your operator.

Only proceed in this situation after you have received formal approval
and directions from me.

At the end of an increment you must commit to git.
The commit message MUST contain all of your thinking,
important learnings, and the current state of the code.

After committing, your colleague will continue the work,
but the commit message is the only piece of information
they have access to.
