#include "tclc.h"
#include "internal.h"

// Helper macro
#define S(lit) (lit), tcl_strlen(lit)

TclResult tcl_builtin_error(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // error message ?info? ?code?
  if (argc < 1 || argc > 3) {
    TclObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"error message ?info? ?code?\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the error message
  TclObj message = ops->list.at(interp, args, 0);

  // Build return options dictionary
  TclObj options = ops->list.create(interp);
  options = ops->list.push(interp, options,
                           ops->string.intern(interp, S("-code")));
  options = ops->list.push(interp, options, ops->integer.create(interp, 1));

  // Add -errorinfo if provided
  if (argc >= 2) {
    TclObj info = ops->list.at(interp, args, 1);
    options = ops->list.push(interp, options,
                             ops->string.intern(interp, S("-errorinfo")));
    options = ops->list.push(interp, options, info);
  }

  // Add -errorcode if provided
  if (argc >= 3) {
    TclObj code = ops->list.at(interp, args, 2);
    options = ops->list.push(interp, options,
                             ops->string.intern(interp, S("-errorcode")));
    options = ops->list.push(interp, options, code);
  }

  // Store return options
  ops->interp.set_return_options(interp, options);

  // Set the error message as result
  ops->interp.set_result(interp, message);

  return TCL_ERROR;
}
