#include "feather.h"
#include "internal.h"

// Helper macro
#define S(lit) (lit), feather_strlen(lit)

FeatherResult feather_builtin_throw(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  // throw type message
  if (argc != 2) {
    FeatherObj msg = ops->string.intern(
        interp, S("wrong # args: should be \"throw type message\""));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the type (error code list) and message
  FeatherObj type = ops->list.at(interp, args, 0);
  FeatherObj message = ops->list.at(interp, args, 1);

  // Verify type is non-empty
  FeatherObj typeList = ops->list.from(interp, type);
  size_t typeLen = ops->list.length(interp, typeList);
  if (typeLen == 0) {
    FeatherObj msg = ops->string.intern(interp, S("type must be non-empty"));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build return options dictionary
  FeatherObj options = ops->list.create(interp);
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
