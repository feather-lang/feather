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

    // A level starts with # or is purely numeric
    int looksLikeLevel = 0;
    if (feather_obj_starts_with_char(ops, interp, first, '#')) {
      looksLikeLevel = 1;
    } else if (feather_obj_is_pure_digits(ops, interp, first)) {
      looksLikeLevel = 1;
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

void feather_register_uplevel_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Execute a script in a different stack frame",
    "Evaluates a command in the context of a different stack frame. This allows "
    "a procedure to execute code as if it were running in the context of one of "
    "its callers.\n\n"
    "The level argument specifies which stack frame to use. It can be in one of "
    "two forms:\n\n"
    "A positive integer N specifies a relative level, moving N frames up the call "
    "stack from the current frame. Level 1 (the default) refers to the caller's "
    "frame, level 2 to the caller's caller, and so on.\n\n"
    "A number preceded by # (e.g., #0, #1) specifies an absolute frame number, "
    "where #0 is the global frame, #1 is the first procedure call, etc.\n\n"
    "If the first argument could be interpreted as a level (it starts with # or "
    "consists only of digits), it will be treated as the level specifier. Otherwise, "
    "the level defaults to 1.\n\n"
    "When multiple arguments are provided after the level, they are concatenated "
    "with spaces to form the script, similar to the concat command. The script is "
    "then evaluated in the specified frame's variable context. Variables set or "
    "modified by the script will affect the target frame, not the current frame.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?level?");
  e = feather_usage_help(ops, interp, e,
    "Stack frame level to use (default: 1). Can be a relative level (positive "
    "integer) or absolute level (#N). Relative levels count up from the current "
    "frame: 1 is the caller, 2 is the caller's caller, etc. Absolute levels count "
    "from the global frame: #0 is global, #1 is the first procedure call, etc.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<command>");
  e = feather_usage_help(ops, interp, e,
    "The command or script to execute in the target frame.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e,
    "Additional arguments to concatenate with command (joined with spaces).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "uplevel set x 10",
    "Set variable x to 10 in the caller's frame:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "uplevel 1 {expr $a + $b}",
    "Evaluate an expression using variables from the caller's frame:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "uplevel 2 return 42",
    "Return from the caller's caller with value 42:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "uplevel #0 {global myvar; set myvar 5}",
    "Execute code at the global level:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "uplevel", spec);
}
