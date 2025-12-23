#include "internal.h"
#include "tclc.h"

/**
 * Helper to check if a string equals a literal.
 */
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t lit_len = tcl_strlen(lit);
  if (len != lit_len) {
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) {
      return 0;
    }
  }
  return 1;
}

/**
 * trace add variable name ops script
 * trace add command name ops script
 */
static TclResult trace_add(const TclHostOps *ops, TclInterp interp,
                           TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 4) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace add type name ops command\"",
                           57));
    return TCL_ERROR;
  }

  TclObj kind = ops->list.at(interp, args, 0);
  TclObj name = ops->list.at(interp, args, 1);
  TclObj opsArg = ops->list.at(interp, args, 2);
  TclObj script = ops->list.at(interp, args, 3);

  // Validate kind
  size_t kindLen;
  const char *kindStr = ops->string.get(interp, kind, &kindLen);
  if (!str_eq(kindStr, kindLen, "variable") && !str_eq(kindStr, kindLen, "command")) {
    TclObj msg = ops->string.intern(interp, "bad type \"", 10);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be command or variable", 30));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Validate ops - it should be a list of valid operations
  TclObj opsList = ops->list.from(interp, opsArg);
  size_t opsCount = ops->list.length(interp, opsList);
  if (opsCount == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "bad operation list \"\": must be one or more of read, write, or unset", 67));
    return TCL_ERROR;
  }

  // Build space-separated ops string for storage
  TclObj opsString = ops->list.at(interp, opsList, 0);
  for (size_t i = 1; i < opsCount; i++) {
    opsString = ops->string.concat(interp, opsString, ops->string.intern(interp, " ", 1));
    opsString = ops->string.concat(interp, opsString, ops->list.at(interp, opsList, i));
  }

  // For command traces, normalize the name to fully qualified form
  TclObj traceName = name;
  if (str_eq(kindStr, kindLen, "command")) {
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);
    // If unqualified, prepend ::
    if (!tcl_is_qualified(nameStr, nameLen)) {
      traceName = ops->string.intern(interp, "::", 2);
      traceName = ops->string.concat(interp, traceName, name);
    }
  }

  // Register the trace
  if (ops->trace.add(interp, kind, traceName, opsString, script) != TCL_OK) {
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

/**
 * trace remove variable name ops script
 * trace remove command name ops script
 */
static TclResult trace_remove(const TclHostOps *ops, TclInterp interp,
                              TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 4) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace remove type name opList command\"",
                           63));
    return TCL_ERROR;
  }

  TclObj kind = ops->list.at(interp, args, 0);
  TclObj name = ops->list.at(interp, args, 1);
  TclObj opsArg = ops->list.at(interp, args, 2);
  TclObj script = ops->list.at(interp, args, 3);

  // Validate kind
  size_t kindLen;
  const char *kindStr = ops->string.get(interp, kind, &kindLen);
  if (!str_eq(kindStr, kindLen, "variable") && !str_eq(kindStr, kindLen, "command")) {
    TclObj msg = ops->string.intern(interp, "bad type \"", 10);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be command or variable", 30));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build space-separated ops string
  TclObj opsList = ops->list.from(interp, opsArg);
  size_t opsCount = ops->list.length(interp, opsList);
  TclObj opsString = ops->string.intern(interp, "", 0);
  if (opsCount > 0) {
    opsString = ops->list.at(interp, opsList, 0);
    for (size_t i = 1; i < opsCount; i++) {
      opsString = ops->string.concat(interp, opsString, ops->string.intern(interp, " ", 1));
      opsString = ops->string.concat(interp, opsString, ops->list.at(interp, opsList, i));
    }
  }

  // For command traces, normalize the name to fully qualified form
  TclObj traceName = name;
  if (str_eq(kindStr, kindLen, "command")) {
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);
    if (!tcl_is_qualified(nameStr, nameLen)) {
      traceName = ops->string.intern(interp, "::", 2);
      traceName = ops->string.concat(interp, traceName, name);
    }
  }

  // Remove the trace - note: trace.remove returns error if not found
  // but TCL trace remove is silent if trace doesn't exist
  ops->trace.remove(interp, kind, traceName, opsString, script);

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

/**
 * trace info variable name
 * trace info command name
 */
static TclResult trace_info(const TclHostOps *ops, TclInterp interp,
                            TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 2) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace info type name\"",
                           46));
    return TCL_ERROR;
  }

  TclObj kind = ops->list.at(interp, args, 0);
  TclObj name = ops->list.at(interp, args, 1);

  // Validate kind
  size_t kindLen;
  const char *kindStr = ops->string.get(interp, kind, &kindLen);
  if (!str_eq(kindStr, kindLen, "variable") && !str_eq(kindStr, kindLen, "command")) {
    TclObj msg = ops->string.intern(interp, "bad type \"", 10);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be command or variable", 30));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // For command traces, normalize the name to fully qualified form
  TclObj traceName = name;
  if (str_eq(kindStr, kindLen, "command")) {
    size_t nameLen;
    const char *nameStr = ops->string.get(interp, name, &nameLen);
    if (!tcl_is_qualified(nameStr, nameLen)) {
      traceName = ops->string.intern(interp, "::", 2);
      traceName = ops->string.concat(interp, traceName, name);
    }
  }

  // Get trace info
  TclObj info = ops->trace.info(interp, kind, traceName);
  ops->interp.set_result(interp, info);
  return TCL_OK;
}

TclResult tcl_builtin_trace(const TclHostOps *ops, TclInterp interp,
                            TclObj cmd, TclObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace subcommand ?arg ...?\"",
                           52));
    return TCL_ERROR;
  }

  // Get subcommand
  TclObj subcmd = ops->list.shift(interp, args);
  size_t subcmdLen;
  const char *subcmdStr = ops->string.get(interp, subcmd, &subcmdLen);

  if (str_eq(subcmdStr, subcmdLen, "add")) {
    return trace_add(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "remove")) {
    return trace_remove(ops, interp, args);
  }
  if (str_eq(subcmdStr, subcmdLen, "info")) {
    return trace_info(ops, interp, args);
  }

  // Unknown subcommand
  TclObj msg = ops->string.intern(
      interp,
      "unknown or ambiguous subcommand \"", 33);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(
      interp, msg,
      ops->string.intern(interp, "\": must be add, info, or remove", 31));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
