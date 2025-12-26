#include "level_parse.h"

FeatherResult feather_parse_level(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj levelObj, size_t currentLevel,
                                  size_t stackSize, size_t *absLevel) {
  size_t len;
  const char *str = ops->string.get(interp, levelObj, &len);

  if (len > 0 && str[0] == '#') {
    int64_t absVal = 0;
    for (size_t i = 1; i < len; i++) {
      if (str[i] < '0' || str[i] > '9') {
        goto bad_level;
      }
      absVal = absVal * 10 + (str[i] - '0');
    }
    if (absVal < 0 || (size_t)absVal >= stackSize) {
      goto bad_level;
    }
    *absLevel = (size_t)absVal;
    return TCL_OK;
  } else {
    int64_t relVal;
    if (ops->integer.get(interp, levelObj, &relVal) != TCL_OK) {
      goto bad_level;
    }
    if (relVal < 0) {
      goto bad_level;
    }
    if ((size_t)relVal > currentLevel) {
      goto bad_level;
    }
    *absLevel = currentLevel - (size_t)relVal;
    return TCL_OK;
  }

bad_level:
  {
    FeatherObj msg1 = ops->string.intern(interp, "bad level \"", 11);
    FeatherObj msg2 = ops->string.intern(interp, str, len);
    FeatherObj msg3 = ops->string.intern(interp, "\"", 1);
    FeatherObj msg = ops->string.concat(interp, msg1, msg2);
    msg = ops->string.concat(interp, msg, msg3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
