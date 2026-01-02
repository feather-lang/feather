#include "feather.h"
#include "index_parse.h"

FeatherResult feather_builtin_lindex(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"lindex list ?index ...?\"", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj value = ops->list.shift(interp, args);
  argc--;

  // No indices: return list as-is (identity behavior)
  if (argc == 0) {
    ops->interp.set_result(interp, value);
    return TCL_OK;
  }

  // Collect indices to apply
  // If argc == 1 and the arg is a list, use that list as the indices
  // Otherwise, use each argument as an index
  FeatherObj indices[64];
  size_t numIndices = 0;

  if (argc == 1) {
    FeatherObj indexArg = ops->list.shift(interp, args);
    FeatherObj indexList = ops->list.from(interp, indexArg);
    size_t indexListLen = ops->list.length(interp, indexList);

    if (indexListLen == 0) {
      // Empty index list: return value as-is
      ops->interp.set_result(interp, value);
      return TCL_OK;
    } else if (indexListLen == 1) {
      // Single element - could be "end" or a number, use it directly
      indices[0] = indexArg;
      numIndices = 1;
    } else {
      // Multiple elements in list - treat each as an index
      for (size_t i = 0; i < indexListLen && i < 64; i++) {
        indices[i] = ops->list.at(interp, indexList, i);
      }
      numIndices = indexListLen < 64 ? indexListLen : 64;
    }
  } else {
    // Multiple arguments - each is an index
    // We already shifted once, put the first one back conceptually
    // Actually, we need to re-read args - let me reconsider

    // args still has argc-1 elements (we shifted one for indexArg but didn't)
    // Actually we haven't shifted yet in this branch
    for (size_t i = 0; i < argc && i < 64; i++) {
      indices[i] = ops->list.shift(interp, args);
    }
    numIndices = argc < 64 ? argc : 64;
  }

  // Apply each index in sequence
  for (size_t i = 0; i < numIndices; i++) {
    // Convert current value to list
    FeatherObj listCopy = ops->list.from(interp, value);
    size_t len = ops->list.length(interp, listCopy);

    // Parse index with end-N support
    int64_t index;
    if (feather_parse_index(ops, interp, indices[i], len, &index) != TCL_OK) {
      return TCL_ERROR;
    }

    // Out of bounds returns empty string
    if (index < 0 || (size_t)index >= len) {
      ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      return TCL_OK;
    }

    // Get element at index
    value = ops->list.at(interp, listCopy, (size_t)index);
    if (ops->list.is_nil(interp, value)) {
      ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      return TCL_OK;
    }
  }

  ops->interp.set_result(interp, value);
  return TCL_OK;
}
