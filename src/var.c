#include "internal.h"
#include "feather.h"
#include "host.h"

/**
 * feather_get_var retrieves a variable and fires read traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * For qualified names, looks up in namespace storage.
 * For unqualified names, uses frame-local lookup.
 * Fires read traces on the original name BEFORE reading the value,
 * so traces can modify the variable and the new value is returned.
 *
 * On success, returns TCL_OK and stores the value in *out.
 * On read trace error, returns TCL_ERROR with wrapped message.
 */
FeatherResult feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj name, FeatherObj *out) {
  ops = feather_get_ops(ops);

  // Fire traces on original name FIRST (before reading)
  // This allows read traces to modify the variable
  FeatherResult traceResult = feather_fire_var_traces(ops, interp, name, "read");
  if (traceResult != TCL_OK) {
    return traceResult;
  }

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

  *out = value;
  return TCL_OK;
}

/**
 * feather_set_var sets a variable and fires write traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * For qualified names, stores in namespace storage.
 * For unqualified names, uses frame-local storage.
 * Fires write traces on the original name in all cases.
 *
 * On write trace error, returns TCL_ERROR with wrapped message.
 * The variable IS set before the trace fires.
 */
FeatherResult feather_set_var(const FeatherHostOps *ops, FeatherInterp interp,
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
  return feather_fire_var_traces(ops, interp, name, "write");
}

/**
 * feather_unset_var unsets a variable and fires unset traces.
 *
 * Handles both qualified names (::foo::bar) and unqualified names (x).
 * Fires unset traces on the original name BEFORE the variable is unset.
 * Removes all traces on the variable after the unset completes.
 */
void feather_unset_var(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name) {
  ops = feather_get_ops(ops);

  // Fire traces BEFORE unset (standard TCL behavior)
  // Note: unset trace errors are ignored
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

  // Remove all traces on this variable (TCL behavior)
  FeatherObj traceDict = feather_trace_get_dict(ops, interp, "variable");
  traceDict = ops->dict.remove(interp, traceDict, name);
  feather_trace_set_dict(ops, interp, "variable", traceDict);
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
