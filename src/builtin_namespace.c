#include "feather.h"
#include "internal.h"

// Helper: Get the imports variable name for a namespace
// Stores imports in ::tcl::imports::<ns> as a dict {localName originPath ...}
static FeatherObj get_imports_varname(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj ns) {
  FeatherObj prefix = ops->string.intern(interp, "::tcl::imports::", 16);
  return ops->string.concat(interp, prefix, ns);
}

// Helper: Get the imports dict for a namespace (creates empty dict if none)
static FeatherObj get_imports_dict(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj ns) {
  FeatherObj varname = get_imports_varname(ops, interp, ns);
  FeatherObj tclNs = ops->string.intern(interp, "::tcl", 5);

  // Extract the variable name part after ::tcl::
  size_t varname_len = ops->string.byte_length(interp, varname);
  FeatherObj localName = ops->string.slice(interp, varname, 6, varname_len); // skip "::tcl::"

  FeatherObj dict = ops->ns.get_var(interp, tclNs, localName);
  if (dict == 0) {
    // No imports yet, return empty dict
    dict = ops->dict.create(interp);
  }
  return dict;
}

// Helper: Set the imports dict for a namespace
static void set_imports_dict(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj ns, FeatherObj dict) {
  FeatherObj varname = get_imports_varname(ops, interp, ns);
  FeatherObj tclNs = ops->string.intern(interp, "::tcl", 5);

  // Extract the variable name part after ::tcl::
  size_t varname_len = ops->string.byte_length(interp, varname);
  FeatherObj localName = ops->string.slice(interp, varname, 6, varname_len); // skip "::tcl::"

  ops->ns.set_var(interp, tclNs, localName, dict);
}

// Helper: Record an import (localName -> srcNs::srcName)
static void record_import(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj dstNs,
                          FeatherObj localName, FeatherObj srcNs, FeatherObj srcName) {
  FeatherObj dict = get_imports_dict(ops, interp, dstNs);

  // Build the origin path: srcNs::srcName
  FeatherObj origin;
  if (feather_obj_is_global_ns(ops, interp, srcNs)) {
    origin = ops->string.intern(interp, "::", 2);
    origin = ops->string.concat(interp, origin, srcName);
  } else {
    origin = ops->string.concat(interp, srcNs, ops->string.intern(interp, "::", 2));
    origin = ops->string.concat(interp, origin, srcName);
  }

  dict = ops->dict.set(interp, dict, localName, origin);
  set_imports_dict(ops, interp, dstNs, dict);
}

// Helper: Remove an import record
static void remove_import(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj ns, FeatherObj localName) {
  FeatherObj dict = get_imports_dict(ops, interp, ns);
  dict = ops->dict.remove(interp, dict, localName);
  set_imports_dict(ops, interp, ns, dict);
}

// Resolve a namespace path (relative or absolute) to an absolute path
static FeatherObj resolve_ns_path(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj path) {
  // If absolute (starts with ::), return as-is
  size_t len = ops->string.byte_length(interp, path);
  if (len >= 2 && ops->string.byte_at(interp, path, 0) == ':' &&
      ops->string.byte_at(interp, path, 1) == ':') {
    return path;
  }

  // Relative - prepend current namespace
  FeatherObj current = ops->ns.current(interp);

  // If current is global "::", prepend "::" to path
  if (feather_obj_is_global_ns(ops, interp, current)) {
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

  // Set current frame's namespace and locals
  ops->frame.set_namespace(interp, abs_path);
  ops->frame.push_locals(interp, abs_path);

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

  // Restore locals and namespace
  ops->frame.pop_locals(interp);
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

// namespace children ?ns? ?pattern?
static FeatherResult ns_children(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc > 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace children ?name? ?pattern?\"", 61);
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

  // If pattern provided, filter the children list
  if (argc == 2) {
    FeatherObj pattern = ops->list.at(interp, args, 1);
    FeatherObj filtered = ops->list.create(interp);
    size_t num_children = ops->list.length(interp, children);

    for (size_t i = 0; i < num_children; i++) {
      FeatherObj child = ops->list.at(interp, children, i);
      if (feather_obj_glob_match(ops, interp, pattern, child)) {
        filtered = ops->list.push(interp, filtered, child);
      }
    }
    children = filtered;
  }

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
    // If relative and doesn't exist, report error using original name
    FeatherObj original = (argc == 0) ? ns_path : ops->list.at(interp, args, 0);

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

// namespace delete ?ns ...?
static FeatherResult ns_delete(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  // No args is a no-op (TCL behavior)
  if (argc == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  for (size_t i = 0; i < argc; i++) {
    FeatherObj ns_path = ops->list.at(interp, args, i);
    FeatherObj abs_path = resolve_ns_path(ops, interp, ns_path);

    // Check for deleting global namespace
    if (feather_obj_is_global_ns(ops, interp, abs_path)) {
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
  if (feather_obj_eq_literal(ops, interp, first, "-clear")) {
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

  // Find last :: separator
  long last_sep = feather_obj_find_last_colons(ops, interp, str);

  if (last_sep < 0) {
    // No :: in string - return empty string
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Return everything before last ::
  FeatherObj result = ops->string.slice(interp, str, 0, (size_t)last_sep);
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

  // Find last :: separator
  long last_sep = feather_obj_find_last_colons(ops, interp, str);

  if (last_sep < 0) {
    // No :: in string - return the whole string
    ops->interp.set_result(interp, str);
    return TCL_OK;
  }

  // Return everything after last ::
  size_t start = (size_t)last_sep + 2;
  size_t len = ops->string.byte_length(interp, str);
  FeatherObj result = ops->string.slice(interp, str, start, len);
  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// namespace import ?-force? ?pattern pattern ...?
static FeatherResult ns_import(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  FeatherObj current = ops->ns.current(interp);

  // Query mode: no args returns list of imported commands
  if (argc == 0) {
    FeatherObj dict = get_imports_dict(ops, interp, current);
    FeatherObj keys = ops->dict.keys(interp, dict);
    // Convert to space-separated string
    size_t num_keys = ops->list.length(interp, keys);
    if (num_keys == 0) {
      ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      return TCL_OK;
    }
    FeatherObj result = ops->list.at(interp, keys, 0);
    for (size_t i = 1; i < num_keys; i++) {
      result = ops->string.concat(interp, result, ops->string.intern(interp, " ", 1));
      result = ops->string.concat(interp, result, ops->list.at(interp, keys, i));
    }
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Check for -force flag
  int force = 0;
  FeatherObj first = ops->list.at(interp, args, 0);
  if (feather_obj_eq_literal(ops, interp, first, "-force")) {
    force = 1;
    ops->list.shift(interp, args);
    argc--;
  }

  // Process each pattern
  for (size_t i = 0; i < argc; i++) {
    FeatherObj pattern = ops->list.at(interp, args, i);

    // Pattern is something like "math::double" or "math::*"
    // Split into namespace and command pattern
    // Find last ::
    long last_sep = feather_obj_find_last_colons(ops, interp, pattern);

    if (last_sep < 0) {
      // No :: in pattern - error
      FeatherObj msg = ops->string.intern(interp, "unknown or unexported command \"", 31);
      msg = ops->string.concat(interp, msg, pattern);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Extract namespace path and command pattern
    size_t pat_len = ops->string.byte_length(interp, pattern);
    FeatherObj srcNs = ops->string.slice(interp, pattern, 0, (size_t)last_sep);
    if (last_sep == 0 && pat_len >= 2 &&
        ops->string.byte_at(interp, pattern, 0) == ':' &&
        ops->string.byte_at(interp, pattern, 1) == ':') {
      // Pattern like "::cmd" means global namespace
      srcNs = ops->string.intern(interp, "::", 2);
    }
    // Resolve relative namespace
    srcNs = resolve_ns_path(ops, interp, srcNs);

    // Check if source namespace exists
    if (!ops->ns.exists(interp, srcNs)) {
      // Remove leading :: for relative display
      FeatherObj displayNs = srcNs;
      size_t ns_len = ops->string.byte_length(interp, srcNs);
      if (ns_len > 2 &&
          ops->string.byte_at(interp, srcNs, 0) == ':' &&
          ops->string.byte_at(interp, srcNs, 1) == ':') {
        displayNs = ops->string.slice(interp, srcNs, 2, ns_len);
      }
      FeatherObj msg = ops->string.intern(interp, "namespace \"", 11);
      msg = ops->string.concat(interp, msg, displayNs);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\" not found", 11));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    FeatherObj cmdPattern = ops->string.slice(interp, pattern, (size_t)last_sep + 2, pat_len);

    // Get list of commands in source namespace
    FeatherObj srcCmds = ops->ns.list_commands(interp, srcNs);
    size_t numCmds = ops->list.length(interp, srcCmds);

    // Check if pattern contains wildcard
    int has_wildcard = feather_obj_contains_char(ops, interp, cmdPattern, '*') ||
                       feather_obj_contains_char(ops, interp, cmdPattern, '?');

    int matched = 0;
    for (size_t j = 0; j < numCmds; j++) {
      FeatherObj cmdName = ops->list.at(interp, srcCmds, j);

      // Check if command matches pattern
      int matches = 0;
      if (has_wildcard) {
        matches = feather_obj_glob_match(ops, interp, cmdPattern, cmdName);
      } else {
        matches = ops->string.equal(interp, cmdPattern, cmdName);
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
      FeatherCommandType existingType = ops->ns.get_command(interp, current, cmdName, &unusedFn, NULL, NULL);
      if (existingType != TCL_CMD_NONE && !force) {
        FeatherObj msg = ops->string.intern(interp, "can't import command \"", 22);
        msg = ops->string.concat(interp, msg, cmdName);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\": already exists", 17));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Copy command from source to current namespace
      ops->ns.copy_command(interp, srcNs, cmdName, current, cmdName);

      // Record the import for origin/forget tracking
      record_import(ops, interp, current, cmdName, srcNs, cmdName);
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

// namespace origin command
static FeatherResult ns_origin(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace origin name\"", 47);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj name = ops->list.at(interp, args, 0);
  FeatherObj current = ops->ns.current(interp);

  // First check if the command exists
  FeatherBuiltinCmd fn;
  FeatherCommandType cmdType = ops->ns.get_command(interp, current, name, &fn, NULL, NULL);
  if (cmdType == TCL_CMD_NONE) {
    // Try global namespace
    FeatherObj global = ops->string.intern(interp, "::", 2);
    cmdType = ops->ns.get_command(interp, global, name, &fn, NULL, NULL);
    if (cmdType == TCL_CMD_NONE) {
      FeatherObj msg = ops->string.intern(interp, "invalid command name \"", 22);
      msg = ops->string.concat(interp, msg, name);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    // Command found in global, return its qualified name
    FeatherObj result = ops->string.intern(interp, "::", 2);
    result = ops->string.concat(interp, result, name);
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  // Command exists in current namespace - check if it's an import
  FeatherObj dict = get_imports_dict(ops, interp, current);
  FeatherObj origin = ops->dict.get(interp, dict, name);

  if (origin != 0) {
    // It's an imported command, return the origin
    ops->interp.set_result(interp, origin);
  } else {
    // Not imported, return the qualified name in current namespace
    FeatherObj result;
    if (feather_obj_is_global_ns(ops, interp, current)) {
      result = ops->string.intern(interp, "::", 2);
      result = ops->string.concat(interp, result, name);
    } else {
      result = ops->string.concat(interp, current, ops->string.intern(interp, "::", 2));
      result = ops->string.concat(interp, result, name);
    }
    ops->interp.set_result(interp, result);
  }
  return TCL_OK;
}

// namespace forget ?pattern ...?
static FeatherResult ns_forget(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  FeatherObj current = ops->ns.current(interp);

  // No args is a no-op
  if (argc == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Get the imports dict
  FeatherObj dict = get_imports_dict(ops, interp, current);

  // Process each pattern
  for (size_t i = 0; i < argc; i++) {
    FeatherObj pattern = ops->list.at(interp, args, i);

    // Pattern should be like ::src::cmd or ::src::*
    long last_sep = feather_obj_find_last_colons(ops, interp, pattern);
    if (last_sep < 0) {
      // No :: - just use as local pattern
      continue;
    }

    size_t pat_len = ops->string.byte_length(interp, pattern);
    FeatherObj srcNs = ops->string.slice(interp, pattern, 0, (size_t)last_sep);
    if (ops->string.byte_length(interp, srcNs) == 0) {
      srcNs = ops->string.intern(interp, "::", 2);
    }
    srcNs = resolve_ns_path(ops, interp, srcNs);
    FeatherObj cmdPattern = ops->string.slice(interp, pattern, (size_t)last_sep + 2, pat_len);

    int has_wildcard = feather_obj_contains_char(ops, interp, cmdPattern, '*') ||
                       feather_obj_contains_char(ops, interp, cmdPattern, '?');

    // Iterate over imported commands and remove matching ones
    FeatherObj keys = ops->dict.keys(interp, dict);
    size_t num_keys = ops->list.length(interp, keys);

    for (size_t j = 0; j < num_keys; j++) {
      FeatherObj cmdName = ops->list.at(interp, keys, j);
      FeatherObj origin = ops->dict.get(interp, dict, cmdName);

      // Check if origin matches srcNs::cmdPattern
      // First check if origin starts with srcNs::
      size_t origin_len = ops->string.byte_length(interp, origin);
      size_t srcNs_len = ops->string.byte_length(interp, srcNs);

      if (origin_len < srcNs_len + 2) continue;

      // Check namespace prefix
      FeatherObj origin_ns = ops->string.slice(interp, origin, 0, srcNs_len);
      if (!ops->string.equal(interp, origin_ns, srcNs)) continue;

      // Check separator
      if (ops->string.byte_at(interp, origin, srcNs_len) != ':' ||
          ops->string.byte_at(interp, origin, srcNs_len + 1) != ':') continue;

      // Get the command name part from origin
      FeatherObj origin_cmd = ops->string.slice(interp, origin, srcNs_len + 2, origin_len);

      // Check if it matches the pattern
      int matches = 0;
      if (has_wildcard) {
        matches = feather_obj_glob_match(ops, interp, cmdPattern, origin_cmd);
      } else {
        matches = ops->string.equal(interp, cmdPattern, origin_cmd);
      }

      if (matches) {
        // Remove the command from current namespace
        ops->ns.delete_command(interp, current, cmdName);
        // Remove from imports dict
        dict = ops->dict.remove(interp, dict, cmdName);
      }
    }
  }

  set_imports_dict(ops, interp, current, dict);
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

// namespace inscope ns script ?arg ...?
static FeatherResult ns_inscope(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace inscope name arg ?arg...?\"", 61);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj ns_path = ops->list.at(interp, args, 0);
  FeatherObj abs_path = resolve_ns_path(ops, interp, ns_path);

  // Check if namespace exists
  if (!ops->ns.exists(interp, abs_path)) {
    FeatherObj msg = ops->string.intern(interp, "namespace \"", 11);
    msg = ops->string.concat(interp, msg, abs_path);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\" not found", 11));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get the script (first arg after namespace)
  FeatherObj script = ops->list.at(interp, args, 1);

  // If there are additional args, append them as list elements
  // TCL inscope appends extra args to the script as list elements
  if (argc > 2) {
    // Create a list with the script as first element, then extra args
    FeatherObj cmdList = ops->list.create(interp);
    cmdList = ops->list.push(interp, cmdList, script);
    for (size_t i = 2; i < argc; i++) {
      cmdList = ops->list.push(interp, cmdList, ops->list.at(interp, args, i));
    }
    script = cmdList;
  }

  // Save current namespace
  FeatherObj saved_ns = ops->frame.get_namespace(interp);

  // Set current frame's namespace and locals
  ops->frame.set_namespace(interp, abs_path);
  ops->frame.push_locals(interp, abs_path);

  // Evaluate the script
  FeatherResult result = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

  // Restore locals and namespace
  ops->frame.pop_locals(interp);
  ops->frame.set_namespace(interp, saved_ns);

  return result;
}

// namespace code script
static FeatherResult ns_code(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace code arg\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj script = ops->list.at(interp, args, 0);
  FeatherObj current = ops->ns.current(interp);

  // Build result: "::namespace inscope <ns> {<script>}"
  FeatherObj result = ops->string.intern(interp, "::namespace inscope ", 20);
  result = ops->string.concat(interp, result, current);
  result = ops->string.concat(interp, result, ops->string.intern(interp, " {", 2));
  result = ops->string.concat(interp, result, script);
  result = ops->string.concat(interp, result, ops->string.intern(interp, "}", 1));

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// namespace which ?-command? ?-variable? name
static FeatherResult ns_which(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc == 0 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace which ?-command? ?-variable? name\"", 69);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  int is_variable = 0;
  FeatherObj name;

  if (argc == 1) {
    // Default to -command
    name = ops->list.at(interp, args, 0);
  } else {
    // argc == 2: option and name
    FeatherObj option = ops->list.at(interp, args, 0);
    name = ops->list.at(interp, args, 1);
    if (feather_obj_eq_literal(ops, interp, option, "-variable")) {
      is_variable = 1;
    } else if (!feather_obj_eq_literal(ops, interp, option, "-command")) {
      FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"namespace which ?-command? ?-variable? name\"", 69);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Resolve the name to a fully-qualified name
  FeatherObj current = ops->ns.current(interp);

  if (is_variable) {
    // Check if name is already absolute
    size_t name_len = ops->string.byte_length(interp, name);
    if (name_len >= 2 && ops->string.byte_at(interp, name, 0) == ':' &&
        ops->string.byte_at(interp, name, 1) == ':') {
      // Absolute name - extract namespace and variable name
      long last_sep = feather_obj_find_last_colons(ops, interp, name);
      FeatherObj ns = (last_sep <= 0) ? ops->string.intern(interp, "::", 2)
                                      : ops->string.slice(interp, name, 0, (size_t)last_sep);
      FeatherObj varname = ops->string.slice(interp, name, (size_t)last_sep + 2, name_len);
      if (ops->ns.var_exists(interp, ns, varname)) {
        ops->interp.set_result(interp, name);
      } else {
        ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      }
    } else {
      // Relative name - check in current namespace
      if (ops->ns.var_exists(interp, current, name)) {
        FeatherObj result;
        if (feather_obj_is_global_ns(ops, interp, current)) {
          result = ops->string.intern(interp, "::", 2);
          result = ops->string.concat(interp, result, name);
        } else {
          result = ops->string.concat(interp, current, ops->string.intern(interp, "::", 2));
          result = ops->string.concat(interp, result, name);
        }
        ops->interp.set_result(interp, result);
      } else {
        ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      }
    }
  } else {
    // Check for command
    size_t name_len = ops->string.byte_length(interp, name);
    if (name_len >= 2 && ops->string.byte_at(interp, name, 0) == ':' &&
        ops->string.byte_at(interp, name, 1) == ':') {
      // Absolute name - extract namespace and command name
      long last_sep = feather_obj_find_last_colons(ops, interp, name);
      FeatherObj ns = (last_sep <= 0) ? ops->string.intern(interp, "::", 2)
                                      : ops->string.slice(interp, name, 0, (size_t)last_sep);
      FeatherObj cmdname = ops->string.slice(interp, name, (size_t)last_sep + 2, name_len);
      FeatherBuiltinCmd fn;
      FeatherCommandType cmdType = ops->ns.get_command(interp, ns, cmdname, &fn, NULL, NULL);
      if (cmdType != TCL_CMD_NONE) {
        ops->interp.set_result(interp, name);
      } else {
        ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
      }
    } else {
      // Relative name - search current namespace first, then global
      FeatherBuiltinCmd fn;
      FeatherCommandType cmdType = ops->ns.get_command(interp, current, name, &fn, NULL, NULL);
      if (cmdType != TCL_CMD_NONE) {
        FeatherObj result;
        if (feather_obj_is_global_ns(ops, interp, current)) {
          result = ops->string.intern(interp, "::", 2);
          result = ops->string.concat(interp, result, name);
        } else {
          result = ops->string.concat(interp, current, ops->string.intern(interp, "::", 2));
          result = ops->string.concat(interp, result, name);
        }
        ops->interp.set_result(interp, result);
      } else {
        // Try global namespace
        FeatherObj global = ops->string.intern(interp, "::", 2);
        cmdType = ops->ns.get_command(interp, global, name, &fn, NULL, NULL);
        if (cmdType != TCL_CMD_NONE) {
          FeatherObj result = ops->string.intern(interp, "::", 2);
          result = ops->string.concat(interp, result, name);
          ops->interp.set_result(interp, result);
        } else {
          ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
        }
      }
    }
  }

  return TCL_OK;
}

void feather_register_namespace_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Create and manipulate contexts for commands and variables",
    "The namespace command lets you create, access, and destroy separate "
    "contexts for commands and variables. Namespaces are hierarchical and "
    "can be nested. The global namespace is represented by \"::\" and all "
    "other namespaces are its descendants. Namespace names starting with "
    "\"::\" are absolute, while others are relative to the current namespace.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: children ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?namespace?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?pattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "children", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns a list of all child namespaces that belong to the namespace. "
    "If namespace is not specified, then the children are returned for the "
    "current namespace. This command returns fully-qualified names, which "
    "start with a double colon (::).\n\n"
    "If the optional pattern is given, then this command returns only the "
    "names that match the glob-style pattern. The actual pattern used is "
    "determined as follows: a pattern that starts with double colon (::) is "
    "used directly, otherwise the namespace (or the fully-qualified name of "
    "the current namespace) is prepended onto the pattern.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: code ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<script>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "code", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Captures the current namespace context for later execution of the "
    "script. It returns a new script in which script has been wrapped in a "
    "namespace inscope command. The new script has two important properties. "
    "First, it can be evaluated in any namespace and will cause script to be "
    "evaluated in the current namespace (the one where the namespace code "
    "command was invoked). Second, additional arguments can be appended to "
    "the resulting script and they will be passed to script as additional "
    "arguments.\n\n"
    "This command is needed because callbacks are normally executed in the "
    "global namespace. A scoped command captures a command together with its "
    "namespace context in a way that allows it to be executed properly later.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: current ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_cmd(ops, interp, "current", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the fully-qualified name for the current namespace. The actual "
    "name of the global namespace is \"\" (i.e., an empty string), but this "
    "command returns :: for the global namespace as a convenience to "
    "programmers.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: delete ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?namespace?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "delete", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Each namespace is deleted and all variables, procedures, and child "
    "namespaces contained in the namespace are deleted. If a namespace does "
    "not exist, this command returns an error. If no namespace names are "
    "given, this command does nothing.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: eval ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<namespace>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<arg>...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "eval", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Activates a namespace called namespace and evaluates some code in that "
    "context. If the namespace does not already exist, it is created. If "
    "more than one arg argument is specified, the arguments are concatenated "
    "together with a space between each one in the same fashion as the eval "
    "command, and the result is evaluated.\n\n"
    "If namespace has leading namespace qualifiers and any leading namespaces "
    "do not exist, they are automatically created.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: exists ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<namespace>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "exists", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if namespace is a valid namespace in the current context, "
    "returns 0 otherwise.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: export ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-clear", NULL, NULL);
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?pattern?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "export", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Specifies which commands are exported from a namespace. The exported "
    "commands are those that can be later imported into another namespace "
    "using a namespace import command. Both commands defined in a namespace "
    "and commands the namespace has previously imported can be exported by a "
    "namespace. The commands do not have to be defined at the time the "
    "namespace export command is executed.\n\n"
    "Each pattern may contain glob-style special characters, but it may not "
    "include any namespace qualifiers. That is, the pattern can only specify "
    "commands in the current (exporting) namespace. Each pattern is appended "
    "onto the namespace's list of export patterns. If the -clear flag is "
    "given, the namespace's export pattern list is reset to empty before any "
    "pattern arguments are appended. If no patterns are given and the -clear "
    "flag is not given, this command returns the namespace's current export "
    "list.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: forget ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?pattern?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "forget", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Removes previously imported commands from a namespace. Each pattern is "
    "a simple or qualified name such as x, foo::x or a::b::p*. Qualified "
    "names contain double colons (::) and qualify a name with the name of "
    "one or more namespaces.\n\n"
    "For each simple pattern this command deletes the matching commands of "
    "the current namespace that were imported from a different namespace. "
    "For qualified patterns, this command first finds the matching exported "
    "commands. It then checks whether any of those commands were previously "
    "imported by the current namespace. If so, this command deletes the "
    "corresponding imported commands. In effect, this undoes the action of a "
    "namespace import command.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: import ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-force", NULL, NULL);
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?pattern?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "import", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Imports commands into a namespace, or queries the set of imported "
    "commands in a namespace. When no arguments are present, namespace import "
    "returns the list of commands in the current namespace that have been "
    "imported from other namespaces.\n\n"
    "When pattern arguments are present, each pattern is a qualified name "
    "like foo::x or a::p*. That is, it includes the name of an exporting "
    "namespace and may have glob-style special characters in the command name "
    "at the end of the qualified name. All the commands that match a pattern "
    "string and which are currently exported from their namespace are added "
    "to the current namespace. This command normally returns an error if an "
    "imported command conflicts with an existing command. However, if the "
    "-force option is given, imported commands will silently replace existing "
    "commands.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: inscope ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<namespace>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<script>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?arg?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "inscope", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Executes a script in the context of the specified namespace. This "
    "command is not expected to be used directly by programmers; calls to it "
    "are generated implicitly when applications use namespace code commands "
    "to create callback scripts.\n\n"
    "The namespace inscope command is much like the namespace eval command "
    "except that the namespace must already exist, and namespace inscope "
    "appends additional args as proper list elements. Thus additional "
    "arguments will not undergo a second round of substitution, as is the "
    "case with namespace eval.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: origin ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<command>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "origin", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the fully-qualified name of the original command to which the "
    "imported command refers. When a command is imported into a namespace, a "
    "new command is created in that namespace that points to the actual "
    "command in the exporting namespace.\n\n"
    "If a command is imported into a sequence of namespaces a, b,...,n where "
    "each successive namespace just imports the command from the previous "
    "namespace, this command returns the fully-qualified name of the original "
    "command in the first namespace, a. If command does not refer to an "
    "imported command, the command's own fully-qualified name is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: parent ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?namespace?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "parent", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the fully-qualified name of the parent namespace for namespace. "
    "If namespace is not specified, the fully-qualified name of the current "
    "namespace's parent is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: qualifiers ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "qualifiers", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns any leading namespace qualifiers for string. Qualifiers are "
    "namespace names separated by double colons (::). For the string "
    "::foo::bar::x, this command returns ::foo::bar, and for :: it returns "
    "an empty string. This command is the complement of the namespace tail "
    "command. It does not check whether the namespace names are, in fact, "
    "the names of currently defined namespaces.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: tail ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<string>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "tail", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the simple name at the end of a qualified string. Qualifiers "
    "are namespace names separated by double colons (::). For the string "
    "::foo::bar::x, this command returns x, and for :: it returns an empty "
    "string. This command is the complement of the namespace qualifiers "
    "command. It does not check whether the namespace names are, in fact, "
    "the names of currently defined namespaces.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: which ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_flag(ops, interp, "-command", NULL, NULL);
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_flag(ops, interp, "-variable", NULL, NULL);
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<name>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "which", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Looks up name as either a command or variable and returns its "
    "fully-qualified name. For example, if name does not exist in the current "
    "namespace but does exist in the global namespace, this command returns a "
    "fully-qualified name in the global namespace. If the command or variable "
    "does not exist, this command returns an empty string. If no flag is "
    "given, name is treated as a command name.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Examples ---
  e = feather_usage_example(ops, interp,
    "namespace current",
    "Get the current namespace (returns \"::\" if in global namespace)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "namespace eval ::math { proc double {x} { expr {$x * 2} } }",
    "Create a namespace and define a procedure in it",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "namespace eval ::math {\n"
    "    proc add {a b} { expr {$a + $b} }\n"
    "    namespace export add\n"
    "}\n"
    "namespace import ::math::add",
    "Export and import commands between namespaces",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "namespace qualifiers ::math::trig::sin",
    "Extract namespace prefix (returns \"::math::trig\")",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "namespace", spec);
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

  if (feather_obj_eq_literal(ops, interp, subcmd, "current")) {
    return ns_current(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "eval")) {
    return ns_eval(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "exists")) {
    return ns_exists(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "children")) {
    return ns_children(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "parent")) {
    return ns_parent(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "delete")) {
    return ns_delete(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "export")) {
    return ns_export(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "import")) {
    return ns_import(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "origin")) {
    return ns_origin(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "forget")) {
    return ns_forget(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "qualifiers")) {
    return ns_qualifiers(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "tail")) {
    return ns_tail(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "inscope")) {
    return ns_inscope(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "code")) {
    return ns_code(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "which")) {
    return ns_which(ops, interp, args);
  } else {
    FeatherObj msg = ops->string.intern(interp,
      "bad option \"", 12);
    msg = ops->string.concat(interp, msg, subcmd);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be children, code, current, delete, eval, exists, export, forget, import, inscope, origin, parent, qualifiers, tail, or which", 133);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
