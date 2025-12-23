#include "tclc.h"
#include "internal.h"

/**
 * tcl_builtin_rename implements the TCL 'rename' command.
 *
 * Usage:
 *   rename oldName newName
 *
 * Renames a command from oldName to newName.
 * If newName is an empty string, the command is deleted.
 *
 * Errors:
 *   - wrong # args: should be "rename oldName newName"
 *   - can't rename "oldName": command doesn't exist
 *   - can't rename to "newName": command already exists
 */
TclResult tcl_builtin_rename(const TclHostOps *ops, TclInterp interp,
                              TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    TclObj msg = ops->string.intern(
        interp, "wrong # args: should be \"rename oldName newName\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj oldName = ops->list.at(interp, args, 0);
  TclObj newName = ops->list.at(interp, args, 1);

  // Delegate to host's rename operation
  return ops->proc.rename(interp, oldName, newName);
}
