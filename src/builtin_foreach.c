#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_foreach(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  FeatherResult rc = feather_foreach_impl(ops, interp, args, "foreach", NULL, NULL);

  if (rc == TCL_OK) {
    // foreach returns empty string on normal completion
    FeatherObj emptyStr = ops->string.intern(interp, "", 0);
    ops->interp.set_result(interp, emptyStr);
  }

  return rc;
}

void feather_register_foreach_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Iterate over elements of one or more lists",
    "The foreach command iterates over one or more lists, assigning values to loop "
    "variables and executing the command body for each iteration.\n\n"
    "For each iteration, the next value from each list is assigned to the corresponding "
    "variable in varList, then command is executed. When a list is exhausted but others "
    "still have elements, empty strings are assigned to variables from the exhausted list.\n\n"
    "The foreach command supports multiple iteration patterns:\n\n"
    "- Single variable: foreach x {a b c} { ... } assigns each element in turn.\n\n"
    "- Multiple variables per list: foreach {i j} {a b c d} { ... } assigns pairs of elements.\n\n"
    "- Parallel iteration: foreach i {a b c} j {d e f} { ... } iterates both lists in parallel.\n\n"
    "- Combined form: foreach i {a b c} {j k} {d e f g} { ... } combines both patterns.\n\n"
    "The command returns an empty string on normal completion. The break command can be used "
    "to exit the loop early, and continue can skip to the next iteration.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<varList>");
  e = feather_usage_help(ops, interp, e,
    "A variable name or list of variable names to assign values from the corresponding list. "
    "Must be non-empty.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e,
    "A list of values to iterate over.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?varList?...");
  e = feather_usage_help(ops, interp, e,
    "Additional variable names or lists of variable names for parallel iteration.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?list?...");
  e = feather_usage_help(ops, interp, e,
    "Additional lists of values for parallel iteration.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<command>");
  e = feather_usage_help(ops, interp, e,
    "The command body to execute for each iteration. Loop variables are visible in this scope.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "foreach x {a b c} {\n"
    "    puts $x\n"
    "}",
    "Iterate over a single list, printing each element:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "foreach {i j} {1 2 3 4 5 6} {\n"
    "    puts \"$i,$j\"\n"
    "}",
    "Iterate over pairs of elements:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "foreach i {a b c} j {1 2 3} {\n"
    "    puts \"$i = $j\"\n"
    "}",
    "Iterate over two lists in parallel:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "foreach x {a b c d e} {\n"
    "    if {$x eq \"c\"} { continue }\n"
    "    if {$x eq \"e\"} { break }\n"
    "    puts $x\n"
    "}",
    "Using continue to skip an iteration and break to exit early:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "foreach", spec);
}
