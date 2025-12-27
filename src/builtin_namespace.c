#include "feather.h"
#include "internal.h"

// Resolve a namespace path (relative or absolute) to an absolute path
static FeatherObj resolve_ns_path(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj path) {
  size_t len;
  const char *str = ops->string.get(interp, path, &len);

  // If absolute (starts with ::), return as-is
  if (len >= 2 && str[0] == ':' && str[1] == ':') {
    return path;
  }

  // Relative - prepend current namespace
  FeatherObj current = ops->ns.current(interp);
  size_t current_len;
  const char *current_str = ops->string.get(interp, current, &current_len);

  // If current is global "::", prepend "::" to path
  if (current_len == 2 && current_str[0] == ':' && current_str[1] == ':') {
    FeatherObj prefix = ops->string.intern(interp, "::", 2);
    return ops->string.concat(interp, prefix, path);
  }

  // Otherwise append "::" and path to current
  FeatherObj sep = ops->string.intern(interp, "::", 2);
  FeatherObj result = ops->string.concat(interp, current, sep);
  return ops->string.concat(interp, result, path);
}

// namespace current
static FeatherResult ns_current(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 0) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace current\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj current = ops->ns.current(interp);
  ops->interp.set_result(interp, current);
  return TCL_OK;
}

// namespace eval ns script
static FeatherResult ns_eval(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace eval name arg ?arg ...?\"", 59);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj ns_path = ops->list.at(interp, args, 0);
  FeatherObj abs_path = resolve_ns_path(ops, interp, ns_path);

  // Create namespace if it doesn't exist
  ops->ns.create(interp, abs_path);

  // Save current namespace
  FeatherObj saved_ns = ops->frame.get_namespace(interp);

  // Set current frame's namespace
  ops->frame.set_namespace(interp, abs_path);

  // Get the script (concatenate remaining args if multiple)
  FeatherObj script;
  if (argc == 2) {
    script = ops->list.at(interp, args, 1);
  } else {
    // Concatenate remaining arguments with spaces
    script = ops->list.at(interp, args, 1);
    for (size_t i = 2; i < argc; i++) {
      FeatherObj space = ops->string.intern(interp, " ", 1);
      script = ops->string.concat(interp, script, space);
      script = ops->string.concat(interp, script, ops->list.at(interp, args, i));
    }
  }

  // Evaluate the script
  FeatherResult result = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

  // Restore namespace
  ops->frame.set_namespace(interp, saved_ns);

  return result;
}

// namespace exists ns
static FeatherResult ns_exists(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace exists name\"", 47);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj ns_path = ops->list.at(interp, args, 0);
  FeatherObj abs_path = resolve_ns_path(ops, interp, ns_path);

  int exists = ops->ns.exists(interp, abs_path);
  FeatherObj result = ops->integer.create(interp, exists ? 1 : 0);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// namespace children ?ns?
static FeatherResult ns_children(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace children ?name?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj ns_path;
  if (argc == 0) {
    ns_path = ops->ns.current(interp);
  } else {
    ns_path = ops->list.at(interp, args, 0);
    ns_path = resolve_ns_path(ops, interp, ns_path);
  }

  FeatherObj children = ops->ns.children(interp, ns_path);
  ops->interp.set_result(interp, children);
  return TCL_OK;
}

// namespace parent ?ns?
static FeatherResult ns_parent(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace parent ?name?\"", 49);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj ns_path;
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
    FeatherObj original = (argc == 0) ? ns_path : ops->list.at(interp, args, 0);
    size_t orig_len;
    const char *orig_str = ops->string.get(interp, original, &orig_len);

    FeatherObj msg = ops->string.intern(interp, "namespace \"", 11);
    msg = ops->string.concat(interp, msg, original);
    FeatherObj suffix = ops->string.intern(interp, "\" not found", 11);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj parent;
  FeatherResult res = ops->ns.parent(interp, ns_path, &parent);
  if (res != TCL_OK) {
    return res;
  }

  ops->interp.set_result(interp, parent);
  return TCL_OK;
}

// namespace delete ns ?ns ...?
static FeatherResult ns_delete(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace delete ?name name ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  for (size_t i = 0; i < argc; i++) {
    FeatherObj ns_path = ops->list.at(interp, args, i);
    FeatherObj abs_path = resolve_ns_path(ops, interp, ns_path);

    // Check for deleting global namespace
    size_t len;
    const char *str = ops->string.get(interp, abs_path, &len);
    if (len == 2 && str[0] == ':' && str[1] == ':') {
      FeatherObj msg = ops->string.intern(interp, "cannot delete namespace \"::\"", 28);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Check if namespace exists
    if (!ops->ns.exists(interp, abs_path)) {
      FeatherObj msg = ops->string.intern(interp, "namespace \"", 11);
      msg = ops->string.concat(interp, msg, ns_path);
      FeatherObj suffix = ops->string.intern(interp, "\" not found", 11);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    FeatherResult res = ops->ns.delete(interp, abs_path);
    if (res != TCL_OK) {
      return res;
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

// namespace export ?-clear? ?pattern pattern ...?
static FeatherResult ns_export(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  FeatherObj current = ops->ns.current(interp);

  // No args - return current export patterns
  if (argc == 0) {
    FeatherObj exports = ops->ns.get_exports(interp, current);
    // Convert list to space-separated string
    size_t len = ops->list.length(interp, exports);
    if (len == 0) {
      ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      return TCL_OK;
    }
    FeatherObj result = ops->list.at(interp, exports, 0);
    for (size_t i = 1; i < len; i++) {
      result = ops->string.concat(interp, result, ops->string.intern(interp, " ", 1));
      result = ops->string.concat(interp, result, ops->list.at(interp, exports, i));
    }
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Check for -clear flag
  int clear = 0;
  FeatherObj first = ops->list.at(interp, args, 0);
  size_t first_len;
  const char *first_str = ops->string.get(interp, first, &first_len);
  if (first_len == 6 && first_str[0] == '-' && first_str[1] == 'c' &&
      first_str[2] == 'l' && first_str[3] == 'e' && first_str[4] == 'a' &&
      first_str[5] == 'r') {
    clear = 1;
    ops->list.shift(interp, args); // consume -clear
    argc--;
  }

  // Build pattern list from remaining args
  FeatherObj patterns = ops->list.create(interp);
  for (size_t i = 0; i < argc; i++) {
    patterns = ops->list.push(interp, patterns, ops->list.at(interp, args, i));
  }

  // Set exports
  ops->ns.set_exports(interp, current, patterns, clear);
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

// namespace qualifiers string
static FeatherResult ns_qualifiers(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace qualifiers string\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.at(interp, args, 0);
  size_t len;
  const char *s = ops->string.get(interp, str, &len);

  // Find last :: separator
  size_t last_sep = 0;
  int found = 0;
  for (size_t i = 0; i + 1 < len; i++) {
    if (s[i] == ':' && s[i + 1] == ':') {
      last_sep = i;
      found = 1;
    }
  }

  if (!found) {
    // No :: in string - return empty string
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Return everything before last ::
  FeatherObj result = ops->string.intern(interp, s, last_sep);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// namespace tail string
static FeatherResult ns_tail(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace tail string\"", 47);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj str = ops->list.at(interp, args, 0);
  size_t len;
  const char *s = ops->string.get(interp, str, &len);

  // Find last :: separator
  size_t last_sep = 0;
  int found = 0;
  for (size_t i = 0; i + 1 < len; i++) {
    if (s[i] == ':' && s[i + 1] == ':') {
      last_sep = i;
      found = 1;
    }
  }

  if (!found) {
    // No :: in string - return the whole string
    ops->interp.set_result(interp, str);
    return TCL_OK;
  }

  // Return everything after last ::
  size_t start = last_sep + 2;
  FeatherObj result = ops->string.intern(interp, s + start, len - start);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// namespace import ?-force? ?pattern pattern ...?
static FeatherResult ns_import(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"namespace import ?-force? ?pattern pattern ...?\"", 73);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Check for -force flag
  int force = 0;
  FeatherObj first = ops->list.at(interp, args, 0);
  size_t first_len;
  const char *first_str = ops->string.get(interp, first, &first_len);
  if (first_len == 6 && first_str[0] == '-' && first_str[1] == 'f' &&
      first_str[2] == 'o' && first_str[3] == 'r' && first_str[4] == 'c' &&
      first_str[5] == 'e') {
    force = 1;
    ops->list.shift(interp, args);
    argc--;
  }

  FeatherObj current = ops->ns.current(interp);

  // Process each pattern
  for (size_t i = 0; i < argc; i++) {
    FeatherObj pattern = ops->list.at(interp, args, i);
    size_t pat_len;
    const char *pat_str = ops->string.get(interp, pattern, &pat_len);

    // Pattern is something like "math::double" or "math::*"
    // Split into namespace and command pattern
    // Find last ::
    size_t last_sep = 0;
    int found = 0;
    for (size_t j = 0; j + 1 < pat_len; j++) {
      if (pat_str[j] == ':' && pat_str[j + 1] == ':') {
        last_sep = j;
        found = 1;
      }
    }

    if (!found) {
      // No :: in pattern - error
      FeatherObj msg = ops->string.intern(interp, "unknown or unexported command \"", 31);
      msg = ops->string.concat(interp, msg, pattern);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Extract namespace path and command pattern
    FeatherObj srcNs = ops->string.intern(interp, pat_str, last_sep);
    if (last_sep == 0 && pat_len >= 2 && pat_str[0] == ':' && pat_str[1] == ':') {
      // Pattern like "::cmd" means global namespace
      srcNs = ops->string.intern(interp, "::", 2);
    }
    // Resolve relative namespace
    srcNs = resolve_ns_path(ops, interp, srcNs);

    // Check if source namespace exists
    if (!ops->ns.exists(interp, srcNs)) {
      // Extract just the namespace name for the error message
      size_t ns_len;
      const char *ns_str = ops->string.get(interp, srcNs, &ns_len);
      // Remove leading :: for relative display
      FeatherObj displayNs = srcNs;
      if (ns_len > 2 && ns_str[0] == ':' && ns_str[1] == ':') {
        displayNs = ops->string.intern(interp, ns_str + 2, ns_len - 2);
      }
      FeatherObj msg = ops->string.intern(interp, "namespace \"", 11);
      msg = ops->string.concat(interp, msg, displayNs);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\" not found", 11));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    FeatherObj cmdPattern = ops->string.intern(interp, pat_str + last_sep + 2, pat_len - last_sep - 2);
    size_t cmd_pat_len;
    const char *cmd_pat_str = ops->string.get(interp, cmdPattern, &cmd_pat_len);

    // Get list of commands in source namespace
    FeatherObj srcCmds = ops->ns.list_commands(interp, srcNs);
    size_t numCmds = ops->list.length(interp, srcCmds);

    // Check if pattern contains wildcard
    int has_wildcard = 0;
    for (size_t j = 0; j < cmd_pat_len; j++) {
      if (cmd_pat_str[j] == '*' || cmd_pat_str[j] == '?') {
        has_wildcard = 1;
        break;
      }
    }

    int matched = 0;
    for (size_t j = 0; j < numCmds; j++) {
      FeatherObj cmdName = ops->list.at(interp, srcCmds, j);
      size_t cmd_len;
      const char *cmd_str = ops->string.get(interp, cmdName, &cmd_len);

      // Check if command matches pattern
      int matches = 0;
      if (has_wildcard) {
        matches = feather_glob_match(cmd_pat_str, cmd_pat_len, cmd_str, cmd_len);
      } else {
        matches = (cmd_len == cmd_pat_len);
        for (size_t k = 0; k < cmd_len && matches; k++) {
          if (cmd_str[k] != cmd_pat_str[k]) matches = 0;
        }
      }

      if (!matches) continue;

      // Check if command is exported
      if (!ops->ns.is_exported(interp, srcNs, cmdName)) {
        if (!has_wildcard) {
          // Specific command not exported - error
          FeatherObj msg = ops->string.intern(interp, "unknown or unexported command \"", 31);
          msg = ops->string.concat(interp, msg, pattern);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        continue; // Skip unexported when using wildcard
      }

      matched = 1;

      // Check if command already exists in current namespace
      FeatherBuiltinCmd unusedFn = NULL;
      FeatherCommandType existingType = ops->ns.get_command(interp, current, cmdName, &unusedFn);
      if (existingType != TCL_CMD_NONE && !force) {
        FeatherObj msg = ops->string.intern(interp, "can't import command \"", 22);
        msg = ops->string.concat(interp, msg, cmdName);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\": already exists", 17));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Copy command from source to current namespace
      ops->ns.copy_command(interp, srcNs, cmdName, current, cmdName);
    }

    // If no wildcard and no match, error
    if (!has_wildcard && !matched) {
      FeatherObj msg = ops->string.intern(interp, "unknown or unexported command \"", 31);
      msg = ops->string.concat(interp, msg, pattern);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

FeatherResult feather_builtin_namespace(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"namespace subcommand ?arg ...?\"", 56);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj subcmd = ops->list.shift(interp, args);
  size_t subcmd_len;
  const char *subcmd_str = ops->string.get(interp, subcmd, &subcmd_len);

  if (feather_str_eq(subcmd_str, subcmd_len, "current")) {
    return ns_current(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "eval")) {
    return ns_eval(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "exists")) {
    return ns_exists(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "children")) {
    return ns_children(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "parent")) {
    return ns_parent(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "delete")) {
    return ns_delete(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "export")) {
    return ns_export(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "import")) {
    return ns_import(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "qualifiers")) {
    return ns_qualifiers(ops, interp, args);
  } else if (feather_str_eq(subcmd_str, subcmd_len, "tail")) {
    return ns_tail(ops, interp, args);
  } else {
    FeatherObj msg = ops->string.intern(interp,
      "bad option \"", 12);
    msg = ops->string.concat(interp, msg, subcmd);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be children, current, delete, eval, exists, export, import, parent, qualifiers, or tail", 95);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
