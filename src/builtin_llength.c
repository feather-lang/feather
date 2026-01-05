#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_llength(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"llength list\"", 38);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj list = ops->list.shift(interp, args);

  // Convert to list and get length
  FeatherObj listCopy = ops->list.from(interp, list);
  size_t len = ops->list.length(interp, listCopy);

  FeatherObj result = ops->integer.create(interp, (int64_t)len);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_llength_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Return the number of elements in a list",
    "Returns the count of elements in the list. The list argument is parsed "
    "as a TCL list, properly handling nested lists, braces, quotes, and "
    "backslash escapes.\n\n"
    "Each top-level element in the list counts as one, regardless of whether "
    "it is a nested list.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to measure");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "llength {a b c}",
    "Returns 3 - a simple list with three elements",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "llength {}",
    "Returns 0 - an empty list",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "llength {a b {c d} e}",
    "Returns 4 - the nested {c d} counts as one element",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "llength", spec);
}
