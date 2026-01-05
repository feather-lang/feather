#include "feather.h"
#include "internal.h"
#include "namespace_util.h"

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

  // First try the name as-is
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, oldName, NULL, NULL, NULL);

  if (cmdType == TCL_CMD_NONE) {
    // Try with :: prefix
    FeatherObj globalQualified = ops->string.intern(interp, "::", 2);
    globalQualified = ops->string.concat(interp, globalQualified, oldName);
    cmdType = feather_lookup_command(ops, interp, globalQualified, NULL, NULL, NULL);
    if (cmdType != TCL_CMD_NONE) {
      qualifiedOld = globalQualified;
    }
  } else if (!feather_obj_is_qualified(ops, interp, oldName)) {
    // Command found with unqualified name - qualify it for trace lookup
    FeatherObj globalQualified = ops->string.intern(interp, "::", 2);
    qualifiedOld = ops->string.concat(interp, globalQualified, oldName);
  }

  // Validate: old command must exist
  if (cmdType == TCL_CMD_NONE) {
    FeatherObj displayOld = feather_get_display_name(ops, interp, oldName);
    FeatherObj msg = ops->string.intern(interp, "can't rename \"", 14);
    msg = ops->string.concat(interp, msg, displayOld);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\": command doesn't exist", 24));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
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

  // Validate: new command must not exist (if newName is not empty)
  if (newLen > 0) {
    FeatherCommandType newCmdType = feather_lookup_command(ops, interp, qualifiedNew, NULL, NULL, NULL);
    if (newCmdType != TCL_CMD_NONE) {
      FeatherObj displayNew = feather_get_display_name(ops, interp, newName);
      FeatherObj msg = ops->string.intern(interp, "can't rename to \"", 17);
      msg = ops->string.concat(interp, msg, displayNew);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\": command already exists", 25));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Perform the rename operation
  FeatherResult result = feather_rename_command(ops, interp, qualifiedOld, qualifiedNew);

  // Fire command traces if rename succeeded
  if (result == TCL_OK) {
    // Set empty result for successful rename
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    const char *op = (newLen == 0) ? "delete" : "rename";
    feather_fire_cmd_traces(ops, interp, qualifiedOld, qualifiedNew, op);
  }

  return result;
}

void feather_register_rename_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Rename or delete a command",
    "Renames a command from oldName to newName. The command can be invoked using "
    "the new name after the rename operation completes.\n\n"
    "If newName is an empty string, the command is deleted instead of renamed. "
    "This provides a way to remove commands from the interpreter.\n\n"
    "Both oldName and newName can be namespace-qualified (e.g., ::ns::cmd). "
    "If unqualified, oldName is resolved relative to the current namespace, "
    "and newName is created in the current namespace.\n\n"
    "The rename command fires command traces after a successful operation, "
    "with operation \"rename\" for normal renames or \"delete\" for deletions.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<oldName>");
  e = feather_usage_help(ops, interp, e,
    "The current name of the command to rename. The command must exist.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<newName>");
  e = feather_usage_help(ops, interp, e,
    "The new name for the command. Use an empty string to delete the command. "
    "If non-empty, a command with this name must not already exist.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "rename puts write",
    "Rename the puts command to write",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "rename myproc \"\"",
    "Delete the myproc command",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "rename ::ns::cmd ::other::newcmd",
    "Rename a command from one namespace to another",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "rename", spec);
}
