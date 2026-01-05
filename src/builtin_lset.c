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

void feather_register_lset_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Modify element of a list stored in a variable",
    "Changes one or more elements of the list stored in varName and returns "
    "the modified list. Without indices, replaces the entire value.\n\n"
    "With one or more indices, replaces the element at the specified position. "
    "Indices can be specified as separate arguments or as a single list argument. "
    "Each index identifies a level of nesting in the list structure.\n\n"
    "Index values support integer positions (0-based), the \"end\" keyword for "
    "the last element, and arithmetic expressions like \"end-1\" or \"0+1\". "
    "An index equal to the list length appends a new element.\n\n"
    "Returns an error if the variable does not exist or if any index is out of range.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<varName>");
  e = feather_usage_help(ops, interp, e,
    "Name of the variable containing the list to modify");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?index?...");
  e = feather_usage_help(ops, interp, e,
    "Zero or more indices specifying which element to modify. Can be specified "
    "as separate arguments or as a single list. Each index identifies a nesting "
    "level. Supports integers, \"end\" keyword, and arithmetic like \"end-1\"");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<value>");
  e = feather_usage_help(ops, interp, e,
    "The new value to set at the specified position");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {a b c}\n"
    "lset x 1 B",
    "Replace element at index 1",
    "Result: {a B c}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {a b c}\n"
    "lset x end Z",
    "Replace last element using \"end\" keyword",
    "Result: {a b Z}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {a b c}\n"
    "lset x 3 d",
    "Append element by using index equal to list length",
    "Result: {a b c d}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {{a b} {c d}}\n"
    "lset x 0 1 B",
    "Modify nested element using multiple indices",
    "Result: {{a B} {c d}}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x {{a b} {c d}}\n"
    "lset x {1 0} C",
    "Modify nested element using list of indices",
    "Result: {{a b} {C d}}");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set x hello\n"
    "lset x WORLD",
    "Replace entire variable value (no indices)",
    "Result: WORLD");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lset", spec);
}
