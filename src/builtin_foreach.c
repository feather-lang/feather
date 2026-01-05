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
    "Iterate over all elements in one or more lists",
    "The foreach command implements a loop where the loop variable(s) take on values "
    "from one or more lists. In the simplest case there is one loop variable, varname, "
    "and one list, that is a list of values to assign to varname. The body argument is "
    "a Tcl script. For each element of list (in order from first to last), foreach "
    "assigns the contents of the element to varname as if the lindex command had been "
    "used to extract the element, then calls the Tcl interpreter to execute body.\n\n"
    "In the general case there can be more than one value list (e.g., list1 and list2), "
    "and each value list can be associated with a list of loop variables (e.g., varlist1 "
    "and varlist2). During each iteration of the loop the variables of each varlist are "
    "assigned consecutive values from the corresponding list. Values in each list are "
    "used in order from first to last, and each value is used exactly once. The total "
    "number of loop iterations is large enough to use up all the values from all the "
    "value lists. If a value list does not contain enough elements for each of its loop "
    "variables in each iteration, empty values are used for the missing elements.\n\n"
    "The break and continue statements may be invoked inside body, with the same effect "
    "as in the for command. Foreach returns an empty string.");
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

  e = feather_usage_arg(ops, interp, "?varList list?...");
  e = feather_usage_help(ops, interp, e,
    "Additional pairs of variable list and value list for parallel iteration.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e,
    "The Tcl script to execute for each iteration. Loop variables are visible in this scope.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set values {1 3 5 7 2 4 6 8}\n"
    "puts \"Value\\tSquare\\tCube\"\n"
    "foreach x $values {\n"
    "    puts \" $x\\t [expr {$x**2}]\\t [expr {$x**3}]\"\n"
    "}",
    "Print each value in a list with its square and cube:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {}\n"
    "foreach {i j} {a b c d e f} {\n"
    "    lappend x $j $i\n"
    "}\n"
    "# The value of x is \"b a d c f e\"\n"
    "# There are 3 iterations of the loop.",
    "Use i and j to iterate over pairs of elements of a single list:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {}\n"
    "foreach i {a b c} j {d e f g} {\n"
    "    lappend x $i $j\n"
    "}\n"
    "# The value of x is \"a d b e c f {} g\"\n"
    "# There are 4 iterations of the loop.",
    "Use i and j to iterate over two lists in parallel:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {}\n"
    "foreach i {a b c} {j k} {d e f g} {\n"
    "    lappend x $i $j $k\n"
    "}\n"
    "# The value of x is \"a d e b f g c {} {}\"\n"
    "# There are 3 iterations of the loop.",
    "Combine both forms:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "for, while, break, continue");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "foreach", spec);
}
