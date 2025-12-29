#include "internal.h"
#include "feather.h"
#include "host.h"

/**
 * feather_get_var retrieves a variable and fires read traces.
 *
 * This is the traced version of ops->var.get().
 * All builtins should use this instead of ops->var.get() directly.
 */
FeatherObj feather_get_var(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj name) {
  ops = feather_get_ops(ops);
  FeatherObj value = ops->var.get(interp, name);
  feather_fire_var_traces(ops, interp, name, "read");
  return value;
}

/**
 * feather_set_var sets a variable and fires write traces.
 *
 * This is the traced version of ops->var.set().
 * All builtins should use this instead of ops->var.set() directly.
 */
void feather_set_var(const FeatherHostOps *ops, FeatherInterp interp,
                     FeatherObj name, FeatherObj value) {
  ops = feather_get_ops(ops);
  ops->var.set(interp, name, value);
  feather_fire_var_traces(ops, interp, name, "write");
}

/**
 * feather_unset_var unsets a variable and fires unset traces.
 *
 * This is the traced version of ops->var.unset().
 * All builtins should use this instead of ops->var.unset() directly.
 * Note: unset traces fire BEFORE the variable is actually unset.
 */
void feather_unset_var(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj name) {
  ops = feather_get_ops(ops);
  feather_fire_var_traces(ops, interp, name, "unset");
  ops->var.unset(interp, name);
}
