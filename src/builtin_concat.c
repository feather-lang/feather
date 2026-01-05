#include "feather.h"
#include "internal.h"
#include "charclass.h"

FeatherResult feather_builtin_concat(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  // Empty args returns empty string
  if (argc == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // concat joins arguments with spaces, trimming leading/trailing whitespace
  // from each argument
  FeatherObj result = 0;

  for (size_t i = 0; i < argc; i++) {
    FeatherObj arg = ops->list.shift(interp, args);
    size_t len = ops->string.byte_length(interp, arg);

    // Trim leading whitespace using byte_at
    size_t start = 0;
    while (start < len && feather_is_whitespace_full(ops->string.byte_at(interp, arg, start))) start++;

    // Trim trailing whitespace
    size_t end = len;
    while (end > start && feather_is_whitespace_full(ops->string.byte_at(interp, arg, end - 1))) end--;

    // Skip empty segments
    if (start >= end) continue;

    // Use slice to extract trimmed portion
    FeatherObj trimmed = ops->string.slice(interp, arg, start, end);

    if (ops->list.is_nil(interp, result) || result == 0) {
      result = trimmed;
    } else {
      FeatherObj space = ops->string.intern(interp, " ", 1);
      result = ops->string.concat(interp, result, space);
      result = ops->string.concat(interp, result, trimmed);
    }
  }

  if (ops->list.is_nil(interp, result) || result == 0) {
    result = ops->string.intern(interp, "", 0);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

void feather_register_concat_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Join arguments with spaces",
    "Joins each of its arguments together with spaces after trimming leading "
    "and trailing whitespace from each of them. Arguments that are empty after "
    "trimming are ignored entirely.\n\n"
    "If no arguments are provided, returns an empty string. Internal whitespace "
    "within arguments is preserved.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e,
    "Zero or more arguments to concatenate. Each argument will have leading "
    "and trailing whitespace trimmed before joining");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "concat a b c",
    "Simple concatenation:",
    "a b c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "concat \"  hello  \" \"  world  \"",
    "Trimming whitespace:",
    "hello world");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "concat \"a b\" c {d   e}",
    "Preserving internal spaces:",
    "a b c d   e");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "concat",
    "No arguments returns empty string:",
    "");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "concat", spec);
}
