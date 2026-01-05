#include "feather.h"

FeatherResult feather_builtin_break(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 0) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"break\"", 32);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_BREAK;
}

void feather_register_break_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Abort looping command",
    "This command may be invoked only inside the body of a looping command "
    "such as for, foreach, or while. It returns a TCL_BREAK code to signal "
    "the innermost containing loop command to terminate and return normally.\n\n"
    "The break command will also terminate an enclosing catch body, causing "
    "catch to return the break as an exception rather than catching it.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "for {set i 0} {$i < 10} {incr i} {\n"
    "    if {$i == 5} {\n"
    "        break\n"
    "    }\n"
    "    puts $i\n"
    "}",
    "Terminate a for loop early when i reaches 5",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "foreach item $list {\n"
    "    if {$item eq \"stop\"} {\n"
    "        break\n"
    "    }\n"
    "    process $item\n"
    "}",
    "Stop processing items when \"stop\" is encountered",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // SEE ALSO section
  e = feather_usage_section(ops, interp, "See Also",
    "catch, continue, for, foreach, return, while");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "break", spec);
}
