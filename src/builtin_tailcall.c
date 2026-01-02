#include "feather.h"
#include "internal.h"

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
