#include "tclc.h"
#include "internal.h"

// Sort mode
typedef enum {
  SORT_ASCII,
  SORT_INTEGER,
  SORT_REAL
} SortMode;

// Comparison context
typedef struct {
  const TclHostOps *ops;
  TclInterp interp;
  SortMode mode;
  int nocase;
  int decreasing;
} SortContext;

// Helper for case-insensitive compare
static int char_tolower(int c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  return c;
}

static int compare_nocase(const TclHostOps *ops, TclInterp interp,
                          TclObj a, TclObj b) {
  size_t lenA, lenB;
  const char *strA = ops->string.get(interp, a, &lenA);
  const char *strB = ops->string.get(interp, b, &lenB);

  size_t minLen = lenA < lenB ? lenA : lenB;
  for (size_t i = 0; i < minLen; i++) {
    int ca = char_tolower((unsigned char)strA[i]);
    int cb = char_tolower((unsigned char)strB[i]);
    if (ca != cb) return ca - cb;
  }
  if (lenA < lenB) return -1;
  if (lenA > lenB) return 1;
  return 0;
}

// Compare two elements
static int compare_elements(SortContext *ctx, TclObj a, TclObj b) {
  int result = 0;

  switch (ctx->mode) {
    case SORT_ASCII:
      if (ctx->nocase) {
        result = compare_nocase(ctx->ops, ctx->interp, a, b);
      } else {
        result = ctx->ops->string.compare(ctx->interp, a, b);
      }
      break;

    case SORT_INTEGER: {
      int64_t va, vb;
      // For integer mode, we assume the conversion will succeed
      // (error checking should happen before sort)
      ctx->ops->integer.get(ctx->interp, a, &va);
      ctx->ops->integer.get(ctx->interp, b, &vb);
      if (va < vb) result = -1;
      else if (va > vb) result = 1;
      else result = 0;
      break;
    }

    case SORT_REAL: {
      double va, vb;
      ctx->ops->dbl.get(ctx->interp, a, &va);
      ctx->ops->dbl.get(ctx->interp, b, &vb);
      if (va < vb) result = -1;
      else if (va > vb) result = 1;
      else result = 0;
      break;
    }
  }

  if (ctx->decreasing) result = -result;
  return result;
}

// Simple insertion sort for small lists (stable)
static void insertion_sort(SortContext *ctx, TclObj *arr, size_t n) {
  for (size_t i = 1; i < n; i++) {
    TclObj key = arr[i];
    size_t j = i;
    while (j > 0 && compare_elements(ctx, arr[j - 1], key) > 0) {
      arr[j] = arr[j - 1];
      j--;
    }
    arr[j] = key;
  }
}

// Helper to check string equality
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t llen = 0;
  while (lit[llen]) llen++;
  if (len != llen) return 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) return 0;
  }
  return 1;
}

TclResult tcl_builtin_lsort(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
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
  int unique = 0;

  // Process options until we find a non-option (the list)
  TclObj listObj = 0;
  while (ops->list.length(interp, args) > 0) {
    TclObj arg = ops->list.shift(interp, args);
    size_t len;
    const char *str = ops->string.get(interp, arg, &len);

    if (len > 0 && str[0] == '-') {
      if (str_eq(str, len, "-ascii")) {
        ctx.mode = SORT_ASCII;
      } else if (str_eq(str, len, "-integer")) {
        ctx.mode = SORT_INTEGER;
      } else if (str_eq(str, len, "-real")) {
        ctx.mode = SORT_REAL;
      } else if (str_eq(str, len, "-increasing")) {
        ctx.decreasing = 0;
      } else if (str_eq(str, len, "-decreasing")) {
        ctx.decreasing = 1;
      } else if (str_eq(str, len, "-nocase")) {
        ctx.nocase = 1;
      } else if (str_eq(str, len, "-unique")) {
        unique = 1;
      } else {
        TclObj msg = ops->string.intern(interp, "bad option \"", 12);
        msg = ops->string.concat(interp, msg, arg);
        TclObj suffix = ops->string.intern(interp,
          "\": must be -ascii, -decreasing, -increasing, -integer, -nocase, -real, or -unique", 81);
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
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lsort ?options? list\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Convert to list
  TclObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Handle empty or single-element list
  if (listLen <= 1) {
    ops->interp.set_result(interp, list);
    return TCL_OK;
  }

  // Copy elements to array for sorting
  // For large lists, we would need dynamic allocation, but for now
  // we'll use a fixed-size array. The host should handle this ideally.
  // For simplicity, we'll rebuild using list operations.
  #define MAX_SORT_SIZE 1024
  TclObj arr[MAX_SORT_SIZE];
  if (listLen > MAX_SORT_SIZE) {
    TclObj msg = ops->string.intern(interp, "list too large to sort", 22);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  for (size_t i = 0; i < listLen; i++) {
    arr[i] = ops->list.at(interp, list, i);
  }

  // Sort
  insertion_sort(&ctx, arr, listLen);

  // Build result, optionally removing duplicates
  TclObj result = ops->list.create(interp);
  for (size_t i = 0; i < listLen; i++) {
    if (unique && i > 0) {
      // Check if same as previous
      if (compare_elements(&ctx, arr[i], arr[i - 1]) == 0) {
        continue;
      }
    }
    result = ops->list.push(interp, result, arr[i]);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
