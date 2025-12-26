/*
 * Feather Amalgamation
 * 
 * This file includes all feather C source files into a single translation unit.
 * It enables `go install` support by letting cgo compile the C core directly.
 *
 * Usage in Go (interp/feather_core.c):
 *   #include "../src/feather_amalgamation.c"
 *
 * DO NOT include host.c - that's for WASM import-based builds only.
 */

/* Utility modules first (no dependencies) */
#include "charclass.c"
#include "index_parse.c"
#include "level_parse.c"
#include "namespace_util.c"

/* Core modules */
#include "arena.c"
#include "memory.c"
#include "glob.c"
#include "parse.c"
#include "resolve.c"
#include "interp.c"
#include "eval.c"

/* Builtins */
#include "builtin_append.c"
#include "builtin_apply.c"
#include "builtin_break.c"
#include "builtin_catch.c"
#include "builtin_concat.c"
#include "builtin_continue.c"
#include "builtin_dict.c"
#include "builtin_error.c"
#include "builtin_expr.c"
#include "builtin_for.c"
#include "builtin_foreach.c"
#include "builtin_format.c"
#include "builtin_global.c"
#include "builtin_if.c"
#include "builtin_incr.c"
#include "builtin_info.c"
#include "builtin_join.c"
#include "builtin_lappend.c"
#include "builtin_lindex.c"
#include "builtin_list.c"
#include "builtin_llength.c"
#include "builtin_lrange.c"
#include "builtin_lreplace.c"
#include "builtin_lsearch.c"
#include "builtin_lset.c"
#include "builtin_lsort.c"
#include "builtin_mathfunc.c"
#include "builtin_namespace.c"
#include "builtin_proc.c"
#include "builtin_rename.c"
#include "builtin_return.c"
#include "builtin_scan.c"
#include "builtin_set.c"
#include "builtin_split.c"
#include "builtin_string.c"
#include "builtin_subst.c"
#include "builtin_switch.c"
#include "builtin_tailcall.c"
#include "builtin_throw.c"
#include "builtin_trace.c"
#include "builtin_try.c"
#include "builtin_unset.c"
#include "builtin_uplevel.c"
#include "builtin_upvar.c"
#include "builtin_variable.c"
#include "builtin_while.c"
