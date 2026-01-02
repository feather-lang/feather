#include "feather.h"
#include "internal.h"
#include "charclass.h"

// Sort mode
typedef enum {
  SORT_ASCII,
  SORT_INTEGER,
  SORT_REAL,
  SORT_DICTIONARY
} SortMode;

// Comparison context
typedef struct {
  const FeatherHostOps *ops;
  FeatherInterp interp;
  SortMode mode;
  int nocase;
  int decreasing;
  int hasIndex;
  int64_t sortIndex;
  int sortingPairs; // True when sorting {idx, value} pairs for -indices
  int hasCommand;   // True when using -command
  FeatherObj commandProc; // The command to use for comparison
  int error;        // Set to 1 if command comparison fails
} SortContext;

static int lsort_compare_nocase(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj a, FeatherObj b) {
  size_t lenA = ops->string.byte_length(interp, a);
  size_t lenB = ops->string.byte_length(interp, b);

  size_t minLen = lenA < lenB ? lenA : lenB;
  for (size_t i = 0; i < minLen; i++) {
    int ca = feather_char_tolower((unsigned char)ops->string.byte_at(interp, a, i));
    int cb = feather_char_tolower((unsigned char)ops->string.byte_at(interp, b, i));
    if (ca != cb) return ca - cb;
  }
  if (lenA < lenB) return -1;
  if (lenA > lenB) return 1;
  return 0;
}

// Dictionary comparison: numbers in strings compared numerically, case-insensitive with case as tiebreaker
static int lsort_compare_dictionary(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj a, FeatherObj b) {
  size_t lenA = ops->string.byte_length(interp, a);
  size_t lenB = ops->string.byte_length(interp, b);
  size_t iA = 0, iB = 0;
  int caseDiff = 0; // Track first case difference as tiebreaker

  while (iA < lenA && iB < lenB) {
    unsigned char cA = (unsigned char)ops->string.byte_at(interp, a, iA);
    unsigned char cB = (unsigned char)ops->string.byte_at(interp, b, iB);

    // Both are digits - compare as numbers
    if (feather_is_digit(cA) && feather_is_digit(cB)) {
      // Count leading zeros
      size_t zerosA = 0, zerosB = 0;
      while (iA < lenA && ops->string.byte_at(interp, a, iA) == '0') {
        zerosA++;
        iA++;
      }
      while (iB < lenB && ops->string.byte_at(interp, b, iB) == '0') {
        zerosB++;
        iB++;
      }

      // Extract numeric values
      int64_t numA = 0, numB = 0;
      size_t digitsA = 0, digitsB = 0;
      while (iA < lenA && feather_is_digit((unsigned char)ops->string.byte_at(interp, a, iA))) {
        numA = numA * 10 + (ops->string.byte_at(interp, a, iA) - '0');
        digitsA++;
        iA++;
      }
      while (iB < lenB && feather_is_digit((unsigned char)ops->string.byte_at(interp, b, iB))) {
        numB = numB * 10 + (ops->string.byte_at(interp, b, iB) - '0');
        digitsB++;
        iB++;
      }

      // Compare numeric values
      if (numA != numB) {
        return (numA < numB) ? -1 : 1;
      }

      // Same numeric value - more leading zeros comes later (a1 < a01 < a001)
      if (zerosA != zerosB) {
        return (zerosA < zerosB) ? -1 : 1;
      }
      // Continue to next part of string
    } else {
      // Non-digit comparison: case-insensitive, but track case difference
      int lowerA = feather_char_tolower(cA);
      int lowerB = feather_char_tolower(cB);

      if (lowerA != lowerB) {
        return lowerA - lowerB;
      }

      // Same letter case-insensitively - track case difference for tiebreaker
      // Uppercase comes before lowercase in TCL dictionary sort
      if (caseDiff == 0 && cA != cB) {
        caseDiff = (int)cA - (int)cB;
      }

      iA++;
      iB++;
    }
  }

  // One string is prefix of the other
  if (iA < lenA) return 1;  // A is longer
  if (iB < lenB) return -1; // B is longer

  // Strings are equal ignoring case - use case difference as tiebreaker
  return caseDiff;
}

// Extract element for comparison - handles -index option and -indices pairs
static FeatherObj extract_compare_value(SortContext *ctx, FeatherInterp interp, FeatherObj elem) {
  FeatherObj value = elem;

  // If sorting pairs (for -indices), first extract the value from {idx, value}
  if (ctx->sortingPairs) {
    FeatherObj pairList = ctx->ops->list.from(interp, elem);
    size_t pairLen = ctx->ops->list.length(interp, pairList);
    if (pairLen >= 2) {
      value = ctx->ops->list.at(interp, pairList, 1);
    }
  }

  // If -index was specified, extract the element at sortIndex from the value
  if (ctx->hasIndex) {
    FeatherObj sublist = ctx->ops->list.from(interp, value);
    size_t sublistLen = ctx->ops->list.length(interp, sublist);
    if (ctx->sortIndex >= 0 && (size_t)ctx->sortIndex < sublistLen) {
      return ctx->ops->list.at(interp, sublist, (size_t)ctx->sortIndex);
    }
  }

  return value;
}

// Compare two elements - signature matches the host sort callback
static int compare_elements(FeatherInterp interp, FeatherObj a, FeatherObj b, void *ctx_ptr) {
  SortContext *ctx = (SortContext *)ctx_ptr;
  int result = 0;

  // If an error already occurred, don't continue processing
  if (ctx->error) return 0;

  // Extract values to compare (handles -index)
  FeatherObj valA = extract_compare_value(ctx, interp, a);
  FeatherObj valB = extract_compare_value(ctx, interp, b);

  if (ctx->hasCommand) {
    // Build command: {proc a b}
    FeatherObj cmdList = ctx->ops->list.create(interp);
    cmdList = ctx->ops->list.push(interp, cmdList, ctx->commandProc);
    cmdList = ctx->ops->list.push(interp, cmdList, valA);
    cmdList = ctx->ops->list.push(interp, cmdList, valB);

    // Execute the command
    FeatherResult rc = feather_command_exec(ctx->ops, interp, cmdList, TCL_EVAL_LOCAL);
    if (rc != TCL_OK) {
      // Command failed - set error flag and return
      ctx->error = 1;
      return 0;
    }

    // Get the result and parse as integer
    FeatherObj cmdResult = ctx->ops->interp.get_result(interp);
    int64_t cmpResult;
    if (ctx->ops->integer.get(interp, cmdResult, &cmpResult) != TCL_OK) {
      // Result is not an integer
      ctx->error = 1;
      FeatherObj msg = ctx->ops->string.intern(interp,
        "-compare command returned non-integer result", 44);
      ctx->ops->interp.set_result(interp, msg);
      return 0;
    }

    result = (cmpResult < 0) ? -1 : (cmpResult > 0) ? 1 : 0;
  } else {
    switch (ctx->mode) {
      case SORT_ASCII:
        if (ctx->nocase) {
          result = lsort_compare_nocase(ctx->ops, interp, valA, valB);
        } else {
          result = ctx->ops->string.compare(interp, valA, valB);
        }
        break;

      case SORT_INTEGER: {
        int64_t va, vb;
        // For integer mode, we assume the conversion will succeed
        // (error checking should happen before sort)
        ctx->ops->integer.get(interp, valA, &va);
        ctx->ops->integer.get(interp, valB, &vb);
        if (va < vb) result = -1;
        else if (va > vb) result = 1;
        else result = 0;
        break;
      }

      case SORT_REAL: {
        double va, vb;
        ctx->ops->dbl.get(interp, valA, &va);
        ctx->ops->dbl.get(interp, valB, &vb);
        if (va < vb) result = -1;
        else if (va > vb) result = 1;
        else result = 0;
        break;
      }

      case SORT_DICTIONARY:
        result = lsort_compare_dictionary(ctx->ops, interp, valA, valB);
        break;
    }
  }

  if (ctx->decreasing) result = -result;
  return result;
}

FeatherResult feather_builtin_lsort(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lsort ?options? list\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Parse options
  SortContext ctx;
  ctx.ops = ops;
  ctx.interp = interp;
  ctx.mode = SORT_ASCII;
  ctx.nocase = 0;
  ctx.decreasing = 0;
  ctx.hasIndex = 0;
  ctx.sortIndex = 0;
  ctx.sortingPairs = 0;
  ctx.hasCommand = 0;
  ctx.commandProc = 0;
  ctx.error = 0;
  int unique = 0;
  int returnIndices = 0;
  int64_t strideLength = 1; // Default is 1 (no stride)

  // Process options until we find a non-option (the list)
  FeatherObj listObj = 0;
  while (ops->list.length(interp, args) > 0) {
    FeatherObj arg = ops->list.shift(interp, args);

    // Check if this is an option (starts with '-')
    if (ops->string.byte_at(interp, arg, 0) == '-') {
      if (feather_obj_eq_literal(ops, interp, arg, "-ascii")) {
        ctx.mode = SORT_ASCII;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-integer")) {
        ctx.mode = SORT_INTEGER;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-real")) {
        ctx.mode = SORT_REAL;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-dictionary")) {
        ctx.mode = SORT_DICTIONARY;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-increasing")) {
        ctx.decreasing = 0;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-decreasing")) {
        ctx.decreasing = 1;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-nocase")) {
        ctx.nocase = 1;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-unique")) {
        unique = 1;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-indices")) {
        returnIndices = 1;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-index")) {
        // -index requires an argument (index value) plus the list
        // If only 1 arg remains, that's the list, so -index is missing its argument
        if (ops->list.length(interp, args) <= 1) {
          FeatherObj msg = ops->string.intern(interp,
            "\"-index\" option must be followed by list index", 46);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        FeatherObj indexArg = ops->list.shift(interp, args);
        if (ops->integer.get(interp, indexArg, &ctx.sortIndex) != TCL_OK) {
          FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
          msg = ops->string.concat(interp, msg, indexArg);
          FeatherObj suffix = ops->string.intern(interp,
            "\": must be integer?[+-]integer? or end?[+-]integer?", 51);
          msg = ops->string.concat(interp, msg, suffix);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        ctx.hasIndex = 1;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-command")) {
        // -command requires an argument (command name) plus the list
        if (ops->list.length(interp, args) <= 1) {
          FeatherObj msg = ops->string.intern(interp,
            "\"-command\" option must be followed by comparison command", 56);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        ctx.commandProc = ops->list.shift(interp, args);
        ctx.hasCommand = 1;
      } else if (feather_obj_eq_literal(ops, interp, arg, "-stride")) {
        // -stride requires an argument (stride length) plus the list
        if (ops->list.length(interp, args) <= 1) {
          FeatherObj msg = ops->string.intern(interp,
            "\"-stride\" option must be followed by stride length", 50);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        FeatherObj strideArg = ops->list.shift(interp, args);
        if (ops->integer.get(interp, strideArg, &strideLength) != TCL_OK) {
          FeatherObj msg = ops->string.intern(interp, "bad stride length \"", 19);
          msg = ops->string.concat(interp, msg, strideArg);
          FeatherObj suffix = ops->string.intern(interp, "\"", 1);
          msg = ops->string.concat(interp, msg, suffix);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        if (strideLength < 2) {
          FeatherObj msg = ops->string.intern(interp,
            "stride length must be at least 2", 32);
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
      } else {
        FeatherObj msg = ops->string.intern(interp, "bad option \"", 12);
        msg = ops->string.concat(interp, msg, arg);
        FeatherObj suffix = ops->string.intern(interp,
          "\": must be -ascii, -command, -decreasing, -dictionary, -increasing, -index, -indices, -integer, -nocase, -real, -stride, or -unique", 131);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
    } else {
      // This must be the list
      listObj = arg;
      break;
    }
  }

  if (ops->list.is_nil(interp, listObj)) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lsort ?options? list\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Convert to list
  FeatherObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Validate stride constraint
  if (strideLength > 1 && listLen % (size_t)strideLength != 0) {
    FeatherObj msg = ops->string.intern(interp,
      "list size must be a multiple of the stride length", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Handle stride: group elements into sublists of stride size
  size_t stride = (size_t)strideLength;
  FeatherObj workList = list;
  size_t numGroups = listLen;
  if (stride > 1) {
    workList = ops->list.create(interp);
    numGroups = listLen / stride;
    for (size_t i = 0; i < listLen; i += stride) {
      FeatherObj group = ops->list.create(interp);
      for (size_t j = 0; j < stride; j++) {
        group = ops->list.push(interp, group, ops->list.at(interp, list, i + j));
      }
      workList = ops->list.push(interp, workList, group);
    }
    // When using stride, -index now refers to position within the group
    // If no -index was specified, default to first element (index 0)
    if (!ctx.hasIndex) {
      ctx.hasIndex = 1;
      ctx.sortIndex = 0;
    }
  }

  // Handle empty or single-element list (or single group with stride)
  if (numGroups <= 1) {
    if (returnIndices) {
      if (listLen == 0) {
        ops->interp.set_result(interp, list);
      } else if (stride > 1) {
        // Return all indices in the single group
        FeatherObj result = ops->list.create(interp);
        for (size_t i = 0; i < stride; i++) {
          result = ops->list.push(interp, result, ops->integer.create(interp, (int64_t)i));
        }
        ops->interp.set_result(interp, result);
      } else {
        FeatherObj result = ops->list.create(interp);
        result = ops->list.push(interp, result, ops->integer.create(interp, 0));
        ops->interp.set_result(interp, result);
      }
    } else {
      ops->interp.set_result(interp, list);
    }
    return TCL_OK;
  }

  if (returnIndices) {
    // For -indices, create a list of {start-index, value/group} pairs
    FeatherObj pairs = ops->list.create(interp);
    if (stride > 1) {
      // Create pairs of {start-index, group} for each stride group
      for (size_t i = 0; i < listLen; i += stride) {
        FeatherObj group = ops->list.create(interp);
        for (size_t j = 0; j < stride; j++) {
          group = ops->list.push(interp, group, ops->list.at(interp, list, i + j));
        }
        FeatherObj pair = ops->list.create(interp);
        pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)i));
        pair = ops->list.push(interp, pair, group);
        pairs = ops->list.push(interp, pairs, pair);
      }
    } else {
      for (size_t i = 0; i < listLen; i++) {
        FeatherObj elem = ops->list.at(interp, list, i);
        FeatherObj pair = ops->list.create(interp);
        pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)i));
        pair = ops->list.push(interp, pair, elem);
        pairs = ops->list.push(interp, pairs, pair);
      }
    }

    // Sort the pairs - the context's sortingPairs flag handles extraction
    ctx.sortingPairs = 1;

    if (ops->list.sort(interp, pairs, compare_elements, &ctx) != TCL_OK) {
      FeatherObj msg = ops->string.intern(interp, "sort failed", 11);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Check if -command callback failed
    if (ctx.error) {
      return TCL_ERROR;
    }

    // Extract indices from sorted pairs
    // Reset sortingPairs for value comparisons in unique check
    ctx.sortingPairs = 0;
    size_t pairsLen = ops->list.length(interp, pairs);
    if (unique) {
      // TCL keeps the LAST duplicate, so we add when NEXT is different
      FeatherObj result = ops->list.create(interp);
      for (size_t i = 0; i < pairsLen; i++) {
        FeatherObj pair = ops->list.at(interp, pairs, i);
        FeatherObj pairList = ops->list.from(interp, pair);
        FeatherObj value = ops->list.at(interp, pairList, 1);
        int64_t startIdx;
        ops->integer.get(interp, ops->list.at(interp, pairList, 0), &startIdx);

        if (i == pairsLen - 1) {
          // Last element, always add all indices in the group
          for (size_t j = 0; j < stride; j++) {
            result = ops->list.push(interp, result, ops->integer.create(interp, startIdx + (int64_t)j));
          }
        } else {
          // Compare with next value
          FeatherObj nextPair = ops->list.at(interp, pairs, i + 1);
          FeatherObj nextPairList = ops->list.from(interp, nextPair);
          FeatherObj nextValue = ops->list.at(interp, nextPairList, 1);
          if (compare_elements(interp, value, nextValue, &ctx) != 0) {
            for (size_t j = 0; j < stride; j++) {
              result = ops->list.push(interp, result, ops->integer.create(interp, startIdx + (int64_t)j));
            }
          }
        }
      }
      ops->interp.set_result(interp, result);
    } else {
      FeatherObj result = ops->list.create(interp);
      for (size_t i = 0; i < pairsLen; i++) {
        FeatherObj pair = ops->list.at(interp, pairs, i);
        FeatherObj pairList = ops->list.from(interp, pair);
        int64_t startIdx;
        ops->integer.get(interp, ops->list.at(interp, pairList, 0), &startIdx);
        // Add all indices in the stride group
        for (size_t j = 0; j < stride; j++) {
          result = ops->list.push(interp, result, ops->integer.create(interp, startIdx + (int64_t)j));
        }
      }
      ops->interp.set_result(interp, result);
    }
  } else {
    // Regular sort without -indices
    // Use host's O(n log n) sort - no size limit!
    if (ops->list.sort(interp, workList, compare_elements, &ctx) != TCL_OK) {
      FeatherObj msg = ops->string.intern(interp, "sort failed", 11);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Check if -command callback failed
    if (ctx.error) {
      return TCL_ERROR;
    }

    // Handle -unique: remove consecutive duplicates after sorting
    // TCL keeps the LAST duplicate, so we add an element when the NEXT is different
    if (unique) {
      FeatherObj result = ops->list.create(interp);
      size_t workLen = ops->list.length(interp, workList);
      for (size_t i = 0; i < workLen; i++) {
        FeatherObj elem = ops->list.at(interp, workList, i);
        // Add if this is the last element or different from next
        if (i == workLen - 1) {
          // Flatten if using stride
          if (stride > 1) {
            FeatherObj group = ops->list.from(interp, elem);
            size_t groupLen = ops->list.length(interp, group);
            for (size_t j = 0; j < groupLen; j++) {
              result = ops->list.push(interp, result, ops->list.at(interp, group, j));
            }
          } else {
            result = ops->list.push(interp, result, elem);
          }
        } else {
          FeatherObj next = ops->list.at(interp, workList, i + 1);
          if (compare_elements(interp, elem, next, &ctx) != 0) {
            // Flatten if using stride
            if (stride > 1) {
              FeatherObj group = ops->list.from(interp, elem);
              size_t groupLen = ops->list.length(interp, group);
              for (size_t j = 0; j < groupLen; j++) {
                result = ops->list.push(interp, result, ops->list.at(interp, group, j));
              }
            } else {
              result = ops->list.push(interp, result, elem);
            }
          }
        }
      }
      ops->interp.set_result(interp, result);
    } else if (stride > 1) {
      // Flatten the sorted groups back into a single list
      FeatherObj result = ops->list.create(interp);
      size_t workLen = ops->list.length(interp, workList);
      for (size_t i = 0; i < workLen; i++) {
        FeatherObj group = ops->list.at(interp, workList, i);
        FeatherObj groupList = ops->list.from(interp, group);
        size_t groupLen = ops->list.length(interp, groupList);
        for (size_t j = 0; j < groupLen; j++) {
          result = ops->list.push(interp, result, ops->list.at(interp, groupList, j));
        }
      }
      ops->interp.set_result(interp, result);
    } else {
      ops->interp.set_result(interp, workList);
    }
  }

  return TCL_OK;
}
