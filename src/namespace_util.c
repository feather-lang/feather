#include "namespace_util.h"
#include "internal.h"

FeatherObj feather_get_display_name(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj name) {
  size_t len = ops->string.byte_length(interp, name);

  // Check if starts with "::"
  if (len > 2 &&
      ops->string.byte_at(interp, name, 0) == ':' &&
      ops->string.byte_at(interp, name, 1) == ':') {
    // Check for another :: after the prefix
    for (size_t i = 2; i + 1 < len; i++) {
      if (ops->string.byte_at(interp, name, i) == ':' &&
          ops->string.byte_at(interp, name, i + 1) == ':') {
        return name;  // Has more qualifiers, return as-is
      }
    }
    // Just "::foo" pattern - return without leading ::
    return ops->string.slice(interp, name, 2, len);
  }
  return name;
}

void feather_register_command(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj qualifiedName, FeatherCommandType kind,
                              FeatherBuiltinCmd fn, FeatherObj params, FeatherObj body) {
  // Split the qualified name into namespace and simple name
  FeatherObj ns, simpleName;
  feather_obj_split_command(ops, interp, qualifiedName, &ns, &simpleName);

  // If no namespace part, use global
  if (ops->list.is_nil(interp, ns)) {
    ns = ops->string.intern(interp, "::", 2);
  }

  // Create namespace if needed
  ops->ns.create(interp, ns);

  // Store command in namespace
  ops->ns.set_command(interp, ns, simpleName, kind, fn, params, body);
}

FeatherCommandType feather_lookup_command(const FeatherHostOps *ops, FeatherInterp interp,
                                          FeatherObj name, FeatherBuiltinCmd *fn,
                                          FeatherObj *params, FeatherObj *body) {
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj currentNs = ops->ns.current(interp);
  int inGlobalNs = feather_obj_is_global_ns(ops, interp, currentNs);

  FeatherCommandType cmdType = TCL_CMD_NONE;
  FeatherObj lookupNs, simpleName;

  if (feather_obj_is_qualified(ops, interp, name)) {
    // Qualified name - split and look up in the target namespace
    feather_obj_split_command(ops, interp, name, &lookupNs, &simpleName);
    if (ops->list.is_nil(interp, lookupNs)) {
      lookupNs = globalNs;
    }
    cmdType = ops->ns.get_command(interp, lookupNs, simpleName, fn, params, body);
  } else {
    // Unqualified name - try current namespace first, then global
    if (!inGlobalNs) {
      cmdType = ops->ns.get_command(interp, currentNs, name, fn, params, body);
    }

    // If not found in current namespace, try global namespace
    if (cmdType == TCL_CMD_NONE) {
      cmdType = ops->ns.get_command(interp, globalNs, name, fn, params, body);
    }
  }

  return cmdType;
}

int feather_proc_exists(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj name) {
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, name, NULL, NULL, NULL);
  return cmdType == TCL_CMD_PROC;
}

FeatherResult feather_delete_command(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj name) {
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj ns, simpleName;

  if (feather_obj_is_qualified(ops, interp, name)) {
    // Qualified name - split to get namespace and simple name
    feather_obj_split_command(ops, interp, name, &ns, &simpleName);
    if (ops->list.is_nil(interp, ns)) {
      ns = globalNs;
    }
  } else {
    // Unqualified name - use current namespace
    ns = ops->ns.current(interp);
    simpleName = name;
  }

  return ops->ns.delete_command(interp, ns, simpleName);
}

FeatherResult feather_rename_command(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj oldName, FeatherObj newName) {
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);

  // Split old name
  FeatherObj oldNs, oldSimple;
  feather_obj_split_command(ops, interp, oldName, &oldNs, &oldSimple);
  if (ops->list.is_nil(interp, oldNs)) {
    oldNs = globalNs;
  }

  // Get the command from old namespace
  FeatherBuiltinCmd fn = NULL;
  FeatherObj params = 0, body = 0;
  FeatherCommandType cmdType = ops->ns.get_command(interp, oldNs, oldSimple, &fn, &params, &body);

  if (cmdType == TCL_CMD_NONE) {
    return TCL_ERROR;
  }

  // Check if newName is empty (delete operation)
  size_t newLen = ops->string.byte_length(interp, newName);
  if (newLen == 0) {
    // Just delete the old command
    return ops->ns.delete_command(interp, oldNs, oldSimple);
  }

  // Split new name
  FeatherObj newNs, newSimple;
  feather_obj_split_command(ops, interp, newName, &newNs, &newSimple);
  if (ops->list.is_nil(interp, newNs)) {
    newNs = globalNs;
  }

  // Create namespace if needed
  ops->ns.create(interp, newNs);

  // Register command with new name
  ops->ns.set_command(interp, newNs, newSimple, cmdType, fn, params, body);

  // Delete old command
  ops->ns.delete_command(interp, oldNs, oldSimple);

  return TCL_OK;
}
