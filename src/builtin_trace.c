#include "internal.h"
#include "feather.h"

/**
 * trace add variable name ops script
 * trace add command name ops script
 * trace add execution name ops script
 */
static FeatherResult trace_add(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 4) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace add type name ops command\"",
                           57));
    return TCL_ERROR;
  }

  FeatherObj kind = ops->list.at(interp, args, 0);
  FeatherObj name = ops->list.at(interp, args, 1);
  FeatherObj opsArg = ops->list.at(interp, args, 2);
  FeatherObj script = ops->list.at(interp, args, 3);

  // Validate kind
  if (!feather_obj_eq_literal(ops, interp, kind, "variable") && 
      !feather_obj_eq_literal(ops, interp, kind, "command") &&
      !feather_obj_eq_literal(ops, interp, kind, "execution")) {
    FeatherObj msg = ops->string.intern(interp, "bad type \"", 10);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be command, execution, or variable", 42));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Validate ops - it should be a list of valid operations
  FeatherObj opsList = ops->list.from(interp, opsArg);
  size_t opsCount = ops->list.length(interp, opsList);
  if (opsCount == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "bad operation list \"\": must be one or more of read, write, or unset", 67));
    return TCL_ERROR;
  }

  // Build space-separated ops string for storage
  FeatherObj opsString = ops->list.at(interp, opsList, 0);
  for (size_t i = 1; i < opsCount; i++) {
    opsString = ops->string.concat(interp, opsString, ops->string.intern(interp, " ", 1));
    opsString = ops->string.concat(interp, opsString, ops->list.at(interp, opsList, i));
  }

  // For command and execution traces, normalize the name to fully qualified form
  FeatherObj traceName = name;
  if (feather_obj_eq_literal(ops, interp, kind, "command") || feather_obj_eq_literal(ops, interp, kind, "execution")) {
    // If unqualified, prepend ::
    if (!feather_obj_is_qualified(ops, interp, name)) {
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
 * trace remove execution name ops script
 */
static FeatherResult trace_remove(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 4) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace remove type name opList command\"",
                           63));
    return TCL_ERROR;
  }

  FeatherObj kind = ops->list.at(interp, args, 0);
  FeatherObj name = ops->list.at(interp, args, 1);
  FeatherObj opsArg = ops->list.at(interp, args, 2);
  FeatherObj script = ops->list.at(interp, args, 3);

  // Validate kind
  if (!feather_obj_eq_literal(ops, interp, kind, "variable") && 
      !feather_obj_eq_literal(ops, interp, kind, "command") &&
      !feather_obj_eq_literal(ops, interp, kind, "execution")) {
    FeatherObj msg = ops->string.intern(interp, "bad type \"", 10);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be command, execution, or variable", 42));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Build space-separated ops string
  FeatherObj opsList = ops->list.from(interp, opsArg);
  size_t opsCount = ops->list.length(interp, opsList);
  FeatherObj opsString = ops->string.intern(interp, "", 0);
  if (opsCount > 0) {
    opsString = ops->list.at(interp, opsList, 0);
    for (size_t i = 1; i < opsCount; i++) {
      opsString = ops->string.concat(interp, opsString, ops->string.intern(interp, " ", 1));
      opsString = ops->string.concat(interp, opsString, ops->list.at(interp, opsList, i));
    }
  }

  // For command and execution traces, normalize the name to fully qualified form
  FeatherObj traceName = name;
  if (feather_obj_eq_literal(ops, interp, kind, "command") || feather_obj_eq_literal(ops, interp, kind, "execution")) {
    if (!feather_obj_is_qualified(ops, interp, name)) {
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
 * trace info execution name
 */
static FeatherResult trace_info(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 2) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace info type name\"",
                           46));
    return TCL_ERROR;
  }

  FeatherObj kind = ops->list.at(interp, args, 0);
  FeatherObj name = ops->list.at(interp, args, 1);

  // Validate kind
  if (!feather_obj_eq_literal(ops, interp, kind, "variable") && 
      !feather_obj_eq_literal(ops, interp, kind, "command") &&
      !feather_obj_eq_literal(ops, interp, kind, "execution")) {
    FeatherObj msg = ops->string.intern(interp, "bad type \"", 10);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be command, execution, or variable", 42));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // For command and execution traces, normalize the name to fully qualified form
  FeatherObj traceName = name;
  if (feather_obj_eq_literal(ops, interp, kind, "command") || feather_obj_eq_literal(ops, interp, kind, "execution")) {
    if (!feather_obj_is_qualified(ops, interp, name)) {
      traceName = ops->string.intern(interp, "::", 2);
      traceName = ops->string.concat(interp, traceName, name);
    }
  }

  // Get trace info
  FeatherObj info = ops->trace.info(interp, kind, traceName);
  ops->interp.set_result(interp, info);
  return TCL_OK;
}

FeatherResult feather_builtin_trace(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
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
  FeatherObj subcmd = ops->list.shift(interp, args);

  if (feather_obj_eq_literal(ops, interp, subcmd, "add")) {
    return trace_add(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "remove")) {
    return trace_remove(ops, interp, args);
  }
  if (feather_obj_eq_literal(ops, interp, subcmd, "info")) {
    return trace_info(ops, interp, args);
  }

  // Unknown subcommand
  FeatherObj msg = ops->string.intern(
      interp,
      "unknown or ambiguous subcommand \"", 33);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(
      interp, msg,
      ops->string.intern(interp, "\": must be add, info, or remove", 31));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
