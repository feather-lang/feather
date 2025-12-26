#include "feather.h"
#include "internal.h"
#include "level_parse.h"

FeatherResult feather_builtin_uplevel(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // uplevel requires at least 1 arg: script
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"uplevel ?level? command ?arg ...?\"", 59);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Make a copy for shifting
  FeatherObj argsCopy = ops->list.from(interp, args);

  // Get current level info
  size_t currentLevel = ops->frame.level(interp);
  size_t stackSize = ops->frame.size(interp);

  // Default level is 1 (caller's frame)
  size_t targetLevel = (currentLevel > 0) ? currentLevel - 1 : 0;

  // Check if first arg looks like a level (only if we have > 1 arg)
  if (argc > 1) {
    FeatherObj first = ops->list.at(interp, argsCopy, 0);
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
      if (feather_parse_level(ops, interp, first, currentLevel, stackSize, &parsedLevel) == TCL_OK) {
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
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"uplevel ?level? command ?arg ...?\"", 59);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build the script - if multiple args, concatenate with spaces
  FeatherObj script;
  if (argc == 1) {
    script = ops->list.at(interp, argsCopy, 0);
  } else {
    // Concatenate all remaining args with spaces
    script = ops->list.shift(interp, argsCopy);
    argc--;
    while (argc > 0) {
      FeatherObj space = ops->string.intern(interp, " ", 1);
      FeatherObj next = ops->list.shift(interp, argsCopy);
      script = ops->string.concat(interp, script, space);
      script = ops->string.concat(interp, script, next);
      argc--;
    }
  }

  // Save current active frame
  size_t savedLevel = currentLevel;

  // Set active frame to target
  if (ops->frame.set_active(interp, targetLevel) != TCL_OK) {
    FeatherObj msg = ops->string.intern(interp, "failed to set active frame", 26);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Evaluate the script in the target frame's context
  FeatherResult result = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

  // Restore active frame
  ops->frame.set_active(interp, savedLevel);

  return result;
}
