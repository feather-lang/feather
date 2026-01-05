#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_join(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"join list ?joinString?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Default separator is space
  FeatherObj sepObj = ops->string.intern(interp, " ", 1);
  if (argc == 2) {
    sepObj = ops->list.shift(interp, args);
  }

  // Handle empty list
  if (listLen == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Handle single element
  if (listLen == 1) {
    ops->interp.set_result(interp, ops->list.at(interp, list, 0));
    return TCL_OK;
  }

  // Build result by concatenating elements with separator
  FeatherObj result = ops->list.at(interp, list, 0);

  for (size_t i = 1; i < listLen; i++) {
    result = ops->string.concat(interp, result, sepObj);
    FeatherObj elem = ops->list.at(interp, list, i);
    result = ops->string.concat(interp, result, elem);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_join_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Create a string by joining list elements with a separator",
    "Returns a string created by joining all elements of list together "
    "with joinString separating each adjacent pair of elements.\n\n"
    "If joinString is not specified, it defaults to a single space character.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Arguments
  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list whose elements will be joined");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?joinString?");
  e = feather_usage_help(ops, interp, e, "The separator string to place between elements (default: single space)");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "join {a b c}",
    "Join elements with default space separator:",
    "a b c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "join {a b c} {, }",
    "Join elements with custom separator:",
    "a, b, c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "join {one} {-}",
    "Single element list returns the element unchanged:",
    "one");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "join {}",
    "Empty list returns empty string:",
    "");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "join {1 {2 3} 4 {5 {6 7} 8}}",
    "Flatten a list by a single level (nested braces are preserved):",
    "1 2 3 4 5 {6 7} 8");
  spec = feather_usage_add(ops, interp, spec, e);

  // See Also section
  e = feather_usage_section(ops, interp, "See Also",
    "list(1), lappend(1), split(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "join", spec);
}
