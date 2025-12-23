#include "tclc.h"

int tcl_is_qualified(const char *name, size_t len) {
  // Look for "::" anywhere in the name
  for (size_t i = 0; i + 1 < len; i++) {
    if (name[i] == ':' && name[i + 1] == ':') {
      return 1;
    }
  }
  return 0;
}

TclResult tcl_resolve_variable(const TclHostOps *ops, TclInterp interp,
                               const char *name, size_t len,
                               TclObj *ns_out, TclObj *local_out) {
  // Find if name contains "::"
  if (!tcl_is_qualified(name, len)) {
    // Case 1: Unqualified - just a local variable
    *ns_out = 0; // nil
    *local_out = ops->string.intern(interp, name, len);
    return TCL_OK;
  }

  // Find the last "::" to split namespace and local name
  size_t last_sep = 0;
  int found = 0;
  for (size_t i = 0; i + 1 < len; i++) {
    if (name[i] == ':' && name[i + 1] == ':') {
      last_sep = i;
      found = 1;
    }
  }

  if (!found) {
    // Shouldn't happen since tcl_is_qualified returned true
    *ns_out = 0;
    *local_out = ops->string.intern(interp, name, len);
    return TCL_OK;
  }

  // Local name is everything after the last "::"
  size_t local_start = last_sep + 2;
  size_t local_len = len - local_start;
  *local_out = ops->string.intern(interp, name + local_start, local_len);

  // Namespace is everything up to (and not including) the last "::"
  // Handle edge case: "::x" means namespace is "::", local is "x"
  if (last_sep == 0 && name[0] == ':') {
    // Name starts with "::" and last_sep is 0, meaning the pattern is "::local"
    // Check if there are more :: after position 0
    int has_more_sep = 0;
    for (size_t i = 2; i + 1 < len; i++) {
      if (name[i] == ':' && name[i + 1] == ':') {
        has_more_sep = 1;
        break;
      }
    }
    if (!has_more_sep) {
      // Pattern is "::x", namespace is "::"
      *ns_out = ops->string.intern(interp, "::", 2);
      return TCL_OK;
    }
  }

  // Check if absolute (starts with "::")
  if (len >= 2 && name[0] == ':' && name[1] == ':') {
    // Case 2: Absolute path
    if (last_sep == 0) {
      *ns_out = ops->string.intern(interp, "::", 2);
    } else {
      *ns_out = ops->string.intern(interp, name, last_sep);
    }
    return TCL_OK;
  }

  // Case 3: Relative path - prepend current namespace
  TclObj current_ns = ops->ns.current(interp);
  size_t current_len;
  const char *current_str = ops->string.get(interp, current_ns, &current_len);

  // Build the full namespace path
  // If current is "::", relative "foo::x" becomes "::foo", local "x"
  // If current is "::bar", relative "foo::x" becomes "::bar::foo", local "x"

  // Relative part is name[0..last_sep)
  size_t rel_ns_len = last_sep;

  if (current_len == 2 && current_str[0] == ':' && current_str[1] == ':') {
    // Current is global, just prepend "::"
    TclObj prefix = ops->string.intern(interp, "::", 2);
    TclObj rel_part = ops->string.intern(interp, name, rel_ns_len);
    *ns_out = ops->string.concat(interp, prefix, rel_part);
  } else {
    // Current is something like "::bar", append "::foo"
    TclObj sep = ops->string.intern(interp, "::", 2);
    TclObj rel_part = ops->string.intern(interp, name, rel_ns_len);
    TclObj full = ops->string.concat(interp, current_ns, sep);
    *ns_out = ops->string.concat(interp, full, rel_part);
  }

  return TCL_OK;
}
