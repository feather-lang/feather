#include "tclc.h"

TclResult tcl_builtin_set(const TclHostOps *ops, TclInterp interp,
                          TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    // No arguments - error
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the variable name (first argument)
  TclObj varName = ops->list.shift(interp, args);
  size_t nameLen;
  const char *nameStr = ops->string.get(interp, varName, &nameLen);

  // Resolve the variable name (handles qualified names)
  TclObj ns, localName;
  tcl_resolve_variable(ops, interp, nameStr, nameLen, &ns, &localName);

  if (argc == 1) {
    // One argument: get variable value
    TclObj value;
    if (ops->list.is_nil(interp, ns)) {
      // Unqualified - frame-local lookup
      value = ops->var.get(interp, localName);
    } else {
      // Qualified - namespace lookup
      value = ops->ns.get_var(interp, ns, localName);
    }

    if (ops->list.is_nil(interp, value)) {
      // Variable doesn't exist - build error message
      TclObj part1 = ops->string.intern(interp, "can't read \"", 12);
      TclObj part2 = ops->string.intern(interp, nameStr, nameLen);
      TclObj part3 = ops->string.intern(interp, "\": no such variable", 19);

      TclObj msg = ops->string.concat(interp, part1, part2);
      msg = ops->string.concat(interp, msg, part3);

      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    ops->interp.set_result(interp, value);
    return TCL_OK;
  }

  // Two arguments: set variable value
  if (argc > 2) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"set varName ?newValue?\"", 48);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  TclObj value = ops->list.shift(interp, args);

  if (ops->list.is_nil(interp, ns)) {
    // Unqualified - frame-local
    ops->var.set(interp, localName, value);
  } else {
    // Qualified - namespace
    ops->ns.set_var(interp, ns, localName, value);
  }

  ops->interp.set_result(interp, value);
  return TCL_OK;
}
