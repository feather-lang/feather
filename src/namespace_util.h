#ifndef FEATHER_NAMESPACE_UTIL_H
#define FEATHER_NAMESPACE_UTIL_H

#include "feather.h"

FeatherObj feather_get_display_name(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj name);

/**
 * feather_register_command registers a command with a possibly-qualified name.
 *
 * For builtins: kind=TCL_CMD_BUILTIN, fn=builtin function, params=0, body=0
 * For procs: kind=TCL_CMD_PROC, fn=NULL, params=param list, body=body script
 *
 * Handles splitting qualified names and creating namespaces as needed.
 * The qualified name should be fully qualified (starting with ::) for
 * absolute paths, or unqualified for current namespace registration.
 */
void feather_register_command(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj qualifiedName, FeatherCommandType kind,
                              FeatherBuiltinCmd fn, FeatherObj params, FeatherObj body);

/**
 * feather_lookup_command looks up a command by possibly-qualified name.
 *
 * Returns the command type (TCL_CMD_BUILTIN, TCL_CMD_PROC, or TCL_CMD_NONE).
 * If fn is non-NULL, stores the builtin function pointer there.
 * If params is non-NULL and the command is a proc, stores the params.
 * If body is non-NULL and the command is a proc, stores the body.
 *
 * Search order for unqualified names:
 *   1. Current namespace
 *   2. Global namespace
 *
 * Qualified names are looked up directly in the specified namespace.
 */
FeatherCommandType feather_lookup_command(const FeatherHostOps *ops, FeatherInterp interp,
                                          FeatherObj name, FeatherBuiltinCmd *fn,
                                          FeatherObj *params, FeatherObj *body);

/**
 * feather_proc_exists checks if a proc exists with the given name.
 *
 * Returns 1 if a user-defined proc exists, 0 otherwise.
 * Returns 0 for builtins.
 */
int feather_proc_exists(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj name);

/**
 * feather_delete_command deletes a command by possibly-qualified name.
 *
 * Returns TCL_OK on success, TCL_ERROR if command doesn't exist.
 */
FeatherResult feather_delete_command(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj name);

/**
 * feather_rename_command renames a command from oldName to newName.
 *
 * Both names should be fully qualified.
 * If newName is empty, the command is deleted.
 * Returns TCL_OK on success, TCL_ERROR on failure.
 */
FeatherResult feather_rename_command(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj oldName, FeatherObj newName);

#endif
