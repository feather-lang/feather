#include "feather.h"
#include "internal.h"

void feather_register_tailcall_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Replace the current procedure with another command",
    "Replaces the currently executing procedure, lambda application, or method "
    "with another command. The command, which will have arg ... passed as "
    "arguments if they are supplied, will be looked up in the current namespace "
    "context, not in the caller's. Apart from that difference in resolution, "
    "it is equivalent to:\n\n"
    "    return [uplevel 1 [list command ?arg ...?]]\n\n"
    "This command may not be invoked from within an uplevel into a procedure "
    "or inside a catch inside a procedure or lambda.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<command>");
  e = feather_usage_help(ops, interp, e, "The command to execute as replacement");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e, "Arguments to pass to the command");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc factorial {n {acc 1}} {\n"
    "    if {$n <= 1} { return $acc }\n"
    "    tailcall factorial [expr {$n - 1}] [expr {$acc * $n}]\n"
    "}",
    "Tail-recursive factorial using accumulator pattern:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc countdown {n} {\n"
    "    if {$n <= 0} { return \"Done!\" }\n"
    "    puts $n\n"
    "    tailcall countdown [expr {$n - 1}]\n"
    "}",
    "Countdown without growing the call stack:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "apply, proc, uplevel");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "tailcall", spec);
}

FeatherResult feather_builtin_tailcall(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"tailcall command ?arg ...?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Check we're inside a proc (level > 0)
  size_t level = ops->frame.level(interp);
  if (level == 0) {
    FeatherObj msg = ops->string.intern(interp,
        "tailcall can only be called from a proc or lambda", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build the command to execute
  // The args list is already [cmdName, arg1, arg2, ...]
  FeatherObj tailCmd = args;

  // Save the current namespace BEFORE popping the frame
  // TCL specifies: "will be looked up in the current namespace context, not in the caller's"
  FeatherObj savedNs = ops->ns.current(interp);

  // Pop current frame - this makes the caller's frame active
  ops->frame.pop(interp);

  // Temporarily set namespace to the original proc's namespace for command lookup
  FeatherObj callerNs = ops->ns.current(interp);
  ops->frame.set_namespace(interp, savedNs);

  // Execute the command with the original namespace context
  // The command is already a list: [cmdName, arg1, arg2, ...]
  FeatherResult rc = feather_command_exec(ops, interp, tailCmd, TCL_EVAL_LOCAL);

  // Restore the caller's namespace
  ops->frame.set_namespace(interp, callerNs);

  // If command succeeded, return TCL_RETURN to stop body evaluation.
  // If command failed, propagate the error.
  // Note: feather_invoke_proc will try to pop the frame again, but since
  // we already popped it, frame.pop will return TCL_ERROR (no-op).
  if (rc == TCL_OK) {
    // Set up return options so feather_invoke_proc processes this correctly
    // With -code 0 -level 1, when level decrements to 0, code becomes TCL_OK
    FeatherObj opts = ops->list.create(interp);
    opts = ops->list.push(interp, opts, ops->string.intern(interp, "-code", 5));
    opts = ops->list.push(interp, opts, ops->integer.create(interp, TCL_OK));
    opts = ops->list.push(interp, opts, ops->string.intern(interp, "-level", 6));
    opts = ops->list.push(interp, opts, ops->integer.create(interp, 1));
    ops->interp.set_return_options(interp, opts);
    return TCL_RETURN;
  }

  return rc;
}
