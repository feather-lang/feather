# Feather TCL Builtin Comparison Index

This document summarizes the comparison between Feather's TCL builtin implementations and official TCL 8.6+/9.0.

## Feature-Complete Builtins (26)

These builtins match TCL's documented behavior:

- `apply` - Lambda application (with required-after-optional handling)
- `break` - Loop termination
- `continue` - Loop continuation
- `list` - List construction
- `llength` - List length
- `lreverse` - List reversal
- `concat` - List concatenation
- `join` - List to string
- `lrange` - List range extraction
- `lreplace` - List element replacement
- `linsert` - List insertion
- `lrepeat` - List repetition
- `lmap` - List mapping
- `lassign` - List assignment to variables
- `foreach` - List iteration
- `while` - While loop
- `if` - Conditional execution
- `error` - Error raising
- `throw` - Exception throwing
- `rename` - Command renaming
- `global` - Global variable access
- `eval` - Script evaluation
- `proc` - Procedure definition (with default parameters)
- `return` - Procedure return (with -code, -level, -options, -errorcode, -errorinfo, -errorstack)
- `catch` - Exception catching (with -errorinfo, -errorcode, -errorstack, -errorline, globals)
- `uplevel` - Execute script in different stack frame (with namespace and apply interaction)

## Builtins with Missing Features

| Builtin | Key Missing Features |
|---------|---------------------|
| `string` | 12 subcommands (cat, compare, equal, first, last, insert, is, repeat, replace, reverse, totitle, wordend/wordstart) |
| `dict` | filter, map, update, with subcommands |
| `info` | 14+ subcommands (cmdcount, cmdtype, complete, coroutine, class/object introspection) |
| `namespace` | 9 subcommands (code, ensemble, forget, inscope, origin, path, unknown, upvar, which) |
| `try` | -during key in exception dictionary |
| `switch` | All major features implemented |
| `format` | Size modifier truncation, some # flag behaviors |
| `scan` | Integer truncation, Unicode %c, unsigned conversion |
| `subst` | Unicode escapes (\uNNNN) |
| `trace` | Variable creation on trace add, array operation |
| `incr` | Array default values (Feather doesn't support arrays) |
| `set` | Array element syntax |
| `append` | Array default values |
| `lappend` | Array default values |
| `unset` | Array support |
| `upvar` | Array element references, validation checks |
| `tailcall` | Uplevel restriction (may not be enforced in TCL 9.0) |
| `for` | All features implemented |
| `split` | Unicode character handling |

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
