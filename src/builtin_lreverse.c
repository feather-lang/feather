#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_lreverse(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"lreverse list\"", 39);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listArg = ops->list.shift(interp, args);
  FeatherObj list = ops->list.from(interp, listArg);
  if (list == 0) {
    return TCL_ERROR;
  }

  size_t len = ops->list.length(interp, list);
  FeatherObj result = ops->list.create(interp);

  for (size_t i = len; i > 0; i--) {
    FeatherObj elem = ops->list.at(interp, list, i - 1);
    result = ops->list.push(interp, result, elem);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_lreverse_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Reverse the elements of a list",
    "Returns a list with the same elements as the input list, but in reverse order. "
    "The original list is not modified; a new list is returned.\n\n"
    "The command preserves the structure of nested lists as elements. For example, "
    "reversing {a b {c d} e} produces {e {c d} b a}.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to reverse");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreverse {a b c}",
    "Reverse a simple list",
    "c b a");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreverse {1 2 {3 4} 5}",
    "Reverse a list with nested elements",
    "5 {3 4} 2 1");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreverse {}",
    "Reverse an empty list",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "list(1), lappend(1), lindex(1), llength(1), lrange(1), lsort(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lreverse", spec);
}
