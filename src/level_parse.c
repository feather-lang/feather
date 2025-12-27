#include "level_parse.h"

FeatherResult feather_parse_level(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj levelObj, size_t currentLevel,
                                  size_t stackSize, size_t *absLevel) {
  size_t len = ops->string.byte_length(interp, levelObj);
  int firstByte = ops->string.byte_at(interp, levelObj, 0);

  if (len > 0 && firstByte == '#') {
    // Absolute level: #N
    int64_t absVal = 0;
    for (size_t i = 1; i < len; i++) {
      int ch = ops->string.byte_at(interp, levelObj, i);
      if (ch < '0' || ch > '9') {
        goto bad_level;
      }
      absVal = absVal * 10 + (ch - '0');
    }
    if (absVal < 0 || (size_t)absVal >= stackSize) {
      goto bad_level;
    }
    *absLevel = (size_t)absVal;
    return TCL_OK;
  } else {
    // Relative level: N (use integer.get for proper parsing)
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
    FeatherObj msg3 = ops->string.intern(interp, "\"", 1);
    FeatherObj msg = ops->string.concat(interp, msg1, levelObj);
    msg = ops->string.concat(interp, msg, msg3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
