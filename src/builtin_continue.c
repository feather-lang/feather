#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_continue(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 0) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"continue\"", 35);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_CONTINUE;
}

void feather_register_continue_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Skip to the next iteration of a loop",
    "This command is typically invoked inside the body of a looping command "
    "such as for or foreach or while. It returns a TCL_CONTINUE code, which "
    "causes a continue exception to occur. The exception causes the current "
    "script to be aborted out to the innermost containing loop command, which "
    "then continues with the next iteration of the loop. Catch exceptions are "
    "also handled in a few other situations, such as the catch command and the "
    "outermost scripts of procedure bodies.\n\n"
    "The continue command takes no arguments.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "for {set x 0} {$x < 10} {incr x} {\n"
    "    if {$x % 2 == 0} {\n"
    "        continue\n"
    "    }\n"
    "    puts $x\n"
    "}",
    "Print only odd numbers from 0 to 9",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "foreach item $list {\n"
    "    if {$item eq \"\"} {\n"
    "        continue\n"
    "    }\n"
    "    process $item\n"
    "}",
    "Skip empty items in a list",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "continue", spec);
}
