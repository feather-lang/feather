#include "internal.h"
#include "feather.h"
#include "host.h"

/**
 * feather_get_var retrieves a variable and fires read traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * For qualified names, looks up in namespace storage.
 * For unqualified names, uses frame-local lookup.
 * Fires read traces on the original name in all cases.
 */
FeatherObj feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj name) {
  ops = feather_get_ops(ops);

  // Resolve qualified name
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, name, &ns, &localName);

  FeatherObj value;
  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    value = ops->var.get(interp, localName);
  } else {
    // Qualified - namespace storage
    value = ops->ns.get_var(interp, ns, localName);
  }

  // Fire traces on original name
  feather_fire_var_traces(ops, interp, name, "read");
  return value;
}

/**
 * feather_set_var sets a variable and fires write traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * For qualified names, stores in namespace storage.
 * For unqualified names, uses frame-local storage.
 * Fires write traces on the original name in all cases.
 */
void feather_set_var(const FeatherHostOps *ops, FeatherInterp interp,
                     FeatherObj name, FeatherObj value) {
  ops = feather_get_ops(ops);

  // Resolve qualified name
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, name, &ns, &localName);

  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    ops->var.set(interp, localName, value);
  } else {
    // Qualified - namespace storage
    ops->ns.set_var(interp, ns, localName, value);
  }

  // Fire traces on original name
  feather_fire_var_traces(ops, interp, name, "write");
}

/**
 * feather_unset_var unsets a variable and fires unset traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * Fires unset traces on the original name BEFORE the variable is unset.
 */
void feather_unset_var(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name) {
  ops = feather_get_ops(ops);

  // Fire traces BEFORE unset (standard TCL behavior)
  feather_fire_var_traces(ops, interp, name, "unset");

  // Resolve qualified name
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, name, &ns, &localName);

  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    ops->var.unset(interp, localName);
  } else {
    // Qualified - namespace storage
    ops->ns.unset_var(interp, ns, localName);
  }
}

/**
 * feather_var_exists checks if a variable exists.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * Returns 1 if the variable exists, 0 otherwise.
 */
int feather_var_exists(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name) {
  ops = feather_get_ops(ops);

  // Resolve qualified name
  FeatherObj ns, localName;
  feather_obj_resolve_variable(ops, interp, name, &ns, &localName);

  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    return (ops->var.exists(interp, localName) == TCL_OK) ? 1 : 0;
  } else {
    // Qualified - namespace storage
    return ops->ns.var_exists(interp, ns, localName);
  }
}
