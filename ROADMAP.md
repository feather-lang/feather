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

Since the set of commands we want to implement is large,
picking a reasonable order is important.

The guiding principle: each milestone should add exactly one new concept to the interpreter's execution model. We front-load features that affect the interpreter's core logic (call frames, return codes, variable scoping) before features that are merely new commands using existing machinery.

### M1: Just Invoking Commands from the Host ✓

This phase is mostly about testing the parser and making sure the
host / interpreter interop works at a basic level.

Tests in this phase make no use of special Tcl semantics beyond simple,
flat command invocation.

Builtin commands implemented:

- set, so that we can test variable and command substitution

### M2: The Expression DSL ✓

In order to build up to control flow, we need to have a working expression evaluator.

Tests start with basic comparison operators, then math operators for integers only.

This forces us to get the integer data type working.

Builtin commands implemented:

- expr, partially: <, <=, >, >=, ==, !=, |, &, ||, &&; integers only

### M3: Proc with Implicit Return ✓

With parsing complete and expressions working, we can define user procedures.

TCL procedures return the result of their last evaluated command automatically—no explicit `return` is needed. This "implicit return" is the default behavior:

```tcl
proc double {x} {
    expr {$x * 2}
}
double 5  ;# returns 10
```

This milestone establishes the core procedure mechanism:

- Binding a name to a parameter list and body
- Creating a new call frame when invoked
- Evaluating the body in that frame
- Returning the final result to the caller

We defer explicit `return` to M4 because implicit return exercises the full call/return lifecycle with minimal complexity.

Builtin commands implemented:

- proc: define named procedures with parameters

### M4: Control Flow with break and continue ✓

Before implementing `return` with its full option machinery, we introduce the simpler loop control commands. This forces us to handle non-OK return codes propagating through the interpreter, but with simpler semantics than `return -code`.

The key insight: `break` and `continue` are return codes that travel up the call stack until caught by a loop construct. This is the same machinery `return` will need, but constrained to a single, obvious catching point.

```tcl
proc find_first_negative {lst} {
    set result ""
    set i 0
    set len [llength $lst]
    while {$i < $len} {
        set x [lindex $lst $i]
        if {$x < 0} {
            set result $x
            break
        }
        incr i
    }
    set result
}
```

Builtin commands implemented:

- if: conditional with then/elseif/else, boolean literals (1/0, true/false, yes/no)
- while: loop with condition, must catch break/continue
- break: return TCL_BREAK to be caught by enclosing loop
- continue: return TCL_CONTINUE to be caught by enclosing loop
- incr: increment variable by amount (default 1)
- llength, lindex: to have basic list access

### M5: The return Command ✓

With non-OK return codes working for loops, we add the general `return` command. The critical feature is `-code`: it allows a procedure to return any code, not just TCL_OK or TCL_RETURN.

```tcl
proc my_break {} {
    return -code break
}

proc my_error {msg} {
    return -code error $msg
}
```

This milestone also introduces `-level`, which controls how many call frames the return code should travel through before taking effect. This is essential for writing control-flow abstractions.

Builtin commands implemented:

- return: with -code and -level options

### M6: Error Handling ✓

With return codes fully working, we can implement proper error handling. The `error` command is the producer; `catch` is the consumer.

```tcl
proc divide {a b} {
    if {$b == 0} {
        error "division by zero"
    }
    expr {$a / $b}
}

if {[catch {divide 10 0} result]} {
    set result "error caught"
}
```

The `catch` command is where introspection begins: it captures not just whether an error occurred, but optionally the return options dictionary containing the error code, stack trace, etc.

Builtin commands implemented:

- error: raise an error with message, optional errorInfo and errorCode
- catch: execute script, capture return code and result

The error needs to be stored in the interpreters error value, cf 

### M7: Introspection with info ✓

The `info` command is TCL's window into itself. We implement a minimal subset that exercises the key introspectable state:

```tcl
info exists varName     ;# does variable exist?
info level ?number?     ;# call stack depth or frame info
info commands ?pattern? ;# list available commands
info procs ?pattern?    ;# list user-defined procedures
info body procName      ;# get procedure body
info args procName      ;# get procedure parameters
```

This forces the interpreter to expose its internal state in a structured way. The `info level` subcommand is particularly important: it reveals the call stack, which is foundational for `uplevel` and `upvar`.

Builtin commands implemented:

- info: exists, level, commands, procs, body, args subcommands

### M8: Metaprogramming with uplevel and upvar ✓

These commands break the normal scoping rules, allowing code to execute in or access variables from calling frames. They are the heart of TCL's metaprogramming capability.

```tcl
proc localvar {name value} {
    upvar 1 $name var
    set var $value
}

proc debug_eval {script} {
    uplevel 1 $script
}
```

`upvar` creates an alias between a local variable and a variable in another frame. `uplevel` evaluates a script in a calling frame's context. Together they enable any control structure to be written as a procedure.

Builtin commands implemented:

- upvar: link local variable to variable in another frame
- uplevel: evaluate script in a calling frame

### M9: The unknown Handler ✓

When a command is not found, TCL calls `unknown` with the original command and arguments. This enables auto-loading, abbreviation expansion, and domain-specific command resolution.

```tcl
proc unknown {cmd args} {
    if {[string match "get*" $cmd]} {
        return [dict get $::data [string range $cmd 3 end]]
    }
    error "invalid command name \"$cmd\""
}
```

This milestone exercises the bind.unknown hook in TclHostOps and demonstrates how the interpreter delegates to user code for missing commands.

Builtin commands implemented:

- rename: rename or delete commands (needed to replace `unknown`)
- The `unknown` mechanism via TclBindOps

## M10: Namespaces

Namespace support tests symbol table scoping and procedure resolution:

- namespace: eval, current, exists, children, parent, delete, export, import
- variable: declare namespace variables

## M11: Advanced Error Handling

Structured exception handling validates the interpreter's frame and result management:

- try/on/trap/finally: structured exception handling
- throw: raise typed exceptions

## M12: Trace Support

Traces exercise the interpreter's hook points and introspection capabilities:

- trace: add, remove, info for variable and command traces
- Complete info subcommands: frame, default, locals, globals, vars, script

## M13: Completing the Control Flow Suite

With error handling, namespaces, and tracing validated, we round out control flow:

- for: C-style loop
- foreach: iteration over lists
- switch: multi-way branch with -exact, -glob, -regexp modes
- tailcall: replace current frame with new command

## M14: Full Expression Support

Extend `expr` to its complete form:

- Floating point numbers
- Mathematical functions (sin, cos, sqrt, etc.)
- String comparisons (eq, ne)
- Ternary operator (? :)
- List membership (in, ni)

## M15: String and List Operations

Complete the data structure commands:

- list: construct lists
- lindex, lrange, llength, lappend, lset, lreplace, lsort, lsearch
- string: length, index, range, match, map, trim, toupper, tolower
- split, join, concat, append

## M16: Dictionary Support

- dict: create, get, set, exists, keys, values, for, map, filter, remove, merge
