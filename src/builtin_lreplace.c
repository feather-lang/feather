#include "feather.h"
#include "internal.h"
#include "index_parse.h"

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
