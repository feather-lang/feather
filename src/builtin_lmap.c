#include "feather.h"
#include "internal.h"

// Callback for lmap: appends body result to accumulator list
static void lmap_callback(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj bodyResult, void *ctx) {
  FeatherObj *result = (FeatherObj *)ctx;
  *result = ops->list.push(interp, *result, bodyResult);
}

FeatherResult feather_builtin_lmap(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  FeatherObj result = ops->list.create(interp);
  FeatherResult rc = feather_foreach_impl(ops, interp, args, "lmap", lmap_callback, &result);

  if (rc == TCL_OK) {
    // lmap returns the accumulated list
    ops->interp.set_result(interp, result);
  }

  return rc;
}

void feather_register_lmap_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Map a script over one or more lists",
    "The lmap command iterates over one or more lists, executing a body script "
    "for each iteration and collecting the results into a list. It is similar to "
    "foreach, but returns a list of the results from each iteration instead of an "
    "empty string.\n\n"
    "Each varList is a list of one or more variable names. On each iteration, "
    "consecutive elements from the corresponding list are assigned to these variables. "
    "If a list is exhausted, remaining variables receive empty strings.\n\n"
    "The total number of iterations is large enough to use up all values from all "
    "value lists. The body script is executed once per iteration, and its result is "
    "appended to the accumulator list (unless break or continue is invoked).\n\n"
    "The break command exits the loop immediately and returns the accumulated results "
    "so far. The continue command skips appending the current iteration's result and "
    "proceeds to the next iteration.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: varList
  e = feather_usage_arg(ops, interp, "<varList>");
  e = feather_usage_help(ops, interp, e, "List of one or more variable names to assign on each iteration");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: list
  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "List to iterate over");
  spec = feather_usage_add(ops, interp, spec, e);

  // Optional additional varList/list pairs
  e = feather_usage_arg(ops, interp, "?varList list ...?");
  e = feather_usage_help(ops, interp, e, "Additional variable lists and lists for parallel iteration");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: body
  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to execute on each iteration");
  e = feather_usage_type(ops, interp, e, "script");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "lmap x {1 2 3} {expr {$x * 2}}",
    "Double each element:",
    "2 4 6");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lmap {a b} {1 2 3 4} {expr {$a + $b}}",
    "Sum pairs of elements:",
    "3 7");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lmap x {a b c} y {1 2 3} {list $x $y}",
    "Parallel iteration over two lists:",
    "{a 1} {b 2} {c 3}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lmap x {1 2 3 4 5} {\n"
    "    if {$x % 2 == 0} {\n"
    "        set x\n"
    "    } else {\n"
    "        continue\n"
    "    }\n"
    "}",
    "Filter to only even numbers:",
    "2 4");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "foreach, for, while, break, continue, list");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lmap", spec);
}
