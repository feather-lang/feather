#include "feather.h"
#include "internal.h"

FeatherResult feather_obj_resolve_variable(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj name,
                                           FeatherObj *ns_out, FeatherObj *local_out) {
  size_t len = ops->string.byte_length(interp, name);

  // Check if qualified (contains "::")
  if (!feather_obj_is_qualified(ops, interp, name)) {
    // Case 1: Unqualified - just a local variable
    *ns_out = 0; // nil
    *local_out = name;
    return TCL_OK;
  }

  // Find the last "::" to split namespace and local name
  long last_sep = feather_obj_find_last_colons(ops, interp, name);
  if (last_sep < 0) {
    // Shouldn't happen since feather_obj_is_qualified returned true
    *ns_out = 0;
    *local_out = name;
    return TCL_OK;
  }

  // Local name is everything after the last "::"
  size_t local_start = (size_t)last_sep + 2;
  *local_out = ops->string.slice(interp, name, local_start, len);

  // Namespace is everything up to (and not including) the last "::"
  // Handle edge case: "::x" means namespace is "::", local is "x"
  if (last_sep == 0 && ops->string.byte_at(interp, name, 0) == ':') {
    // Name starts with "::" and last_sep is 0, meaning the pattern is "::local"
    // Check if there are more :: after position 0
    int has_more_sep = 0;
    for (size_t i = 2; i + 1 < len; i++) {
      int c1 = ops->string.byte_at(interp, name, i);
      if (c1 == ':') {
        int c2 = ops->string.byte_at(interp, name, i + 1);
        if (c2 == ':') {
          has_more_sep = 1;
          break;
        }
      }
    }
    if (!has_more_sep) {
      // Pattern is "::x", namespace is "::"
      *ns_out = ops->string.intern(interp, "::", 2);
      return TCL_OK;
    }
  }

  // Check if absolute (starts with "::")
  if (len >= 2 && ops->string.byte_at(interp, name, 0) == ':' &&
      ops->string.byte_at(interp, name, 1) == ':') {
    // Case 2: Absolute path
    if (last_sep == 0) {
      *ns_out = ops->string.intern(interp, "::", 2);
    } else {
      *ns_out = ops->string.slice(interp, name, 0, (size_t)last_sep);
    }
    return TCL_OK;
  }

  // Case 3: Relative path - prepend current namespace
  FeatherObj current_ns = ops->ns.current(interp);

  // Build the full namespace path
  // Relative part is name[0..last_sep)
  FeatherObj rel_part = ops->string.slice(interp, name, 0, (size_t)last_sep);

  if (feather_obj_is_global_ns(ops, interp, current_ns)) {
    // Current is global, just prepend "::"
    FeatherObj prefix = ops->string.intern(interp, "::", 2);
    *ns_out = ops->string.concat(interp, prefix, rel_part);
  } else {
    // Current is something like "::bar", append "::foo"
    FeatherObj sep = ops->string.intern(interp, "::", 2);
    FeatherObj full = ops->string.concat(interp, current_ns, sep);
    *ns_out = ops->string.concat(interp, full, rel_part);
  }

  return TCL_OK;
}

FeatherResult feather_obj_split_command(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj qualified,
                                        FeatherObj *ns_out, FeatherObj *name_out) {
  size_t len = ops->string.byte_length(interp, qualified);

  // Check if qualified (contains "::")
  if (!feather_obj_is_qualified(ops, interp, qualified)) {
    // Unqualified - just a command name, no namespace part
    *ns_out = 0; // nil
    *name_out = qualified;
    return TCL_OK;
  }

  // Find the last "::" to split namespace and simple name
  long last_sep = feather_obj_find_last_colons(ops, interp, qualified);
  if (last_sep < 0) {
    *ns_out = 0;
    *name_out = qualified;
    return TCL_OK;
  }

  // Simple name is everything after the last "::"
  size_t simple_start = (size_t)last_sep + 2;
  *name_out = ops->string.slice(interp, qualified, simple_start, len);

  // Namespace is everything up to (and not including) the last "::"
  // Handle edge case: "::x" means namespace is "::", simple is "x"
  if (last_sep == 0 && ops->string.byte_at(interp, qualified, 0) == ':') {
    // Check if there are more :: after position 0
    int has_more_sep = 0;
    for (size_t i = 2; i + 1 < len; i++) {
      int c1 = ops->string.byte_at(interp, qualified, i);
      if (c1 == ':') {
        int c2 = ops->string.byte_at(interp, qualified, i + 1);
        if (c2 == ':') {
          has_more_sep = 1;
          break;
        }
      }
    }
    if (!has_more_sep) {
      // Pattern is "::x", namespace is "::"
      *ns_out = ops->string.intern(interp, "::", 2);
      return TCL_OK;
    }
  }

  // Check if absolute (starts with "::")
  if (len >= 2 && ops->string.byte_at(interp, qualified, 0) == ':' &&
      ops->string.byte_at(interp, qualified, 1) == ':') {
    // Absolute path
    if (last_sep == 0) {
      *ns_out = ops->string.intern(interp, "::", 2);
    } else {
      *ns_out = ops->string.slice(interp, qualified, 0, (size_t)last_sep);
    }
    return TCL_OK;
  }

  // Relative path - prepend current namespace
  FeatherObj current_ns = ops->ns.current(interp);

  // Relative part is name[0..last_sep)
  FeatherObj rel_part = ops->string.slice(interp, qualified, 0, (size_t)last_sep);

  if (feather_obj_is_global_ns(ops, interp, current_ns)) {
    // Current is global, just prepend "::"
    FeatherObj prefix = ops->string.intern(interp, "::", 2);
    *ns_out = ops->string.concat(interp, prefix, rel_part);
  } else {
    // Current is something like "::bar", append "::foo"
    FeatherObj sep = ops->string.intern(interp, "::", 2);
    FeatherObj full = ops->string.concat(interp, current_ns, sep);
    *ns_out = ops->string.concat(interp, full, rel_part);
  }

  return TCL_OK;
}
