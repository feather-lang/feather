# Roadmap

## Background and project goals

tclc is an embeddable implementation of the core TCL language.

TCL was conceived at a time when most networked software was written
in C at the core, the internet was young, user expectations were looser.

It is a tiny language full of great ideas, but features that were useful
20 years ago are a hindrance today:

- I/O in the language is an obstacle, as the host is more than likely
  to already have taken a stance on how it wants to handle I/O,
- a built-in event loop for multiplexing I/O and orchestrating timers
  was useful when no host could easily provide this, but event loops
  are widespread and having to integrate multiple event loops in one
  application is error-prone.
- reference counting with lots of calls to malloc and free works great for
  standalone TCL, but the emergence of zig and wasm incentivizes being in
  control of allocations.

So what ideas are worth preserving?

A pure form of metaprogramming, syntax moldable like clay, with meaning
to be added at a time and in a form that is convenient for that particular
use case.

A transparent execution environment: every aspect of a running TCL program
can be inspected from within that program, and often even modified.

A focus on expressing computation in the form of instructions to carry out.

The latter point is key: agentic coding benefits from an inspectable and
moldable environment. Having the agent talk to your running program gives it
highly valuable feedback for a small amount of tokens.

The browser is one example of this model being successful, but what about all
the other applications? Your job runner, web server, database, your desktop
or mobile app.

tclc wants to be the thin glue layer that's easy to embed into your programs,
so that you can talk to them while they are running.

Another way to look at TCL is this: it is a Lisp-2 with fexprs that extend
to the lexical syntax level. Maybe that is more exciting.

Here you will find a faithful implementation of:

- control flow and execution primitives: proc, foreach, for, while, if,
  return, break, continue, error, tailcall, try, throw, catch, switch
- introspection capabilities: info, errorCode, errorInfo, trace
- values and expressions: expr, incr, set, unset, global, variable
- metaprogramming: upvar, uplevel, rename, unknown, namespace
- data structures: list, dict, string, apply
- string manipulation: split, subst, concat, append, regexp, regsub, join

Notable omissions (all to be covered by the host):

- Arrays: TCL-style associative arrays were supplanted by the dictionary datatype.
  There is no reason to have both in the language when starting from scratch.

- I/O: chan, puts, gets, refchan, transchan, after, vwait, update
  These are better provided by the host in the form of exposed commands.

- OO: tclc intended use case is short, interactive programs
  similar to bash. Programming in the large is explicitly not supported.

- Coroutines: tclc interpreter objects are small and lightweight so you can
  have of them if you need something like coroutines.

## Development Process

At a high-level we want to start with a small interpreter working end-to-end,
using a single host implementation until we have reached a certain complexity
threshold.

Reaching the threshold is important to establish a ground truth before adding
the burden of having to maintain another implementation.

We iterate strictly using TDD and progressively more complicated tests
that exercise more and more of the functionality.

## Milestones

Since the set of commands we and to implement is large,
picking a reasonable order is important.

### M1: just invoking commands from the host

This phase is mostly about testing the parser and making sure the
host / interpreter interop works at a basic level.

Tests in this phase make no use of special Tcl semantics beyond simple,
flat command invocation.

Builtin commands implemented:

- set, so that we can test variable and command substitution

### M2: the expression DSL

In order to build up to control flow, we need to have a working expression evaluator.

Tests start with basic comparison operators, then math operator for integers only.

This forces us to get the integer data type working.

Builtin commands implemented:

- expr, partially: <, <=, >, >=, =, !=, |, &; integers only

### M3: proc with implicit return

TODO: description, rationale

### M4: the return command with all options

TODO: description, rationale

Implemented:

- proc with explicit return
- return with all options

### M5: the error command with all options

TODO: description, rationale

Implemented:

- error with all options

### M6: the if command

TODO: description, rationale

Implemented:

- if as per `man -P cat n if`
  - optional then keyword,
  - elseif + else
  - 1, true, yes as true
  - 0, false, no as false

### M7: the while command

TODO: description, rationale

Implemented:

- while as per `man -P cat n while`
