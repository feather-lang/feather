#include "feather.h"
#include "internal.h"

void feather_register_lassign_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Assign list elements to variables",
    "Assigns successive elements from list to the variables given by the varName "
    "arguments in order. If there are more variable names than list elements, the "
    "remaining variables are set to the empty string. If there are more list elements "
    "than variables, a list of the unassigned elements is returned as the result of "
    "the command. If no varName arguments are provided, the command returns the entire "
    "list.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list whose elements are to be assigned");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?varName?...");
  e = feather_usage_help(ops, interp, e, "Names of variables to assign list elements to");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lassign {a b c} x y z",
    "Assigns x=a, y=b, z=c, returns empty string",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lassign {d e} x y z",
    "Assigns x=d, y=e, z=\"\", returns empty string",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lassign {f g h i} x y",
    "Assigns x=f, y=g, returns \"h i\"",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set argv [lassign $argv firstArg]",
    "Remove and return the first element (similar to shell's shift command)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "list(1), lappend(1), lindex(1), linsert(1), llength(1), lrange(1), "
    "lreplace(1), lsearch(1), lset(1), lsort(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lassign", spec);
}

FeatherResult feather_builtin_lassign(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"lassign list ?varName ...?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listArg = ops->list.at(interp, args, 0);
  FeatherObj list = ops->list.from(interp, listArg);
  if (list == 0) {
    return TCL_ERROR;
  }
  size_t listLen = ops->list.length(interp, list);

  size_t numVars = argc - 1;
  FeatherObj emptyStr = ops->string.intern(interp, "", 0);

  for (size_t i = 0; i < numVars; i++) {
    FeatherObj varName = ops->list.at(interp, args, i + 1);
    FeatherObj value;
    if (i < listLen) {
      value = ops->list.at(interp, list, i);
    } else {
      value = emptyStr;
    }
    if (feather_set_var(ops, interp, varName, value) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  if (numVars >= listLen) {
    ops->interp.set_result(interp, emptyStr);
  } else {
    FeatherObj remaining = ops->list.create(interp);
    for (size_t i = numVars; i < listLen; i++) {
      ops->list.push(interp, remaining, ops->list.at(interp, list, i));
    }
    ops->interp.set_result(interp, remaining);
  }

  return TCL_OK;
}
