#include "feather.h"
#include "internal.h"

/**
 * feather_builtin_rename implements the TCL 'rename' command.
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
FeatherResult feather_builtin_rename(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    FeatherObj msg = ops->string.intern(
        interp, "wrong # args: should be \"rename oldName newName\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj oldName = ops->list.at(interp, args, 0);
  FeatherObj newName = ops->list.at(interp, args, 1);

  // Resolve oldName to fully qualified form
  FeatherObj qualifiedOld = oldName;
  FeatherBuiltinCmd unusedFn = NULL;

  // First try the name as-is
  FeatherCommandType cmdType = ops->proc.lookup(interp, oldName, &unusedFn);

  if (cmdType == TCL_CMD_NONE) {
    // Try with :: prefix
    FeatherObj globalQualified = ops->string.intern(interp, "::", 2);
    globalQualified = ops->string.concat(interp, globalQualified, oldName);
    cmdType = ops->proc.lookup(interp, globalQualified, &unusedFn);
    if (cmdType != TCL_CMD_NONE) {
      qualifiedOld = globalQualified;
    }
  }

  // Resolve newName similarly if it's not empty
  size_t newLen = ops->string.byte_length(interp, newName);

  FeatherObj qualifiedNew = newName;
  if (newLen > 0 && !feather_obj_is_qualified(ops, interp, newName)) {
    // Prepend current namespace to new name
    FeatherObj currentNs = ops->ns.current(interp);
    FeatherObj globalNs = ops->string.intern(interp, "::", 2);

    if (ops->string.equal(interp, currentNs, globalNs)) {
      // Global namespace: prepend "::"
      qualifiedNew = ops->string.concat(interp, globalNs, newName);
    } else {
      // Other namespace: "::ns::name"
      qualifiedNew = ops->string.concat(interp, currentNs, globalNs);
      qualifiedNew = ops->string.concat(interp, qualifiedNew, newName);
    }
  }

  // Delegate to host's rename operation with resolved names
  return ops->proc.rename(interp, qualifiedOld, qualifiedNew);
}
