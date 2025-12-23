#include "tclc.h"
#include "internal.h"

// Helper macro
#define S(lit) (lit), tcl_strlen(lit)

TclResult tcl_builtin_throw(const TclHostOps *ops, TclInterp interp,
                             TclObj cmd, TclObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // throw type message
  if (argc != 2) {
    TclObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"throw type message\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the type (error code list) and message
  TclObj type = ops->list.at(interp, args, 0);
  TclObj message = ops->list.at(interp, args, 1);

  // Verify type is non-empty
  TclObj typeList = ops->list.from(interp, type);
  size_t typeLen = ops->list.length(interp, typeList);
  if (typeLen == 0) {
    TclObj msg = ops->string.intern(interp, S("type must be non-empty"));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build return options dictionary
  TclObj options = ops->list.create(interp);
  options = ops->list.push(interp, options,
                           ops->string.intern(interp, S("-code")));
  options = ops->list.push(interp, options, ops->integer.create(interp, 1));
  options = ops->list.push(interp, options,
                           ops->string.intern(interp, S("-errorcode")));
  options = ops->list.push(interp, options, type);

  // Store return options
  ops->interp.set_return_options(interp, options);

  // Set the error message as result
  ops->interp.set_result(interp, message);

  return TCL_ERROR;
}
