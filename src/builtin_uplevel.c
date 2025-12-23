#include "tclc.h"
#include "internal.h"

// Parse a level specification and compute the absolute frame level
// Level can be:
//   N     - relative level (N levels up from current)
//   #N    - absolute frame level
// Returns TCL_OK on success and sets *absLevel, TCL_ERROR on failure
static TclResult parse_level(const TclHostOps *ops, TclInterp interp,
                              TclObj levelObj, size_t currentLevel,
                              size_t stackSize, size_t *absLevel) {
  size_t len;
  const char *str = ops->string.get(interp, levelObj, &len);

  if (len > 0 && str[0] == '#') {
    // Absolute level: #N
    // Parse the number after #
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
    // Relative level: N levels up
    int64_t relVal;
    if (ops->integer.get(interp, levelObj, &relVal) != TCL_OK) {
      goto bad_level;
    }
    if (relVal < 0) {
      goto bad_level;
    }
    // currentLevel - relVal = target level
    if ((size_t)relVal > currentLevel) {
      goto bad_level;
    }
    *absLevel = currentLevel - (size_t)relVal;
    return TCL_OK;
  }

bad_level:
  {
    TclObj msg1 = ops->string.intern(interp, "bad level \"", 11);
    TclObj msg2 = ops->string.intern(interp, str, len);
    TclObj msg3 = ops->string.intern(interp, "\"", 1);
    TclObj msg = ops->string.concat(interp, msg1, msg2);
    msg = ops->string.concat(interp, msg, msg3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}

TclResult tcl_builtin_uplevel(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // uplevel requires at least 1 arg: script
  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"uplevel ?level? script ?arg ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Make a copy for shifting
  TclObj argsCopy = ops->list.from(interp, args);

  // Get current level info
  size_t currentLevel = ops->frame.level(interp);
  size_t stackSize = ops->frame.size(interp);

  // Default level is 1 (caller's frame)
  size_t targetLevel = (currentLevel > 0) ? currentLevel - 1 : 0;

  // Check if first arg looks like a level (only if we have > 1 arg)
  if (argc > 1) {
    TclObj first = ops->list.at(interp, argsCopy, 0);
    size_t firstLen;
    const char *firstStr = ops->string.get(interp, first, &firstLen);

    // A level starts with # or is purely numeric
    int looksLikeLevel = 0;
    if (firstLen > 0) {
      if (firstStr[0] == '#') {
        looksLikeLevel = 1;
      } else if (firstStr[0] >= '0' && firstStr[0] <= '9') {
        // Check if entirely digits
        looksLikeLevel = 1;
        for (size_t i = 0; i < firstLen; i++) {
          if (firstStr[i] < '0' || firstStr[i] > '9') {
            looksLikeLevel = 0;
            break;
          }
        }
      }
    }

    if (looksLikeLevel) {
      size_t parsedLevel;
      if (parse_level(ops, interp, first, currentLevel, stackSize, &parsedLevel) == TCL_OK) {
        targetLevel = parsedLevel;
        ops->list.shift(interp, argsCopy);
        argc--;
      } else {
        // Parse failed - this is an error since it looks like a level
        return TCL_ERROR;  // Error already set by parse_level
      }
    }
  }

  // We need at least one script arg
  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"uplevel ?level? script ?arg ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build the script - if multiple args, concatenate with spaces
  TclObj script;
  if (argc == 1) {
    script = ops->list.at(interp, argsCopy, 0);
  } else {
    // Concatenate all remaining args with spaces
    script = ops->list.shift(interp, argsCopy);
    argc--;
    while (argc > 0) {
      TclObj space = ops->string.intern(interp, " ", 1);
      TclObj next = ops->list.shift(interp, argsCopy);
      script = ops->string.concat(interp, script, space);
      script = ops->string.concat(interp, script, next);
      argc--;
    }
  }

  // Save current active frame
  size_t savedLevel = currentLevel;

  // Set active frame to target
  if (ops->frame.set_active(interp, targetLevel) != TCL_OK) {
    TclObj msg = ops->string.intern(interp, "failed to set active frame", 26);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Evaluate the script in the target frame's context
  TclResult result = tcl_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

  // Restore active frame
  ops->frame.set_active(interp, savedLevel);

  return result;
}
