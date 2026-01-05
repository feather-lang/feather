#include "feather.h"
#include "index_parse.h"

FeatherResult feather_builtin_lrange(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lrange list first last\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj firstObj = ops->list.shift(interp, args);
  FeatherObj lastObj = ops->list.shift(interp, args);

  // Convert to list
  FeatherObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Parse indices
  int64_t first, last;
  if (feather_parse_index(ops, interp, firstObj, listLen, &first) != TCL_OK) {
    return TCL_ERROR;
  }
  if (feather_parse_index(ops, interp, lastObj, listLen, &last) != TCL_OK) {
    return TCL_ERROR;
  }

  // Clamp indices
  if (first < 0) first = 0;
  if (last >= (int64_t)listLen) last = (int64_t)listLen - 1;

  // If range is empty or invalid, return empty list
  if (first > last || listLen == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Use slice for efficient O(n) extraction where n is slice size
  FeatherObj result = ops->list.slice(interp, list, (size_t)first, (size_t)last);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_lrange_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Extract a range of elements from a list",
    "Returns a new list containing the elements from index first through index last "
    "(inclusive). List indexing is zero-based, where 0 is the first element.\n\n"
    "Indices can be integers, the keyword \"end\" (last element), \"end-N\" "
    "(N positions before the last), or arithmetic expressions like \"M+N\" or \"M-N\".\n\n"
    "If first is less than zero, it is treated as zero. If last is greater than or "
    "equal to the list length, it is treated as the index of the last element. "
    "If first is greater than last, an empty string is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to extract elements from");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<first>");
  e = feather_usage_help(ops, interp, e,
    "Index of the first element to include. Can be an integer, \"end\", \"end-N\", "
    "or an arithmetic expression. Negative values are treated as zero.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<last>");
  e = feather_usage_help(ops, interp, e,
    "Index of the last element to include. Can be an integer, \"end\", \"end-N\", "
    "or an arithmetic expression. Values beyond the list length are treated as the "
    "index of the last element.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} 0 1",
    "Extract first two elements:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} end-2 end",
    "Extract last three elements:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} 1 end-1",
    "Extract middle elements (skip first and last):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} 2 2",
    "Extract a single element as a list:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c} 5 10",
    "Indices beyond list length are clamped - returns empty string:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lrange", spec);
}
