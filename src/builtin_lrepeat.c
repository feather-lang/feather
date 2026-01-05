#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_lrepeat(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"lrepeat count ?value ...?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj countObj = ops->list.shift(interp, args);
  int64_t count;
  if (ops->integer.get(interp, countObj, &count) != TCL_OK) {
    feather_error_expected(ops, interp, "integer", countObj);
    return TCL_ERROR;
  }

  if (count < 0) {
    FeatherObj part1 = ops->string.intern(interp, "bad count \"", 11);
    FeatherObj part3 = ops->string.intern(interp, "\": must be integer >= 0", 23);
    FeatherObj msg = ops->string.concat(interp, part1, countObj);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  size_t numElements = argc - 1;
  FeatherObj result = ops->list.create(interp);

  if (numElements == 0 || count == 0) {
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  FeatherObj elements = ops->list.from(interp, args);
  if (elements == 0) {
    return TCL_ERROR;
  }

  for (int64_t i = 0; i < count; i++) {
    for (size_t j = 0; j < numElements; j++) {
      FeatherObj elem = ops->list.at(interp, elements, j);
      result = ops->list.push(interp, result, elem);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_lrepeat_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Build a list by repeating elements",
    "Creates a list consisting of the given elements repeated count times. "
    "The count must be a non-negative integer. If count is 0 or no elements "
    "are provided, returns an empty list.\n\n"
    "The result list will contain count repetitions of the element sequence. "
    "For example, if three elements are provided, the result will contain "
    "those three elements repeated count times in order.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<count>");
  e = feather_usage_help(ops, interp, e,
    "A non-negative integer specifying how many times to repeat the elements");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?value?...");
  e = feather_usage_help(ops, interp, e,
    "Zero or more elements to repeat. If no elements are provided, returns an empty list");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrepeat 3 a",
    "Repeat single element three times:",
    "a a a");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrepeat 2 a b c",
    "Repeat multiple elements twice:",
    "a b c a b c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrepeat 0 x y",
    "Zero count returns empty list:",
    "");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "lrepeat 3",
    "No elements returns empty list:",
    "");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "lrepeat", spec);
}
