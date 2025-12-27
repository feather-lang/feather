#include "feather.h"
#include "internal.h"
#include "level_parse.h"

FeatherResult feather_builtin_upvar(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // upvar requires at least 2 args: otherVar localVar
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"upvar ?level? otherVar localVar ?otherVar localVar ...?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Make a copy for shifting
  FeatherObj argsCopy = ops->list.from(interp, args);

  // Get current and stack info
  size_t currentLevel = ops->frame.level(interp);
  size_t stackSize = ops->frame.size(interp);

  // Default level is 1 (caller's frame)
  size_t targetLevel = (currentLevel > 0) ? currentLevel - 1 : 0;

  // Check if first arg looks like a level
  FeatherObj first = ops->list.at(interp, argsCopy, 0);

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
    if (feather_obj_starts_with_char(ops, interp, first, '#')) {
      looksLikeLevel = 1;
    } else if (feather_obj_is_pure_digits(ops, interp, first)) {
      looksLikeLevel = 1;
    }

    if (looksLikeLevel) {
      // Try to parse as level
      size_t parsedLevel;
      if (feather_parse_level(ops, interp, first, currentLevel, stackSize, &parsedLevel) == TCL_OK) {
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
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"upvar ?level? otherVar localVar ?otherVar localVar ...?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Process each pair
  while (argc >= 2) {
    FeatherObj otherVar = ops->list.shift(interp, argsCopy);
    FeatherObj localVar = ops->list.shift(interp, argsCopy);
    argc -= 2;

    // Create the link
    ops->var.link(interp, localVar, targetLevel, otherVar);
  }

  // Return empty string on success
  FeatherObj empty = ops->string.intern(interp, "", 0);
  ops->interp.set_result(interp, empty);
  return TCL_OK;
}
