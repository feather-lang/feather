#include "feather.h"
#include "internal.h"
#include "index_parse.h"

void feather_register_linsert_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Insert elements into a list",
    "Returns a new list formed by inserting zero or more elements at a "
    "specified index in the list. The first argument is parsed as a list "
    "if it is a string.\n\n"
    "The index may be a non-negative integer, end, or end-N where N is "
    "a non-negative integer. If index is less than or equal to zero, "
    "elements are inserted at the beginning. If index is greater than or "
    "equal to the list length, elements are appended to the end.\n\n"
    "When index is an integer or zero, the first inserted element will be at "
    "that index in the resulting list. When index is end-relative, the last "
    "inserted element will be at that index in the resulting list.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<list>");
  e = feather_usage_help(ops, interp, e, "The list to insert elements into");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<index>");
  e = feather_usage_help(ops, interp, e, "Position to insert at (integer, end, or end-N)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?element?...");
  e = feather_usage_help(ops, interp, e, "Elements to insert (zero or more)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "linsert {a b c} 0 X Y",
    "Insert at beginning:",
    "X Y a b c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "linsert {a b c} 2 X Y",
    "Insert before index 2:",
    "a b X Y c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "linsert {a b c} end X Y",
    "Append to end:",
    "a b c X Y");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "linsert {a b c} end-1 X",
    "Insert before last element:",
    "a b X c");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "list, lappend, lassign, lindex, llength, lmap, lrange, "
    "lrepeat, lreplace, lreverse, lsearch, lset, lsort");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "linsert", spec);
}

FeatherResult feather_builtin_linsert(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"linsert list index ?element ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj listObj = ops->list.shift(interp, args);
  FeatherObj indexObj = ops->list.shift(interp, args);

  FeatherObj list = ops->list.from(interp, listObj);
  if (list == 0) {
    return TCL_ERROR;
  }
  size_t listLen = ops->list.length(interp, list);

  // Check if index starts with "end"
  size_t idxLen = ops->string.byte_length(interp, indexObj);
  int is_end_relative = (idxLen >= 3 &&
                         ops->string.byte_at(interp, indexObj, 0) == 'e' &&
                         ops->string.byte_at(interp, indexObj, 1) == 'n' &&
                         ops->string.byte_at(interp, indexObj, 2) == 'd');

  int64_t index;
  if (feather_parse_index(ops, interp, indexObj, listLen, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  if (is_end_relative) {
    index += 1;
  }

  if (index < 0) index = 0;
  if (index > (int64_t)listLen) index = (int64_t)listLen;

  FeatherObj result = ops->list.splice(interp, list, (size_t)index, 0, args);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
