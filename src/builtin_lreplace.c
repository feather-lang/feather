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
  TclObj result = ops->list.splice(interp, list, (size_t)first, deleteCount, args);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
