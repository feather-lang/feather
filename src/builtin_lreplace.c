#include "feather.h"
#include "internal.h"
#include "index_parse.h"

void feather_register_lreplace_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Replace elements in a list",
    "Returns a new list formed by replacing elements from index first through "
    "last with zero or more new elements. The original list is not modified.\n\n"
    "Indices may be integers, the keyword \"end\" (last element), \"end-N\" "
    "(N positions before the last), or arithmetic expressions like \"M+N\" or \"M-N\". "
    "Indices are clamped to valid positions within the list.\n\n"
    "If no replacement elements are provided, the specified range is deleted from the list. "
    "If last is less than first, the replacement elements are inserted at position first "
    "without deleting any elements. If first is beyond the end of the list, elements "
    "are appended.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to perform replacement on");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<first>");
  e = feather_usage_help(ops, interp, e, "Index of first element to replace (integer, end, or end-N)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<last>");
  e = feather_usage_help(ops, interp, e, "Index of last element to replace (integer, end, or end-N)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?element?...");
  e = feather_usage_help(ops, interp, e, "Replacement elements (zero or more)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreplace {a b c d e} 1 1 X",
    "Replace single element at index 1:",
    "a X c d e");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreplace {a b c d e} 1 2 X Y Z",
    "Replace multiple elements with multiple replacements:",
    "a X Y Z d e");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreplace {a b c d e} 1 2",
    "Delete elements (no replacements):",
    "a d e");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreplace {a b c d e} 2 1 X",
    "Insert without deletion (last < first):",
    "a b X c d e");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreplace {a b c} end end Z",
    "Replace last element using 'end':",
    "a b Z");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lreplace {a b c d e} end-2 end X Y",
    "Replace range using end-relative indices:",
    "a b X Y");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lreplace", spec);
}

FeatherResult feather_builtin_lreplace(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lreplace list first last ?element ...?\"", 64);
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

  // Clamp indices for splice calculation
  if (first < 0) first = 0;
  if (first > (int64_t)listLen) first = (int64_t)listLen;
  if (last < first - 1) last = first - 1;
  if (last >= (int64_t)listLen) last = (int64_t)listLen - 1;

  // Calculate delete count
  size_t deleteCount = 0;
  if (last >= first) {
    deleteCount = (size_t)(last - first + 1);
  }

  // Use splice for efficient O(n) replacement
  FeatherObj result = ops->list.splice(interp, list, (size_t)first, deleteCount, args);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
