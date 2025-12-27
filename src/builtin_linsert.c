#include "feather.h"
#include "internal.h"
#include "index_parse.h"

FeatherResult feather_builtin_linsert(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"linsert list index ?element ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj indexObj = ops->list.shift(interp, args);

  FeatherObj list = ops->list.from(interp, listObj);
  if (list == 0) {
    return TCL_ERROR;
  }
  size_t listLen = ops->list.length(interp, list);

  size_t idxLen;
  const char *idxStr = ops->string.get(interp, indexObj, &idxLen);
  int is_end_relative = (idxLen >= 3 && idxStr[0] == 'e' && idxStr[1] == 'n' && idxStr[2] == 'd');

  int64_t index;
  if (feather_parse_index(ops, interp, indexObj, listLen, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  if (is_end_relative) {
    index += 1;
  }

  if (index < 0) index = 0;
  if (index > (int64_t)listLen) index = (int64_t)listLen;

  FeatherObj result = ops->list.splice(interp, list, (size_t)index, 0, args);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
