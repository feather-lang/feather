# Feather TCL Builtin Comparison Index

This document summarizes the comparison between Feather's TCL builtin implementations and official TCL 8.6+/9.0.

## Feature-Complete Builtins (22)

These builtins match TCL's documented behavior:

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

## Builtins with Missing Features

| Builtin | Key Missing Features |
|---------|---------------------|
| `string` | 12 subcommands (cat, compare, equal, first, last, insert, is, repeat, replace, reverse, totitle, wordend/wordstart) |
| `dict` | filter, map, update, with subcommands |
| `info` | 14+ subcommands (cmdcount, cmdtype, complete, coroutine, class/object introspection) |
| `namespace` | 9 subcommands (code, ensemble, forget, inscope, origin, path, unknown, upvar, which) |
| `lsearch` | -sorted, -start, -stride, -index options |
| `lsort` | -dictionary, -command, -index, -indices, -stride options |
| `lindex` | end/end-N syntax, index arithmetic, nested indexing |
| `lset` | multiple/nested indices support |
| `mathfunc` | 11 functions (bool, entier, isqrt, max, min, rand, srand, isfinite, isnormal, issubnormal, isunordered) |
| `expr` | Same 11 math functions, 0d prefix |
| `apply` | Required-after-optional argument handling |
| `return` | -errorcode, -errorinfo, -errorstack, proper -options processing |
| `catch` | -errorinfo, -errorcode, -errorline, -errorstack population |
| `try` | -during key in exception dictionary |
| `switch` | -nocase, -matchvar, -indexvar |
| `format` | Size modifier truncation, some # flag behaviors |
| `scan` | Integer truncation, Unicode %c, unsigned conversion |
| `subst` | Unicode escapes (\uNNNN) |
| `trace` | Variable creation on trace add, array operation |
| `incr` | Auto-initialization of unset variables |
| `set` | Array element syntax |
| `append` | Array default values |
| `lappend` | Array default values |
| `unset` | Array support |
| `upvar` | Array element references, validation checks |
| `uplevel` | Namespace interaction, concat-style argument joining |
| `tailcall` | Namespace context resolution, uplevel restriction |
| `for` | break handling in next script |
| `variable` | Qualified names for cross-namespace access |
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
