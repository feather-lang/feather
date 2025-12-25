# Philosophy

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

feather wants to be the thin glue layer that's easy to embed into your programs,
so that you can talk to them while they are running.

Another way to look at TCL is this: it is a Lisp-2 with fexprs that extend
to the lexical syntax level. Maybe that is more exciting.
