#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_list(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  // list command just returns its arguments as a proper list
  // The args are already a list, so we just need to return them
  ops->interp.set_result(interp, args);
  return TCL_OK;
}

void feather_register_list_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Create a list",
    "Returns a list comprised of all the supplied arguments. If no arguments "
    "are supplied, the result is an empty list.\n\n"
    "This command creates a list out of all its arguments, preserving the "
    "exact structure of each argument. Unlike concat, which removes one level "
    "of list structure, list preserves all arguments exactly as provided.\n\n"
    "The list command ensures that proper quoting and escaping is applied "
    "when the list is converted to a string representation, so commands like "
    "lindex can correctly extract the original arguments.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Variadic optional arguments
  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e, "Zero or more values to form into a list");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "list a b c",
    "Create a simple list with three elements",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "list a b \"c d e  \" \"  f {g h}\"",
    "Demonstrates quoting behavior. Returns: a b {c d e  } {  f {g h}}. "
    "Note how braces are added to preserve whitespace and special characters",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "list",
    "Create an empty list",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // See Also
  e = feather_usage_section(ops, interp, "See Also",
    "lappend, lindex, linsert, llength, lrange, lreplace, lsearch, lset, lsort, concat");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "list", spec);
}
