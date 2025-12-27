#include "feather.h"

FeatherResult feather_builtin_lrepeat(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"lrepeat count ?value ...?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj countObj = ops->list.shift(interp, args);
  int64_t count;
  if (ops->integer.get(interp, countObj, &count) != TCL_OK) {
    size_t valLen;
    const char *valStr = ops->string.get(interp, countObj, &valLen);
    FeatherObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
    FeatherObj part2 = ops->string.intern(interp, valStr, valLen);
    FeatherObj part3 = ops->string.intern(interp, "\"", 1);
    FeatherObj msg = ops->string.concat(interp, part1, part2);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  if (count < 0) {
    const char *countStr;
    size_t countLen;
    countStr = ops->string.get(interp, countObj, &countLen);
    
    char buf[128];
    size_t msgLen = 0;
    const char *prefix = "bad count \"";
    for (size_t i = 0; prefix[i]; i++) buf[msgLen++] = prefix[i];
    for (size_t i = 0; i < countLen && msgLen < 100; i++) buf[msgLen++] = countStr[i];
    const char *suffix = "\": must be integer >= 0";
    for (size_t i = 0; suffix[i] && msgLen < 127; i++) buf[msgLen++] = suffix[i];
    buf[msgLen] = '\0';
    
    FeatherObj msg = ops->string.intern(interp, buf, msgLen);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  size_t numElements = argc - 1;
  FeatherObj result = ops->list.create(interp);

  if (numElements == 0 || count == 0) {
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  FeatherObj elements = ops->list.from(interp, args);
  if (elements == 0) {
    return TCL_ERROR;
  }

  for (int64_t i = 0; i < count; i++) {
    for (size_t j = 0; j < numElements; j++) {
      FeatherObj elem = ops->list.at(interp, elements, j);
      result = ops->list.push(interp, result, elem);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}
