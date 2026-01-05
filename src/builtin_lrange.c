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
    "Return one or more adjacent elements from a list",
    "Returns a new list consisting of elements first through last, inclusive. "
    "The index values first and last are interpreted the same as index values for "
    "the command string index, supporting simple index arithmetic and indices "
    "relative to the end of the list.\n\n"
    "If first is less than zero, it is treated as if it were zero. If last is "
    "greater than or equal to the number of elements in the list, then it is "
    "treated as if it were end. If first is greater than last then an empty "
    "string is returned.\n\n"
    "Note that \"lrange list first first\" does not always produce the same result "
    "as \"lindex list first\" (although it often does for simple fields that are "
    "not enclosed in braces); it does, however, produce exactly the same results "
    "as \"list [lindex list first]\".");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "A valid Tcl list to extract elements from");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<first>");
  e = feather_usage_help(ops, interp, e,
    "Index of the first element to include. Can be an integer, \"end\", or an "
    "index expression like \"end-N\" or \"M+N\". Values less than zero are "
    "treated as zero.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<last>");
  e = feather_usage_help(ops, interp, e,
    "Index of the last element to include. Can be an integer, \"end\", or an "
    "index expression like \"end-N\" or \"M+N\". Values beyond the list length "
    "are treated as end.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} 0 1",
    "Selecting the first two elements:",
    "a b");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} end-2 end",
    "Selecting the last three elements:",
    "c d e");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {a b c d e} 1 end-1",
    "Selecting everything except the first and last element:",
    "b c d");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrange {some {elements to} select} 1 1",
    "Selecting a single element with lrange preserves braces (unlike lindex):",
    "{elements to}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "list(1), lappend(1), lindex(1), linsert(1), llength(1), lrepeat(1), "
    "lreplace(1), lreverse(1), lsearch(1), lset(1), lsort(1), string(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lrange", spec);
}
