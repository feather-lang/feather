#include "feather.h"
#include "internal.h"

void feather_register_lappend_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Append list elements onto a variable",
    "Treats the variable given by varName as a list and appends each of the "
    "value arguments to that list as a separate element, with spaces between "
    "elements. If varName does not exist, it is created as a list with elements "
    "given by the value arguments.\n\n"
    "lappend is similar to append except that the values are appended as list "
    "elements rather than raw text. This command provides a relatively efficient "
    "way to build up large lists. For example, \"lappend a $b\" is much more "
    "efficient than \"set a [concat $a [list $b]]\" when $a is long.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<varName>");
  e = feather_usage_help(ops, interp, e, "Name of the variable to append to");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?value?...");
  e = feather_usage_help(ops, interp, e, "Values to append as list elements (zero or more)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lappend mylist a b c",
    "Create new list or append to existing:",
    "a b c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {1 2}\nlappend x 3 4",
    "Append multiple elements:",
    "1 2 3 4");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lappend result",
    "Create empty list if variable doesn't exist:",
    "");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "append(1), list(1), lindex(1), llength(1), lrange(1), lreplace(1), "
    "lsearch(1), lset(1), lsort(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lappend", spec);
}

FeatherResult feather_builtin_lappend(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lappend varName ?value ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  // Get current value or create empty list
  // feather_get_var handles qualified names and fires read traces
  FeatherObj current;
  FeatherResult res = feather_get_var(ops, interp, varName, &current);
  if (res != TCL_OK) {
    return res;  // Read trace error already set
  }

  FeatherObj list;
  if (ops->list.is_nil(interp, current)) {
    list = ops->list.create(interp);
  } else {
    list = ops->list.from(interp, current);
  }

  // Append all values
  size_t numValues = ops->list.length(interp, args);
  for (size_t i = 0; i < numValues; i++) {
    FeatherObj value = ops->list.shift(interp, args);
    list = ops->list.push(interp, list, value);
  }

  // Store back in variable
  // feather_set_var handles qualified names and fires write traces
  res = feather_set_var(ops, interp, varName, list);
  if (res != TCL_OK) {
    return res;  // Write trace error already set
  }

  ops->interp.set_result(interp, list);
  return TCL_OK;
}
