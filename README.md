# tclc - a clean TCL core implementation

This repository holds a minimal, clean implementation of the TCL core language, suitable
for embedding into modern applications.

Omissions from Tcl 9:

- I/O functions,
- an event loop,
- an OO system,

## Philosophy

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
moldable environment.  Having the agent talk to your running program gives it
highly valuable feedback for a small amount of tokens.

The browser is one example of this model being successful, but what about all
the other applications? Your job runner, web server, database, your desktop
or mobile app.

tclc wants to be the thin glue layer that's easy to embed into your programs,
so that you can talk to them while they are running.

Another way to look at TCL is this: it is a Lisp-2 with fexprs that extend
to the lexical syntax level.  Maybe that is more exciting.

Here you will find a faithful implementation of:

- control flow and execution primitives: proc, foreach, for, while, if,
return, break, continue, error, tailcall, try, throw, catch, switch
- introspection capabilities: info, errorCode, errorInfo, trace
- values and expressions: expr, incr, set, unset, global, variable
- metaprogramming: upvar, uplevel, rename, unknown, namespace
- data structures: list, dict, string, apply
- string manipulation: split, subst, concat, append, regexp, regsub, join

Notable omissions (all to be covered by the host):

- I/O: chan, puts, gets, refchan, transchan, after, vwait, update
  These are better provided by the host in the form of exposed commands.

- OO: tclc intended use case is short, interactive programs
similar to bash. Programming in the large is explicitly not supported.

- Coroutines: tclc interpreter objects are small and lightweight so you can
have of them if you need something like coroutines.

Notables qualities of the implementation:

This implementation is pure: it does not directly perform I/O or allocation
or interact with the kernel at all. It only provides TCL parsing and
semantics.

All memory is allocated, accessed, and released by the embedding host.
The embedding host is highly likely to already have all the building blocks
we care about in the implementation and there is little value in building
our own version of regular expressions, lists, dictionaries, etc.

While this requires the host to implement a large number of functions, the
implementation is largely mechanical, which makes it a prime candidate
for delegating to agentic coding tools.

