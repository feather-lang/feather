#include "feather.h"
#include "internal.h"
#include "index_parse.h"

// Helper to set error message with index value
static void set_index_error(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj indexObj) {
  FeatherObj msg = ops->string.intern(interp, "index \"", 7);
  msg = ops->string.concat(interp, msg, indexObj);
  FeatherObj suffix = ops->string.intern(interp, "\" out of range", 14);
  msg = ops->string.concat(interp, msg, suffix);
  ops->interp.set_result(interp, msg);
}

// Recursive helper to apply lset with multiple indices
// Stores result in *out on success
// Returns TCL_OK on success, TCL_ERROR on failure
static FeatherResult lset_recursive(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj list, FeatherObj *indices, size_t numIndices,
                                    FeatherObj newValue, FeatherObj *out) {
  if (numIndices == 0) {
    // No indices left - return the new value
    *out = newValue;
    return TCL_OK;
  }

  // Convert to list if needed
  list = ops->list.from(interp, list);
  size_t listLen = ops->list.length(interp, list);

  // Parse the first index
  FeatherObj indexObj = indices[0];
  int64_t index;
  if (feather_parse_index(ops, interp, indexObj, listLen, &index) != TCL_OK) {
    return TCL_ERROR; // Error already set by parse_index
  }

  // Check bounds - negative is always an error
  if (index < 0) {
    set_index_error(ops, interp, indexObj);
    return TCL_ERROR;
  }

  if (numIndices == 1) {
    // Last index - perform the replacement or append
    if ((size_t)index > listLen) {
      set_index_error(ops, interp, indexObj);
      return TCL_ERROR;
    }
    if ((size_t)index == listLen) {
      // Append case
      list = ops->list.push(interp, list, newValue);
    } else {
      // Replace case
      if (ops->list.set_at(interp, list, (size_t)index, newValue) != TCL_OK) {
        set_index_error(ops, interp, indexObj);
        return TCL_ERROR;
      }
    }
    *out = list;
    return TCL_OK;
  }

  // More indices to process - need to recurse into sublist
  if ((size_t)index >= listLen) {
    set_index_error(ops, interp, indexObj);
    return TCL_ERROR;
  }

  // Get the sublist at this index and recurse
  FeatherObj sublist = ops->list.at(interp, list, (size_t)index);
  FeatherObj result;
  if (lset_recursive(ops, interp, sublist, indices + 1, numIndices - 1, newValue, &result) != TCL_OK) {
    return TCL_ERROR;
  }

  // Replace the element with the recursively modified sublist
  if (ops->list.set_at(interp, list, (size_t)index, result) != TCL_OK) {
    set_index_error(ops, interp, indexObj);
    return TCL_ERROR;
  }

  *out = list;
  return TCL_OK;
}

FeatherResult feather_builtin_lset(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lset listVar ?index? ?index ...? value\"", 64);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.at(interp, args, 0);
  FeatherObj newValue = ops->list.at(interp, args, argc - 1);

  // Get current value - error if variable doesn't exist
  FeatherObj current;
  feather_get_var(ops, interp, varName, &current);
  if (ops->list.is_nil(interp, current)) {
    FeatherObj msg = ops->string.intern(interp, "can't read \"", 12);
    msg = ops->string.concat(interp, msg, varName);
    FeatherObj suffix = ops->string.intern(interp, "\": no such variable", 19);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Case 1: argc == 2 - lset varName newValue (replace entire variable)
  if (argc == 2) {
    if (feather_set_var(ops, interp, varName, newValue) != TCL_OK) {
      return TCL_ERROR;
    }
    ops->interp.set_result(interp, newValue);
    return TCL_OK;
  }

  // Build array of indices
  // Case: argc == 3 - could be single index or index list
  // Case: argc >= 4 - multiple indices as separate arguments

  FeatherObj indices[64]; // Max 64 levels of nesting
  size_t numIndices = 0;

  if (argc == 3) {
    // Single index argument - could be empty, single value, or list of indices
    FeatherObj indexArg = ops->list.at(interp, args, 1);
    FeatherObj indexList = ops->list.from(interp, indexArg);
    size_t indexListLen = ops->list.length(interp, indexList);

    if (indexListLen == 0) {
      // Empty index list - replace entire variable
      if (feather_set_var(ops, interp, varName, newValue) != TCL_OK) {
        return TCL_ERROR;
      }
      ops->interp.set_result(interp, newValue);
      return TCL_OK;
    }

    // Use elements of the list as indices
    for (size_t i = 0; i < indexListLen && i < 64; i++) {
      indices[numIndices++] = ops->list.at(interp, indexList, i);
    }
  } else {
    // argc >= 4 - multiple indices as separate arguments
    for (size_t i = 1; i < argc - 1 && numIndices < 64; i++) {
      indices[numIndices++] = ops->list.at(interp, args, i);
    }
  }

  // Convert current value to list and apply lset recursively
  FeatherObj list = ops->list.from(interp, current);
  FeatherObj result;
  if (lset_recursive(ops, interp, list, indices, numIndices, newValue, &result) != TCL_OK) {
    return TCL_ERROR;
  }

  // Store back in variable
  if (feather_set_var(ops, interp, varName, result) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
