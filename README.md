# The Feather Programming Language

<p align="center">
  <img src="./feather-logo.png" width="128" height="128">
</p>

This repository holds Feather: a minimal, clean implementation of the TCL core language, suitable for embedding into modern applications.

Feather stays mostly faithful to the original, but adds new functionality
where user ergonomics count, such as exposing objects from the host to Feather programs.

Omissions from Tcl 9:

- I/O functions,
- an event loop,
- an OO system.

## Feather vs Lua

Lua is great for programming extensions to software in the large.

Feather is filling the niche where you have an existing program,
and want to add an interactive console or a networked REPL for
poking and prodding the program state:

- giving agents access to your program (like the Chrome Dev Tools, but for YOUR application),
- [configuring an HTTP server while it's running](https://vinyl-cache.org/docs/6.0/reference/varnish-cli.html)
- Quake-style consoles,
- as a configuration file format,
- allowing users to customize software after you shipped it.

## Using the example

Since Feather is designed to be embedded into other programming languages,
you need a host for running feather.

The default host for feather, used for developing feather, is written in Go:

> [!CAUTION]
> The gcl (Go Command Language) interpreter provided here is only for Feather's
> internal use and will change frequently -- only use this for playing around.

```bash
# build they example interpreter in bin/gcl
mise build

# start the repl
mise exec -- gcl
```

In the REPL:

```
% set greeting "Hello, World!"
Hello, World!
% string length $greeting
13

% proc greet {name} {
    return "Hello, $name!"
}
% greet TCL
Hello, TCL!

% set numbers {1 2 3 4 5}
1 2 3 4 5
% lindex $numbers 2
3
% llength $numbers
5

% foreach n $numbers {
    if {$n > 3} { break }
    echo $n
}
1
2
3

% expr {2 + 2 * 3}
8
% set x 10
10
% incr x 5
15

% dict set config host localhost
host localhost
% dict set config port 8080
host localhost port 8080
% dict get $config port
8080
```

<details><summary>Run the test harness</summary>

```
$ mise test
[build:harness] $ cd harness
[build:core] $ mkdir -p build
[build:harness] Finished in 207.2ms
[build:core] Finished in 800.3ms
[build:gcl] $ go build -o $MISE_CONFIG_ROOT/bin/gcl ./cmd/gcl
[build:gcl] Finished in 503.8ms
[test] $ harness run --host bin/gcl testcases/
[test]
[test] 1024 tests, 1024 passed, 0 failed
[test] Finished in 9.88s
Finished in 11.19s
```

</details>

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
