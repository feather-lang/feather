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

void feather_register_lindex_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Retrieve an element from a list",
    "Returns the element at the specified index (or indices for nested lists). "
    "List indexing is zero-based, where 0 is the first element.\n\n"
    "If no index is specified, returns the list unchanged. If one or more indices "
    "are provided, each index is applied in sequence to navigate into nested lists.\n\n"
    "Indices can be integers, the keyword \"end\" (last element), \"end-N\" "
    "(N positions before the last), or arithmetic expressions like \"M+N\" or \"M-N\". "
    "Out-of-bounds indices return an empty string.\n\n"
    "When a single index argument is a list, each element of that list is treated "
    "as a separate index for nested list traversal.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to index into");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?index?...");
  e = feather_usage_help(ops, interp, e,
    "Zero or more indices. Each index is applied in sequence to navigate nested lists. "
    "Can be an integer, \"end\", \"end-N\", or an arithmetic expression. "
    "If omitted, returns the list unchanged.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lindex {a b c} 1",
    "Basic indexing - returns the second element:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lindex {a b c d e} end",
    "Use end to get the last element:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lindex {a b c d e} end-2",
    "Use end-N to count backwards from the end:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lindex {{a b} {c d} {e f}} 1 0",
    "Nested list indexing with multiple indices:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lindex {{a b} {c d} {e f}} {1 0}",
    "Equivalent using a list of indices:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lindex {a b c}",
    "No index returns the list unchanged:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lindex", spec);
}
