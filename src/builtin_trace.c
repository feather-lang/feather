#include "internal.h"
#include "feather.h"
#include "namespace_util.h"

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
 * Helper: validate a single operation for variable traces.
 * Valid ops: array, read, unset, write
 * Returns 1 if valid, 0 if invalid.
 */
static int is_valid_variable_op(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj op) {
  return feather_obj_eq_literal(ops, interp, op, "array") ||
         feather_obj_eq_literal(ops, interp, op, "read") ||
         feather_obj_eq_literal(ops, interp, op, "unset") ||
         feather_obj_eq_literal(ops, interp, op, "write");
}

/**
 * Helper: validate a single operation for command traces.
 * Valid ops: delete, rename
 * Returns 1 if valid, 0 if invalid.
 */
static int is_valid_command_op(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj op) {
  return feather_obj_eq_literal(ops, interp, op, "delete") ||
         feather_obj_eq_literal(ops, interp, op, "rename");
}

/**
 * Helper: validate a single operation for execution traces.
 * Valid ops: enter, leave, enterstep, leavestep
 * Returns 1 if valid, 0 if invalid.
 */
static int is_valid_execution_op(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj op) {
  return feather_obj_eq_literal(ops, interp, op, "enter") ||
         feather_obj_eq_literal(ops, interp, op, "leave") ||
         feather_obj_eq_literal(ops, interp, op, "enterstep") ||
         feather_obj_eq_literal(ops, interp, op, "leavestep");
}

/**
 * Helper: validate all operations in the list for a given trace type.
 * Returns the first invalid operation, or 0 (nil) if all are valid.
 */
static FeatherObj validate_ops(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj opsList, const char *kind) {
  size_t count = ops->list.length(interp, opsList);
  for (size_t i = 0; i < count; i++) {
    FeatherObj op = ops->list.at(interp, opsList, i);
    int valid = 0;
    if (feather_str_eq(kind, feather_strlen(kind), "variable")) {
      valid = is_valid_variable_op(ops, interp, op);
    } else if (feather_str_eq(kind, feather_strlen(kind), "command")) {
      valid = is_valid_command_op(ops, interp, op);
    } else if (feather_str_eq(kind, feather_strlen(kind), "execution")) {
      valid = is_valid_execution_op(ops, interp, op);
    }
    if (!valid) {
      return op;
    }
  }
  return 0;  // 0 = nil = all valid
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

  // Validate each operation
  FeatherObj invalidOp = validate_ops(ops, interp, opsList, kindStr);
  if (invalidOp != 0) {
    // Build error message based on trace type
    FeatherObj msg = ops->string.intern(interp, "bad operation \"", 15);
    msg = ops->string.concat(interp, msg, invalidOp);
    if (feather_str_eq(kindStr, feather_strlen(kindStr), "variable")) {
      msg = ops->string.concat(interp, msg,
                                ops->string.intern(interp, "\": must be array, read, unset, or write", 39));
    } else if (feather_str_eq(kindStr, feather_strlen(kindStr), "command")) {
      msg = ops->string.concat(interp, msg,
                                ops->string.intern(interp, "\": must be delete or rename", 27));
    } else {
      msg = ops->string.concat(interp, msg,
                                ops->string.intern(interp, "\": must be enter, leave, enterstep, or leavestep", 48));
    }
    ops->interp.set_result(interp, msg);
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
    // Check that the command exists
    if (feather_lookup_command(ops, interp, traceName, NULL, NULL, NULL) == TCL_CMD_NONE) {
      FeatherObj msg = ops->string.intern(interp, "unknown command \"", 17);
      msg = ops->string.concat(interp, msg, name);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
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
    // Check that the command exists
    if (feather_lookup_command(ops, interp, traceName, NULL, NULL, NULL) == TCL_CMD_NONE) {
      FeatherObj msg = ops->string.intern(interp, "unknown command \"", 17);
      msg = ops->string.concat(interp, msg, name);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
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
    // Check that the command exists
    if (feather_lookup_command(ops, interp, traceName, NULL, NULL, NULL) == TCL_CMD_NONE) {
      FeatherObj msg = ops->string.intern(interp, "unknown command \"", 17);
      msg = ops->string.concat(interp, msg, name);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
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

void feather_register_trace_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Monitor variable accesses, command usages and command executions",
    "This command causes Tcl commands to be executed whenever certain operations "
    "are invoked. Three types of traces are supported: variable traces fire when "
    "variables are read, written, or unset; command traces fire when commands are "
    "renamed or deleted; execution traces fire when commands are executed.\n\n"
    "Note: Feather does not support TCL-style arrays. The 'array' operation for "
    "variable traces is accepted but has no effect, and the array index argument "
    "passed to variable trace callbacks is always an empty string.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: add ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<type>");
  e = feather_usage_help(ops, interp, e, "Must be command, execution, or variable");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<name>");
  e = feather_usage_help(ops, interp, e, "Name of variable or command to trace");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<ops>");
  e = feather_usage_help(ops, interp, e, "List of operations to trace");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<commandPrefix>");
  e = feather_usage_help(ops, interp, e, "Command to execute when trace fires");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "add", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Arrange for commandPrefix to be executed whenever the named variable or "
    "command is accessed in one of the ways specified by ops.\n\n"
    "For 'trace add variable', ops is a list of one or more of: read, write, unset. "
    "The callback receives: name1 name2 op. name1 is the variable name, name2 is "
    "always empty (Feather has no arrays), and op is the operation. Read/write "
    "traces can modify the variable to affect the result. Errors in read/write "
    "traces propagate as \"can't read\" or \"can't set\" errors. Errors in unset "
    "traces are ignored. Traces fire in LIFO order (most recently added first).\n\n"
    "For 'trace add command', ops is a list of one or more of: rename, delete. "
    "The callback receives: oldName newName op. oldName is the current fully-qualified "
    "name, newName is the new name (empty for delete), and op is the operation. "
    "The command must exist. Errors in command traces are ignored.\n\n"
    "For 'trace add execution', ops is a list of one or more of: enter, leave, "
    "enterstep, leavestep. The command must exist. For enter/enterstep, the "
    "callback receives: command-string op. For leave/leavestep, the callback "
    "receives: command-string code result op. Step traces fire for every command "
    "in a procedure and propagate through nested calls. Errors propagate directly.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: remove ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<type>");
  e = feather_usage_help(ops, interp, e, "Must be command, execution, or variable");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<name>");
  e = feather_usage_help(ops, interp, e, "Name of variable or command");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<opList>");
  e = feather_usage_help(ops, interp, e, "List of operations");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<commandPrefix>");
  e = feather_usage_help(ops, interp, e, "Command that was registered");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "remove", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "If there is a trace set on the specified variable or command with the "
    "operations and command given by opList and commandPrefix, then the trace "
    "is removed, so that commandPrefix will never again be invoked. Returns an "
    "empty string.\n\n"
    "For command and execution traces, the command must exist or an error is "
    "thrown. For variable traces, if the variable has no matching trace, the "
    "command silently does nothing.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: info ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<type>");
  e = feather_usage_help(ops, interp, e, "Must be command, execution, or variable");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<name>");
  e = feather_usage_help(ops, interp, e, "Name of variable or command");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "info", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a list containing one element for each trace currently set on the "
    "specified variable or command. Each element of the list is itself a list "
    "containing two elements, which are the opList and commandPrefix associated "
    "with the trace.\n\n"
    "If the variable or command does not have any traces set, then the result "
    "is an empty string. For command and execution traces, the command must "
    "exist or an error is thrown.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Examples ---
  e = feather_usage_example(ops, interp,
    "proc tracer {varname args} {\n"
    "    upvar #0 $varname var\n"
    "    puts \"$varname was updated to be \\\"$var\\\"\"\n"
    "}\n"
    "trace add variable foo write \"tracer foo\"",
    "Print a message when a global variable is updated",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc doMult args {\n"
    "    global foo bar foobar\n"
    "    set foobar [expr {$foo * $bar}]\n"
    "}\n"
    "trace add variable foo write doMult\n"
    "trace add variable bar write doMult",
    "Keep a computed variable in sync with its inputs",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc x {} { y }\n"
    "proc y {} { z }\n"
    "proc z {} { puts hello }\n"
    "proc report args { puts [info level 0] }\n"
    "trace add execution x enterstep report\n"
    "x",
    "Trace all commands executed during a procedure call",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "trace info variable myVar",
    "List all traces on variable myVar",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // --- See Also section ---
  e = feather_usage_section(ops, interp, "See Also",
    "set, unset");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "trace", spec);
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
