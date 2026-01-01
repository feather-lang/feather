#include "internal.h"
#include "feather.h"

// Global flag to prevent recursive trace firing
static int trace_firing = 0;

/**
 * Helper: check if operation matches the trace ops string.
 * The ops string is space-separated (e.g., "read write").
 * Returns 1 if op is in the ops string, 0 otherwise.
 */
static int ops_contains(const FeatherHostOps *ops, FeatherInterp interp,
                        FeatherObj opsString, const char *op) {
  FeatherObj opsList = ops->list.from(interp, opsString);
  size_t count = ops->list.length(interp, opsList);
  FeatherObj opObj = ops->string.intern(interp, op, feather_strlen(op));
  for (size_t i = 0; i < count; i++) {
    if (ops->string.equal(interp, ops->list.at(interp, opsList, i), opObj)) {
      return 1;
    }
  }
  return 0;
}

/**
 * feather_fire_var_traces fires variable traces for the given operation.
 *
 * varName: the variable name (may be qualified or unqualified)
 * op: "read", "write", or "unset"
 *
 * Traces fire in FIFO order (first added, first fired).
 * The trace callback receives: script varName {} op
 */
void feather_fire_var_traces(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj varName, const char *op) {
  if (trace_firing) return;
  trace_firing = 1;

  FeatherObj traceDict = feather_trace_get_dict(ops, interp, "variable");
  FeatherObj traces = ops->dict.get(interp, traceDict, varName);

  if (!ops->list.is_nil(interp, traces)) {
    size_t count = ops->list.length(interp, traces);
    FeatherObj opObj = ops->string.intern(interp, op, feather_strlen(op));
    FeatherObj emptyObj = ops->string.intern(interp, "", 0);

    // Fire in LIFO order (most recently added first)
    for (size_t i = count; i > 0; i--) {
      FeatherObj entry = ops->list.at(interp, traces, i - 1);
      FeatherObj entryOps = ops->list.at(interp, entry, 0);
      FeatherObj script = ops->list.at(interp, entry, 1);

      // Check if this trace matches the operation
      if (!ops_contains(ops, interp, entryOps, op)) {
        continue;
      }

      // Build command as list: {script varName {} op}
      FeatherObj cmd = ops->list.from(interp, script);
      cmd = ops->list.push(interp, cmd, varName);
      cmd = ops->list.push(interp, cmd, emptyObj);  // name2 is always empty
      cmd = ops->list.push(interp, cmd, opObj);

      // Execute the trace command
      feather_script_eval_obj(ops, interp, cmd, 0);
    }
  }

  trace_firing = 0;
}

/**
 * feather_fire_cmd_traces fires command traces for the given operation.
 *
 * oldName: the original command name (fully qualified)
 * newName: the new command name (empty for delete)
 * op: "rename" or "delete"
 *
 * Traces fire in FIFO order (first added, first fired).
 * The trace callback receives: script oldName newName op
 */
void feather_fire_cmd_traces(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj oldName, FeatherObj newName, const char *op) {
  if (trace_firing) return;
  trace_firing = 1;

  FeatherObj traceDict = feather_trace_get_dict(ops, interp, "command");
  FeatherObj traces = ops->dict.get(interp, traceDict, oldName);

  if (!ops->list.is_nil(interp, traces)) {
    size_t count = ops->list.length(interp, traces);
    FeatherObj opObj = ops->string.intern(interp, op, feather_strlen(op));

    // Fire in FIFO order
    for (size_t i = 0; i < count; i++) {
      FeatherObj entry = ops->list.at(interp, traces, i);
      FeatherObj entryOps = ops->list.at(interp, entry, 0);
      FeatherObj script = ops->list.at(interp, entry, 1);

      // Check if this trace matches the operation
      if (!ops_contains(ops, interp, entryOps, op)) {
        continue;
      }

      // Build command as list: {script oldName newName op}
      // TCL passes fully qualified names including :: prefix
      FeatherObj cmd = ops->list.from(interp, script);
      cmd = ops->list.push(interp, cmd, oldName);
      cmd = ops->list.push(interp, cmd, newName);
      cmd = ops->list.push(interp, cmd, opObj);

      // Execute the trace command
      feather_script_eval_obj(ops, interp, cmd, 0);
    }
  }

  trace_firing = 0;
}

/**
 * feather_fire_exec_traces fires execution traces for enter or leave.
 *
 * cmdName: the command name (fully qualified, for lookup)
 * cmdList: the full command as a list [cmdname, arg1, arg2, ...]
 * op: "enter" or "leave"
 * code: return code (only for leave, 0 for enter)
 * result: command result (only for leave, 0 for enter)
 *
 * Traces fire in LIFO order (last added, first fired).
 * For enter: script receives {cmdList} enter
 * For leave: script receives {cmdList} code result leave
 */
void feather_fire_exec_traces(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmdName, FeatherObj cmdList,
                              const char *op, int code, FeatherObj result) {
  if (trace_firing) return;
  trace_firing = 1;

  FeatherObj traceDict = feather_trace_get_dict(ops, interp, "execution");
  FeatherObj traces = ops->dict.get(interp, traceDict, cmdName);

  if (!ops->list.is_nil(interp, traces)) {
    size_t count = ops->list.length(interp, traces);
    FeatherObj opObj = ops->string.intern(interp, op, feather_strlen(op));

    // Fire in LIFO order (iterate backwards)
    for (size_t i = count; i > 0; i--) {
      FeatherObj entry = ops->list.at(interp, traces, i - 1);
      FeatherObj entryOps = ops->list.at(interp, entry, 0);
      FeatherObj script = ops->list.at(interp, entry, 1);

      // Check if this trace matches the operation
      if (!ops_contains(ops, interp, entryOps, op)) {
        continue;
      }

      // Build command as list
      FeatherObj cmd = ops->list.from(interp, script);
      cmd = ops->list.push(interp, cmd, cmdList);

      if (feather_str_eq(op, feather_strlen(op), "leave")) {
        // For leave: add code and result
        // Convert code to string
        char codeBuf[16];
        int codeLen = 0;
        int n = code;
        if (n < 0) {
          codeBuf[codeLen++] = '-';
          n = -n;
        }
        if (n == 0) {
          codeBuf[codeLen++] = '0';
        } else {
          char temp[16];
          int tempLen = 0;
          while (n > 0) {
            temp[tempLen++] = '0' + (n % 10);
            n /= 10;
          }
          for (int j = tempLen - 1; j >= 0; j--) {
            codeBuf[codeLen++] = temp[j];
          }
        }
        FeatherObj codeObj = ops->string.intern(interp, codeBuf, codeLen);
        cmd = ops->list.push(interp, cmd, codeObj);
        cmd = ops->list.push(interp, cmd, result);
      }

      cmd = ops->list.push(interp, cmd, opObj);

      // Execute the trace command
      feather_script_eval_obj(ops, interp, cmd, 0);
    }
  }

  trace_firing = 0;
}
