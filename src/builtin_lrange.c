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
