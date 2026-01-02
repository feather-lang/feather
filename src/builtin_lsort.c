#include "feather.h"
#include "internal.h"
#include "charclass.h"

// Sort mode
typedef enum {
  SORT_ASCII,
  SORT_INTEGER,
  SORT_REAL
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

  // Extract values to compare (handles -index)
  FeatherObj valA = extract_compare_value(ctx, interp, a);
  FeatherObj valB = extract_compare_value(ctx, interp, b);

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
  int unique = 0;
  int returnIndices = 0;

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
      } else {
        FeatherObj msg = ops->string.intern(interp, "bad option \"", 12);
        msg = ops->string.concat(interp, msg, arg);
        FeatherObj suffix = ops->string.intern(interp,
          "\": must be -ascii, -decreasing, -increasing, -index, -indices, -integer, -nocase, -real, or -unique", 99);
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

  // Handle empty or single-element list
  if (listLen <= 1) {
    if (returnIndices) {
      if (listLen == 0) {
        ops->interp.set_result(interp, list);
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
    // For -indices, create a list of {index, value} pairs
    FeatherObj pairs = ops->list.create(interp);
    for (size_t i = 0; i < listLen; i++) {
      FeatherObj elem = ops->list.at(interp, list, i);
      FeatherObj pair = ops->list.create(interp);
      pair = ops->list.push(interp, pair, ops->integer.create(interp, (int64_t)i));
      pair = ops->list.push(interp, pair, elem);
      pairs = ops->list.push(interp, pairs, pair);
    }

    // Sort the pairs - the context's sortingPairs flag handles extraction
    ctx.sortingPairs = 1;

    if (ops->list.sort(interp, pairs, compare_elements, &ctx) != TCL_OK) {
      FeatherObj msg = ops->string.intern(interp, "sort failed", 11);
      ops->interp.set_result(interp, msg);
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
        FeatherObj idx = ops->list.at(interp, pairList, 0);
        FeatherObj value = ops->list.at(interp, pairList, 1);

        if (i == pairsLen - 1) {
          // Last element, always add
          result = ops->list.push(interp, result, idx);
        } else {
          // Compare with next value
          FeatherObj nextPair = ops->list.at(interp, pairs, i + 1);
          FeatherObj nextPairList = ops->list.from(interp, nextPair);
          FeatherObj nextValue = ops->list.at(interp, nextPairList, 1);
          if (compare_elements(interp, value, nextValue, &ctx) != 0) {
            result = ops->list.push(interp, result, idx);
          }
        }
      }
      ops->interp.set_result(interp, result);
    } else {
      FeatherObj result = ops->list.create(interp);
      for (size_t i = 0; i < pairsLen; i++) {
        FeatherObj pair = ops->list.at(interp, pairs, i);
        FeatherObj pairList = ops->list.from(interp, pair);
        FeatherObj idx = ops->list.at(interp, pairList, 0);
        result = ops->list.push(interp, result, idx);
      }
      ops->interp.set_result(interp, result);
    }
  } else {
    // Regular sort without -indices
    // Use host's O(n log n) sort - no size limit!
    if (ops->list.sort(interp, list, compare_elements, &ctx) != TCL_OK) {
      FeatherObj msg = ops->string.intern(interp, "sort failed", 11);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Handle -unique: remove consecutive duplicates after sorting
    // TCL keeps the LAST duplicate, so we add an element when the NEXT is different
    if (unique) {
      FeatherObj result = ops->list.create(interp);
      for (size_t i = 0; i < listLen; i++) {
        FeatherObj elem = ops->list.at(interp, list, i);
        // Add if this is the last element or different from next
        if (i == listLen - 1) {
          result = ops->list.push(interp, result, elem);
        } else {
          FeatherObj next = ops->list.at(interp, list, i + 1);
          if (compare_elements(interp, elem, next, &ctx) != 0) {
            result = ops->list.push(interp, result, elem);
          }
        }
      }
      ops->interp.set_result(interp, result);
    } else {
      ops->interp.set_result(interp, list);
    }
  }

  return TCL_OK;
}
