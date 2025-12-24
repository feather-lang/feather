#include "tclc.h"
#include "internal.h"

// Parse an index like "end", "end-N", or integer
static TclResult parse_index(const TclHostOps *ops, TclInterp interp,
                             TclObj indexObj, size_t listLen, int64_t *out) {
  size_t len;
  const char *str = ops->string.get(interp, indexObj, &len);

  // Check for "end"
  if (len == 3 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd') {
    *out = (int64_t)listLen - 1;
    return TCL_OK;
  }

  // Check for "end-N"
  if (len > 4 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd' && str[3] == '-') {
    int64_t offset = 0;
    for (size_t i = 4; i < len; i++) {
      if (str[i] < '0' || str[i] > '9') {
        TclObj msg = ops->string.intern(interp, "bad index \"", 11);
        msg = ops->string.concat(interp, msg, indexObj);
        TclObj suffix = ops->string.intern(interp, "\"", 1);
        msg = ops->string.concat(interp, msg, suffix);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      offset = offset * 10 + (str[i] - '0');
    }
    *out = (int64_t)listLen - 1 - offset;
    return TCL_OK;
  }

  // Try integer
  if (ops->integer.get(interp, indexObj, out) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "bad index \"", 11);
    msg = ops->string.concat(interp, msg, indexObj);
    TclObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  return TCL_OK;
}

TclResult tcl_builtin_lreplace(const TclHostOps *ops, TclInterp interp,
                                TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"lreplace list first last ?element ...?\"", 64);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj listObj = ops->list.shift(interp, args);
  TclObj firstObj = ops->list.shift(interp, args);
  TclObj lastObj = ops->list.shift(interp, args);

  // Convert to list
  TclObj list = ops->list.from(interp, listObj);
  size_t listLen = ops->list.length(interp, list);

  // Parse indices
  int64_t first, last;
  if (parse_index(ops, interp, firstObj, listLen, &first) != TCL_OK) {
    return TCL_ERROR;
  }
  if (parse_index(ops, interp, lastObj, listLen, &last) != TCL_OK) {
    return TCL_ERROR;
  }

  // Build result:
  // 1. Elements before first
  // 2. Replacement elements
  // 3. Elements after last
  TclObj result = ops->list.create(interp);

  // Add elements before first (if first > 0)
  int64_t insertBefore = first;
  if (insertBefore < 0) insertBefore = 0;
  for (int64_t i = 0; i < insertBefore && i < (int64_t)listLen; i++) {
    TclObj elem = ops->list.at(interp, list, (size_t)i);
    result = ops->list.push(interp, result, elem);
  }

  // Add replacement elements
  size_t numReplacements = ops->list.length(interp, args);
  for (size_t i = 0; i < numReplacements; i++) {
    TclObj elem = ops->list.shift(interp, args);
    result = ops->list.push(interp, result, elem);
  }

  // Add elements after last
  int64_t startAfter = last + 1;
  if (startAfter < 0) startAfter = 0;
  for (int64_t i = startAfter; i < (int64_t)listLen; i++) {
    TclObj elem = ops->list.at(interp, list, (size_t)i);
    result = ops->list.push(interp, result, elem);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
