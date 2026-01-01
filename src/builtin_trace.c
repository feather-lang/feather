#include "internal.h"
#include "feather.h"

/**
 * Helper: get kind as a C string for feather_trace_get_dict.
 * Returns NULL if kind is invalid.
 */
static const char *get_kind_string(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj kind) {
  if (feather_obj_eq_literal(ops, interp, kind, "variable")) {
    return "variable";
  }
  if (feather_obj_eq_literal(ops, interp, kind, "command")) {
    return "command";
  }
  if (feather_obj_eq_literal(ops, interp, kind, "execution")) {
    return "execution";
  }
  return NULL;
}

/**
 * trace add variable name ops script
 * trace add command name ops script
 * trace add execution name ops script
 */
static FeatherResult trace_add(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  // Need at least the type argument
  if (argc == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace add type name opList command\"",
                           60));
    return TCL_ERROR;
  }

  FeatherObj kind = ops->list.at(interp, args, 0);

  // Validate kind first
  const char *kindStr = get_kind_string(ops, interp, kind);
  if (kindStr == NULL) {
    FeatherObj msg = ops->string.intern(interp, "bad option \"", 12);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be execution, command, or variable", 42));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Check argument count with type-specific message
  if (argc != 4) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"trace add ", 35);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, " name opList command\"", 21));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj name = ops->list.at(interp, args, 1);
  FeatherObj opsArg = ops->list.at(interp, args, 2);
  FeatherObj script = ops->list.at(interp, args, 3);

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

  // Get the trace dict for this kind
  FeatherObj traceDict = feather_trace_get_dict(ops, interp, kindStr);

  // Get existing traces for this name (or create empty list)
  FeatherObj traces = ops->dict.get(interp, traceDict, traceName);
  if (ops->list.is_nil(interp, traces)) {
    traces = ops->list.create(interp);
  }

  // Create entry as {ops script}
  FeatherObj entry = ops->list.create(interp);
  entry = ops->list.push(interp, entry, opsString);
  entry = ops->list.push(interp, entry, script);

  // Append entry to traces list
  traces = ops->list.push(interp, traces, entry);

  // Store updated traces in dict
  traceDict = ops->dict.set(interp, traceDict, traceName, traces);

  // Store updated dict back to namespace
  feather_trace_set_dict(ops, interp, kindStr, traceDict);

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
  const char *kindStr = get_kind_string(ops, interp, kind);
  if (kindStr == NULL) {
    FeatherObj msg = ops->string.intern(interp, "bad option \"", 12);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be execution, command, or variable", 42));
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

  // Get the trace dict for this kind
  FeatherObj traceDict = feather_trace_get_dict(ops, interp, kindStr);

  // Get existing traces for this name
  FeatherObj traces = ops->dict.get(interp, traceDict, traceName);
  if (ops->list.is_nil(interp, traces)) {
    // No traces for this name - silent no-op
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Find and remove matching entry
  size_t traceCount = ops->list.length(interp, traces);
  FeatherObj newTraces = ops->list.create(interp);
  for (size_t i = 0; i < traceCount; i++) {
    FeatherObj entry = ops->list.at(interp, traces, i);
    FeatherObj entryOps = ops->list.at(interp, entry, 0);
    FeatherObj entryScript = ops->list.at(interp, entry, 1);

    // Compare ops and script strings
    if (ops->string.equal(interp, entryOps, opsString) &&
        ops->string.equal(interp, entryScript, script)) {
      // Skip this entry (remove it)
      continue;
    }
    // Keep this entry
    newTraces = ops->list.push(interp, newTraces, entry);
  }

  // Store updated traces in dict
  traceDict = ops->dict.set(interp, traceDict, traceName, newTraces);

  // Store updated dict back to namespace
  feather_trace_set_dict(ops, interp, kindStr, traceDict);

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
  const char *kindStr = get_kind_string(ops, interp, kind);
  if (kindStr == NULL) {
    FeatherObj msg = ops->string.intern(interp, "bad option \"", 12);
    msg = ops->string.concat(interp, msg, kind);
    msg = ops->string.concat(interp, msg,
                              ops->string.intern(interp, "\": must be execution, command, or variable", 42));
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

  // Get the trace dict for this kind
  FeatherObj traceDict = feather_trace_get_dict(ops, interp, kindStr);

  // Get traces for this name
  FeatherObj traces = ops->dict.get(interp, traceDict, traceName);
  if (ops->list.is_nil(interp, traces)) {
    // No traces - return empty list
    ops->interp.set_result(interp, ops->list.create(interp));
    return TCL_OK;
  }

  // The traces are already stored as a list of {ops script} pairs
  // However, the stored format has ops as a space-separated string,
  // but trace info should return ops as a list. We need to convert.
  size_t traceCount = ops->list.length(interp, traces);
  FeatherObj result = ops->list.create(interp);
  for (size_t i = 0; i < traceCount; i++) {
    FeatherObj entry = ops->list.at(interp, traces, i);
    FeatherObj entryOps = ops->list.at(interp, entry, 0);
    FeatherObj entryScript = ops->list.at(interp, entry, 1);

    // Convert space-separated ops string to list
    FeatherObj opsAsList = ops->list.from(interp, entryOps);

    // Create result entry {opsList script}
    FeatherObj resultEntry = ops->list.create(interp);
    resultEntry = ops->list.push(interp, resultEntry, opsAsList);
    resultEntry = ops->list.push(interp, resultEntry, entryScript);

    result = ops->list.push(interp, result, resultEntry);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

FeatherResult feather_builtin_trace(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp,
                           "wrong # args: should be \"trace option ?arg ...?\"",
                           48));
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

  // Unknown option
  FeatherObj msg = ops->string.intern(
      interp,
      "bad option \"", 12);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(
      interp, msg,
      ops->string.intern(interp, "\": must be add, info, or remove", 31));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
