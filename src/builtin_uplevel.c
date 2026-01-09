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
    "All of the arg arguments are concatenated as if they had been passed to "
    "concat; the result is then evaluated in the variable context indicated by "
    "level. Uplevel returns the result of that evaluation.\n\n"
    "If level is an integer then it gives a distance (up the procedure calling "
    "stack) to move before executing the command. If level consists of # followed "
    "by an integer then the level gives an absolute level. If level is omitted "
    "then it defaults to 1. Level cannot be defaulted if the first command "
    "argument is an integer or starts with #.\n\n"
    "For example, suppose that procedure a was invoked from top-level, and that "
    "it called b, and that b called c. Suppose that c invokes the uplevel command. "
    "If level is 1 or #2 or omitted, then the command will be executed in the "
    "variable context of b. If level is 2 or #1 then the command will be executed "
    "in the variable context of a. If level is 3 or #0 then the command will be "
    "executed at top-level (only global variables will be visible).\n\n"
    "The uplevel command causes the invoking procedure to disappear from the "
    "procedure calling stack while the command is being executed. In the above "
    "example, suppose c invokes the command \"uplevel 1 {set x 43; d}\" where d "
    "is another procedure. The set command will modify the variable x in b's "
    "context, and d will execute at level 3, as if called from b. If it in turn "
    "executes the command \"uplevel {set x 42}\" then the set command will modify "
    "the same variable x in b's context: the procedure c does not appear to be on "
    "the call stack when d is executing. The info level command may be used to "
    "obtain the level of the current procedure.\n\n"
    "Uplevel makes it possible to implement new control constructs as procedures "
    "(for example, uplevel could be used to implement the while construct as a "
    "procedure).\n\n"
    "The namespace eval and apply commands offer other ways (besides procedure "
    "calls) that the naming context can change. They add a call frame to the stack "
    "to represent the namespace context. This means each namespace eval command "
    "counts as another call level for uplevel and upvar commands. For example, "
    "info level 1 will return a list describing a command that is either the "
    "outermost procedure call or the outermost namespace eval command. Also, "
    "uplevel #0 evaluates a script at top-level in the outermost namespace (the "
    "global namespace).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?level?");
  e = feather_usage_help(ops, interp, e,
    "Stack frame level to use (default: 1). Can be a relative level (positive "
    "integer) or absolute level (#N). Relative levels count up from the current "
    "frame: 1 is the caller, 2 is the caller's caller, etc. Absolute levels count "
    "from the global frame: #0 is global, #1 is the first procedure call, etc.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<arg>...");
  e = feather_usage_help(ops, interp, e,
    "One or more arguments forming the script. Multiple arguments are concatenated "
    "with spaces (as if passed to concat) to form the script to evaluate.");
  e = feather_usage_type(ops, interp, e, "script");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "uplevel 1 {set x 43; d}",
    "Set variable x in the caller's frame and invoke procedure d:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "uplevel #0 {set globalVar 5}",
    "Execute code at the global level (top-level frame):",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc do {body while condition} {\n"
    "    if {$while ne \"while\"} { error \"required word missing\" }\n"
    "    set conditionCmd [list expr $condition]\n"
    "    while {1} {\n"
    "        uplevel 1 $body\n"
    "        if {![uplevel 1 $conditionCmd]} { break }\n"
    "    }\n"
    "}",
    "Implement a do-while control construct using uplevel:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "apply(1), namespace(1), upvar(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "uplevel", spec);
}
