#include "feather.h"
#include "index_parse.h"

FeatherResult feather_builtin_lindex(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"lindex list index\"", 43);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj list = ops->list.shift(interp, args);
  FeatherObj indexObj = ops->list.shift(interp, args);

  // Convert to list first to get length
  FeatherObj listCopy = ops->list.from(interp, list);
  size_t len = ops->list.length(interp, listCopy);

  // Parse index with end-N support
  int64_t index;
  if (feather_parse_index(ops, interp, indexObj, len, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  // Out of bounds returns empty string
  if (index < 0 || (size_t)index >= len) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Get element at index
  FeatherObj elem = ops->list.at(interp, listCopy, (size_t)index);
  if (ops->list.is_nil(interp, elem)) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  ops->interp.set_result(interp, elem);
  return TCL_OK;
}
