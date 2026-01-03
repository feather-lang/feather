# Feather TCL Builtin Comparison Index

This document summarizes the comparison between Feather's TCL builtin implementations and official TCL 8.6+/9.0.

## Feature-Complete Builtins (46)

These builtins match TCL's documented behavior:

- `append` - Variable append
- `apply` - Lambda application (with required-after-optional handling)
- `break` - Loop termination
- `catch` - Exception catching (with -errorinfo, -errorcode, -errorstack, -errorline, globals)
- `concat` - List concatenation
- `continue` - Loop continuation
- `dict` - Dictionary operations (all 21 subcommands)
- `error` - Error raising
- `eval` - Script evaluation
- `expr` - Expression evaluation (all operators, math functions)
- `for` - C-style for loop (with break/continue in next script)
- `foreach` - List iteration
- `format` - String formatting (all format specifiers)
- `global` - Global variable access
- `if` - Conditional execution
- `incr` - Variable increment
- `join` - List to string
- `lappend` - List append to variable
- `lassign` - List assignment to variables
- `lindex` - List element access (with full index expression support)
- `linsert` - List insertion
- `list` - List construction
- `llength` - List length
- `lmap` - List mapping
- `lrange` - List range extraction
- `lrepeat` - List repetition
- `lreplace` - List element replacement
- `lreverse` - List reversal
- `lsearch` - List searching (all modes and options)
- `lset` - List element modification (with nested support)
- `lsort` - List sorting (all modes and options)
- `proc` - Procedure definition (with default parameters)
- `rename` - Command renaming
- `return` - Procedure return (with -code, -level, -options, -errorcode, -errorinfo, -errorstack)
- `scan` - String parsing (all format specifiers)
- `set` - Variable get/set
- `split` - String splitting (with full Unicode support)
- `subst` - String substitution (all types and options)
- `switch` - Pattern matching (with -exact, -glob, -regexp, -nocase, -matchvar, -indexvar)
- `throw` - Exception throwing
- `try` - Exception handling (with -during key in exception dictionary)
- `unset` - Variable deletion
- `uplevel` - Execute script in different stack frame (with namespace and apply interaction)
- `upvar` - Variable linking
- `variable` - Namespace variable declaration (with qualified names)
- `while` - While loop

## Builtins with Missing Features

| Builtin | Key Missing Features |
|---------|---------------------|
| `string` | Range arguments for toupper/tolower (first/last parameters parsed but ignored) |
| `info` | 14+ subcommands (cmdcount, cmdtype, complete, coroutine, class/object introspection, hostname, library) |
| `namespace` | 4 subcommands (ensemble, path, unknown, upvar) |
| `trace` | Variable creation on trace add |
| `tailcall` | Uplevel restriction (may not be enforced in TCL 9.0) |

**Note:** TCL arrays are explicitly not supported in Feather. Array-related features in `set`, `unset`, `append`, `lappend`, `incr`, `upvar`, and `trace` are intentionally omitted.

## Documentation Files

Each builtin has detailed documentation in `docs/builtin-<name>.md`:

- [append](builtin-append.md)
- [apply](builtin-apply.md)
- [break](builtin-break.md)
- [catch](builtin-catch.md)
- [concat](builtin-concat.md)
- [continue](builtin-continue.md)
- [dict](builtin-dict.md)
- [error](builtin-error.md)
- [eval](builtin-eval.md)
- [expr](builtin-expr.md)
- [for](builtin-for.md)
- [foreach](builtin-foreach.md)
- [format](builtin-format.md)
- [global](builtin-global.md)
- [if](builtin-if.md)
- [incr](builtin-incr.md)
- [info](builtin-info.md)
- [join](builtin-join.md)
- [lappend](builtin-lappend.md)
- [lassign](builtin-lassign.md)
- [lindex](builtin-lindex.md)
- [linsert](builtin-linsert.md)
- [list](builtin-list.md)
- [llength](builtin-llength.md)
- [lmap](builtin-lmap.md)
- [lrange](builtin-lrange.md)
- [lrepeat](builtin-lrepeat.md)
- [lreplace](builtin-lreplace.md)
- [lreverse](builtin-lreverse.md)
- [lsearch](builtin-lsearch.md)
- [lset](builtin-lset.md)
- [lsort](builtin-lsort.md)
- [mathfunc](builtin-mathfunc.md)
- [namespace](builtin-namespace.md)
- [proc](builtin-proc.md)
- [rename](builtin-rename.md)
- [return](builtin-return.md)
- [scan](builtin-scan.md)
- [set](builtin-set.md)
- [split](builtin-split.md)
- [string](builtin-string.md)
- [subst](builtin-subst.md)
- [switch](builtin-switch.md)
- [tailcall](builtin-tailcall.md)
- [throw](builtin-throw.md)
- [trace](builtin-trace.md)
- [try](builtin-try.md)
- [unset](builtin-unset.md)
- [uplevel](builtin-uplevel.md)
- [upvar](builtin-upvar.md)
- [variable](builtin-variable.md)
- [while](builtin-while.md)
