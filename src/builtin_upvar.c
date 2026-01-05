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

void feather_register_upvar_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Create link to variable in a different stack frame",
    "Creates one or more local variable names that are linked to variables in a "
    "different stack frame. This allows a procedure to access variables in its "
    "caller's scope or the global scope.\n\n"
    "The level argument specifies which stack frame to link to. It defaults to 1, "
    "meaning the caller's frame. Relative levels are specified as positive integers "
    "(1, 2, 3, etc.), where higher numbers go further up the call stack. Absolute "
    "levels use the #N syntax, where #0 refers to the global frame.\n\n"
    "For each otherVar/localVar pair, a local variable named localVar is created "
    "that links to the variable named otherVar in the target frame. Reading or "
    "writing localVar will actually read or write otherVar. The otherVar need not "
    "exist at the time upvar is called; it will be created when first accessed.\n\n"
    "Note: Feather does not support TCL-style arrays. Both otherVar and localVar "
    "must refer to scalar variables. Array element syntax like \"myArray(key)\" is "
    "not supported.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?level?");
  e = feather_usage_help(ops, interp, e,
    "Stack frame to link to: relative (1, 2, ...) or absolute (#0, #1, ...). Default: 1 (caller)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<otherVar>");
  e = feather_usage_help(ops, interp, e,
    "Name of variable in the target frame to link to");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<localVar>");
  e = feather_usage_help(ops, interp, e,
    "Name of local variable to create as a link");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?otherVar?...");
  e = feather_usage_help(ops, interp, e,
    "Additional variables in target frame (must be paired with localVar arguments)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?localVar?...");
  e = feather_usage_help(ops, interp, e,
    "Additional local variable names (must be paired with otherVar arguments)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc increment {varName} {\n    upvar 1 $varName var\n    set var [expr {$var + 1}]\n}\nset x 5\nincrement x\n# x is now 6",
    "Access caller's variable by name",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "upvar #0 globalCounter counter",
    "Create link to global variable",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "upvar 1 x localX y localY",
    "Create multiple variable links in one call",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "upvar 2 result myResult",
    "Link to variable two levels up the call stack",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "global(1), namespace(1), uplevel(1), variable(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "upvar", spec);
}
