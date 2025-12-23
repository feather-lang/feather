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

  // Resolve oldName to fully qualified form
  size_t oldLen;
  const char *oldStr = ops->string.get(interp, oldName, &oldLen);

  TclObj qualifiedOld = oldName;
  TclBuiltinCmd unusedFn = NULL;

  // First try the name as-is
  TclCommandType cmdType = ops->proc.lookup(interp, oldName, &unusedFn);

  if (cmdType == TCL_CMD_NONE) {
    // Try with :: prefix
    TclObj globalQualified = ops->string.intern(interp, "::", 2);
    globalQualified = ops->string.concat(interp, globalQualified, oldName);
    cmdType = ops->proc.lookup(interp, globalQualified, &unusedFn);
    if (cmdType != TCL_CMD_NONE) {
      qualifiedOld = globalQualified;
    }
  }

  // Resolve newName similarly if it's not empty
  size_t newLen;
  const char *newStr = ops->string.get(interp, newName, &newLen);

  TclObj qualifiedNew = newName;
  if (newLen > 0 && !tcl_is_qualified(newStr, newLen)) {
    // Prepend current namespace to new name (only if not in global namespace)
    // Global procs are stored without :: prefix
    TclObj currentNs = ops->ns.current(interp);
    size_t nsLen;
    const char *nsStr = ops->string.get(interp, currentNs, &nsLen);

    if (nsLen == 2 && nsStr[0] == ':' && nsStr[1] == ':') {
      // Global namespace: keep name as-is
      qualifiedNew = newName;
    } else {
      // Other namespace: "::ns::name"
      qualifiedNew = ops->string.concat(interp, currentNs,
                                        ops->string.intern(interp, "::", 2));
      qualifiedNew = ops->string.concat(interp, qualifiedNew, newName);
    }
  }

  // Delegate to host's rename operation with resolved names
  return ops->proc.rename(interp, qualifiedOld, qualifiedNew);
}
