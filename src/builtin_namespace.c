#include "tclc.h"
#include "internal.h"

// Helper to check if a string equals a literal
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t i = 0;
  while (i < len && lit[i] && s[i] == lit[i]) {
    i++;
  }
  return i == len && lit[i] == '\0';
}

// Resolve a namespace path (relative or absolute) to an absolute path
static TclObj resolve_ns_path(const TclHostOps *ops, TclInterp interp, TclObj path) {
  size_t len;
  const char *str = ops->string.get(interp, path, &len);

  // If absolute (starts with ::), return as-is
  if (len >= 2 && str[0] == ':' && str[1] == ':') {
    return path;
  }

  // Relative - prepend current namespace
  TclObj current = ops->ns.current(interp);
  size_t current_len;
  const char *current_str = ops->string.get(interp, current, &current_len);

  // If current is global "::", prepend "::" to path
  if (current_len == 2 && current_str[0] == ':' && current_str[1] == ':') {
    TclObj prefix = ops->string.intern(interp, "::", 2);
    return ops->string.concat(interp, prefix, path);
  }

  // Otherwise append "::" and path to current
  TclObj sep = ops->string.intern(interp, "::", 2);
  TclObj result = ops->string.concat(interp, current, sep);
  return ops->string.concat(interp, result, path);
}

// namespace current
static TclResult ns_current(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 0) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace current\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  TclObj current = ops->ns.current(interp);
  ops->interp.set_result(interp, current);
  return TCL_OK;
}

// namespace eval ns script
static TclResult ns_eval(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace eval name arg ?arg ...?\"", 59);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj ns_path = ops->list.at(interp, args, 0);
  TclObj abs_path = resolve_ns_path(ops, interp, ns_path);

  // Create namespace if it doesn't exist
  ops->ns.create(interp, abs_path);

  // Save current namespace
  TclObj saved_ns = ops->frame.get_namespace(interp);

  // Set current frame's namespace
  ops->frame.set_namespace(interp, abs_path);

  // Get the script (concatenate remaining args if multiple)
  TclObj script;
  if (argc == 2) {
    script = ops->list.at(interp, args, 1);
  } else {
    // Concatenate remaining arguments with spaces
    script = ops->list.at(interp, args, 1);
    for (size_t i = 2; i < argc; i++) {
      TclObj space = ops->string.intern(interp, " ", 1);
      script = ops->string.concat(interp, script, space);
      script = ops->string.concat(interp, script, ops->list.at(interp, args, i));
    }
  }

  // Evaluate the script
  TclResult result = tcl_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

  // Restore namespace
  ops->frame.set_namespace(interp, saved_ns);

  return result;
}

// namespace exists ns
static TclResult ns_exists(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 1) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace exists name\"", 47);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj ns_path = ops->list.at(interp, args, 0);
  TclObj abs_path = resolve_ns_path(ops, interp, ns_path);

  int exists = ops->ns.exists(interp, abs_path);
  TclObj result = ops->integer.create(interp, exists ? 1 : 0);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// namespace children ?ns?
static TclResult ns_children(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace children ?name?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj ns_path;
  if (argc == 0) {
    ns_path = ops->ns.current(interp);
  } else {
    ns_path = ops->list.at(interp, args, 0);
    ns_path = resolve_ns_path(ops, interp, ns_path);
  }

  TclObj children = ops->ns.children(interp, ns_path);
  ops->interp.set_result(interp, children);
  return TCL_OK;
}

// namespace parent ?ns?
static TclResult ns_parent(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace parent ?name?\"", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj ns_path;
  if (argc == 0) {
    ns_path = ops->ns.current(interp);
  } else {
    ns_path = ops->list.at(interp, args, 0);
    ns_path = resolve_ns_path(ops, interp, ns_path);
  }

  // Check if namespace exists
  if (!ops->ns.exists(interp, ns_path)) {
    size_t path_len;
    const char *path_str = ops->string.get(interp, ns_path, &path_len);

    // If relative and doesn't exist, try original
    TclObj original = (argc == 0) ? ns_path : ops->list.at(interp, args, 0);
    size_t orig_len;
    const char *orig_str = ops->string.get(interp, original, &orig_len);

    TclObj msg = ops->string.intern(interp, "namespace \"", 11);
    msg = ops->string.concat(interp, msg, original);
    TclObj suffix = ops->string.intern(interp, "\" not found", 11);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj parent;
  TclResult res = ops->ns.parent(interp, ns_path, &parent);
  if (res != TCL_OK) {
    return res;
  }

  ops->interp.set_result(interp, parent);
  return TCL_OK;
}

// namespace delete ns ?ns ...?
static TclResult ns_delete(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    TclObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace delete ?name name ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  for (size_t i = 0; i < argc; i++) {
    TclObj ns_path = ops->list.at(interp, args, i);
    TclObj abs_path = resolve_ns_path(ops, interp, ns_path);

    // Check for deleting global namespace
    size_t len;
    const char *str = ops->string.get(interp, abs_path, &len);
    if (len == 2 && str[0] == ':' && str[1] == ':') {
      TclObj msg = ops->string.intern(interp, "cannot delete namespace \"::\"", 28);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Check if namespace exists
    if (!ops->ns.exists(interp, abs_path)) {
      TclObj msg = ops->string.intern(interp, "namespace \"", 11);
      msg = ops->string.concat(interp, msg, ns_path);
      TclObj suffix = ops->string.intern(interp, "\" not found", 11);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    TclResult res = ops->ns.delete(interp, abs_path);
    if (res != TCL_OK) {
      return res;
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

TclResult tcl_builtin_namespace(const TclHostOps *ops, TclInterp interp,
                                 TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"namespace subcommand ?arg ...?\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj subcmd = ops->list.shift(interp, args);
  size_t subcmd_len;
  const char *subcmd_str = ops->string.get(interp, subcmd, &subcmd_len);

  if (str_eq(subcmd_str, subcmd_len, "current")) {
    return ns_current(ops, interp, args);
  } else if (str_eq(subcmd_str, subcmd_len, "eval")) {
    return ns_eval(ops, interp, args);
  } else if (str_eq(subcmd_str, subcmd_len, "exists")) {
    return ns_exists(ops, interp, args);
  } else if (str_eq(subcmd_str, subcmd_len, "children")) {
    return ns_children(ops, interp, args);
  } else if (str_eq(subcmd_str, subcmd_len, "parent")) {
    return ns_parent(ops, interp, args);
  } else if (str_eq(subcmd_str, subcmd_len, "delete")) {
    return ns_delete(ops, interp, args);
  } else {
    TclObj msg = ops->string.intern(interp,
      "bad option \"", 12);
    msg = ops->string.concat(interp, msg, subcmd);
    TclObj suffix = ops->string.intern(interp,
      "\": must be children, current, delete, eval, exists, or parent", 61);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
