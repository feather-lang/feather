# tclc - an exercise in agentic coding.

> [!WARNING]
> This repository is archived and just here for study.
>
> Development of the usable product happens under [feather-lang/feather](https://github.com/feather-lang/feather).

This repository holds a minimal, clean implementation of the TCL core language, suitable
for embedding into modern applications.

Omissions from Feather 9:

- I/O functions,
- an event loop,
- an OO system.

```bash
# tcl-tk is required for the reference check in oracle/
brew install tcl-tk

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

Run the test harness:

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

## ... but why?

I wanted to see how far I can take a hands-off approach to coding,
letting the agent do all the work.

[Inspiration came from within the team.](https://ampcode.com/by-an-agent-for-an-agent)

Combined with: how far can you take feedback loops?

Additionally, I wanted to create a case study others can learn from,
for building large systems from scratch, an AI-native codebase
essentially.

As for TCL: I have a soft spot for it, because command languages are
*so* useful, especially when used by an agent, that it's a shame they
don't have more adoption.

The [fish](https://fishshell.com/) people know, but how many people do you know who have
actually read the bash or zsh manual?

## What you can learn here

Learn from my mistakes!

This implementation was developed using Claude Code, in roughly the following order:

1. Ask Opus 4.5 on Claude Desktop for designing a feedback loop,
2. Whip Claude into action to actually implement it,
3. Drown in slop when it was time to implement coroutines.

After step 3, I deleted everything and started from scratch, DHH style:

1. A long conversation with Opus 4.5 in the Claude Desktop app (see [./initial-consultation.md](./initial-consultation.md));
2. Define the scope: from the slop phase I realized I don't want all
   of TCL, but a subset;
3. TYPE OUT THE INTERFACE in [src/feather.h](./src/tclc.h) -- this actually made me spot more holes in the design and keep a good mental model of the system;
4. Building out a test harness;
5. Defining a roadmap for features of increasing complexity, to tackle
   all of the challenges that have implications for the core interpreter.
6. Ad infinitum: turning the crank until we reached feature parity.

You can find all conversations used with Claude in [.claude/conversations](.claude/conversations)
