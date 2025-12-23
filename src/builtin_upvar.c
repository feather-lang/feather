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

TclResult tcl_builtin_upvar(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // upvar requires at least 2 args: otherVar localVar
  if (argc < 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"upvar ?level? otherVar localVar ?otherVar localVar ...?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Make a copy for shifting
  TclObj argsCopy = ops->list.from(interp, args);

  // Get current and stack info
  size_t currentLevel = ops->frame.level(interp);
  size_t stackSize = ops->frame.size(interp);

  // Default level is 1 (caller's frame)
  size_t targetLevel = (currentLevel > 0) ? currentLevel - 1 : 0;

  // Check if first arg looks like a level
  TclObj first = ops->list.at(interp, argsCopy, 0);
  size_t firstLen;
  const char *firstStr = ops->string.get(interp, first, &firstLen);

  // TCL's level detection rule: only consume the first arg as a level if
  // doing so leaves an EVEN number of remaining args (to form pairs).
  // This means:
  //   upvar 1 x       (2 args) → 1 left after consuming → odd → "1" is var name
  //   upvar 1 x y     (3 args) → 2 left after consuming → even → "1" is level
  //   upvar #0 x      (2 args) → 1 left after consuming → odd → "#0" is var name
  //   upvar #0 x y    (3 args) → 2 left after consuming → even → "#0" is level
  //
  // Only consider consuming as level if it would leave an even number of args
  if ((argc - 1) % 2 == 0 && argc >= 3) {
    // Check if first arg looks like a level (# prefix or purely numeric)
    int looksLikeLevel = 0;
    if (firstLen > 0 && firstStr[0] == '#') {
      looksLikeLevel = 1;
    } else if (firstLen > 0) {
      int isPurelyNumeric = 1;
      for (size_t i = 0; i < firstLen; i++) {
        if (firstStr[i] < '0' || firstStr[i] > '9') {
          isPurelyNumeric = 0;
          break;
        }
      }
      looksLikeLevel = isPurelyNumeric;
    }

    if (looksLikeLevel) {
      // Try to parse as level
      size_t parsedLevel;
      if (parse_level(ops, interp, first, currentLevel, stackSize, &parsedLevel) == TCL_OK) {
        targetLevel = parsedLevel;
        ops->list.shift(interp, argsCopy);
        argc--;
      } else {
        // Parse failed (e.g., level out of range) - this is an error
        return TCL_ERROR;  // Error already set by parse_level
      }
    }
  }

  // Now we need pairs of otherVar localVar
  if (argc < 2 || (argc % 2) != 0) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"upvar ?level? otherVar localVar ?otherVar localVar ...?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Process each pair
  while (argc >= 2) {
    TclObj otherVar = ops->list.shift(interp, argsCopy);
    TclObj localVar = ops->list.shift(interp, argsCopy);
    argc -= 2;

    // Create the link
    ops->var.link(interp, localVar, targetLevel, otherVar);
  }

  // Return empty string on success
  TclObj empty = ops->string.intern(interp, "", 0);
  ops->interp.set_result(interp, empty);
  return TCL_OK;
}
