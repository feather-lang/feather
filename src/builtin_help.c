#include "feather.h"
#include "internal.h"

/*
 * Extract the top-level help text from a usage spec.
 * Returns empty string if no help is defined.
 */
static FeatherObj extract_help_text(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj spec) {
  size_t spec_len = ops->list.length(interp, spec);

  for (size_t i = 0; i < spec_len; i++) {
    FeatherObj entry = ops->list.at(interp, spec, i);
    FeatherObj type_key = ops->string.intern(interp, "type", 4);
    FeatherObj type_val = ops->dict.get(interp, entry, type_key);

    if (ops->list.is_nil(interp, type_val)) continue;

    // Look for meta entry with help text
    if (feather_obj_eq_literal(ops, interp, type_val, "meta")) {
      FeatherObj help_key = ops->string.intern(interp, "about", 5);
      FeatherObj help_val = ops->dict.get(interp, entry, help_key);
      if (!ops->list.is_nil(interp, help_val)) {
        return help_val;
      }
    }
  }

  return ops->string.intern(interp, "", 0);
}

/*
 * Collect all subcommands from a usage spec.
 * Returns a list of dicts, each containing {name help hide}.
 */
static FeatherObj collect_subcommands(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj spec) {
  FeatherObj result = ops->list.create(interp);
  size_t spec_len = ops->list.length(interp, spec);

  for (size_t i = 0; i < spec_len; i++) {
    FeatherObj entry = ops->list.at(interp, spec, i);
    FeatherObj type_key = ops->string.intern(interp, "type", 4);
    FeatherObj type_val = ops->dict.get(interp, entry, type_key);

    if (ops->list.is_nil(interp, type_val)) continue;

    // Look for cmd entries
    if (feather_obj_eq_literal(ops, interp, type_val, "cmd")) {
      result = ops->list.push(interp, result, entry);
    }
  }

  return result;
}

/*
 * Check if a command entry should be hidden.
 */
static int is_hidden(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj entry) {
  FeatherObj hide_key = ops->string.intern(interp, "hide", 4);
  FeatherObj hide_val = ops->dict.get(interp, entry, hide_key);

  if (ops->list.is_nil(interp, hide_val)) return 0;

  // Try to get as integer - if it's truthy (non-zero), it's hidden
  int64_t hide_int = 0;
  if (ops->integer.get(interp, hide_val, &hide_int) == TCL_OK) {
    return hide_int != 0;
  }

  // If not an integer, check if it's a non-empty string
  return ops->string.byte_length(interp, hide_val) > 0;
}

/*
 * Get the usage spec for a command.
 * Returns nil if not found.
 */
static FeatherObj get_usage_spec(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj cmdname) {
  // Get ::usage::specs variable
  FeatherObj specs_var = ops->string.intern(interp, "::usage::specs", 14);
  FeatherObj specs_dict;
  FeatherResult res = feather_get_var(ops, interp, specs_var, &specs_dict);

  if (res != TCL_OK || ops->list.is_nil(interp, specs_dict)) {
    return ops->list.create(interp);  // Return empty list
  }

  // Look up the command
  FeatherObj spec_entry = ops->dict.get(interp, specs_dict, cmdname);
  if (ops->list.is_nil(interp, spec_entry)) {
    return ops->list.create(interp);
  }

  // Extract the parsed spec (stored under "spec" key)
  FeatherObj spec_key = ops->string.intern(interp, "spec", 4);
  FeatherObj spec = ops->dict.get(interp, spec_entry, spec_key);
  if (ops->list.is_nil(interp, spec)) {
    return ops->list.create(interp);
  }

  return spec;
}

/*
 * List all commands with their help strings.
 * Note: Commands are listed in the order returned by the namespace operations (unsorted).
 */
static FeatherResult help_list_all(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj globalNs = ops->string.intern(interp, "::", 2);
  FeatherObj currentNs = ops->ns.current(interp);
  int inGlobalNs = feather_obj_is_global_ns(ops, interp, currentNs);

  // Collect all visible commands (current + global namespace)
  FeatherObj all_commands = ops->list.create(interp);

  // Add commands from current namespace
  FeatherObj currentNames = ops->ns.list_commands(interp, currentNs);
  size_t currentCount = ops->list.length(interp, currentNames);
  for (size_t i = 0; i < currentCount; i++) {
    all_commands = ops->list.push(interp, all_commands, ops->list.at(interp, currentNames, i));
  }

  // If not in global namespace, add global commands (avoiding duplicates)
  if (!inGlobalNs) {
    FeatherObj globalNames = ops->ns.list_commands(interp, globalNs);
    size_t globalCount = ops->list.length(interp, globalNames);
    for (size_t i = 0; i < globalCount; i++) {
      FeatherObj name = ops->list.at(interp, globalNames, i);

      // Check if already in list
      int found = 0;
      for (size_t j = 0; j < currentCount; j++) {
        if (ops->string.equal(interp, name, ops->list.at(interp, currentNames, j))) {
          found = 1;
          break;
        }
      }

      if (!found) {
        all_commands = ops->list.push(interp, all_commands, name);
      }
    }
  }

  // Build output string
  size_t total_count = ops->list.length(interp, all_commands);
  if (total_count == 0) {
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  FeatherObj output = ops->string.intern(interp, "", 0);

  for (size_t i = 0; i < total_count; i++) {
    FeatherObj cmdname = ops->list.at(interp, all_commands, i);

    // Ensure usage is registered (triggers lazy loading)
    feather_ensure_usage_registered(ops, interp, cmdname);

    FeatherObj spec = get_usage_spec(ops, interp, cmdname);

    // Get top-level help text
    FeatherObj help_text = extract_help_text(ops, interp, spec);

    // Get subcommands
    FeatherObj subcommands = collect_subcommands(ops, interp, spec);
    size_t subcmd_count = ops->list.length(interp, subcommands);

    // Format: "cmdname - help text\n"
    if (ops->string.byte_length(interp, help_text) > 0) {
      output = ops->string.concat(interp, output, cmdname);
      output = ops->string.concat(interp, output, ops->string.intern(interp, " - ", 3));
      output = ops->string.concat(interp, output, help_text);
      output = ops->string.concat(interp, output, ops->string.intern(interp, "\n", 1));
    } else if (subcmd_count == 0) {
      // No help and no subcommands, just show name
      output = ops->string.concat(interp, output, cmdname);
      output = ops->string.concat(interp, output, ops->string.intern(interp, "\n", 1));
    } else {
      // Has subcommands but no top-level help
      output = ops->string.concat(interp, output, cmdname);
      output = ops->string.concat(interp, output, ops->string.intern(interp, "\n", 1));
    }

    // Show subcommands indented
    for (size_t j = 0; j < subcmd_count; j++) {
      FeatherObj subcmd = ops->list.at(interp, subcommands, j);

      // Skip hidden subcommands
      if (is_hidden(ops, interp, subcmd)) continue;

      // Get subcommand name and help
      FeatherObj name_key = ops->string.intern(interp, "name", 4);
      FeatherObj subcmd_name = ops->dict.get(interp, subcmd, name_key);

      FeatherObj help_key = ops->string.intern(interp, "help", 4);
      FeatherObj subcmd_help = ops->dict.get(interp, subcmd, help_key);

      if (ops->list.is_nil(interp, subcmd_name)) continue;

      // Format: "  subcommand - help\n"
      output = ops->string.concat(interp, output, ops->string.intern(interp, "  ", 2));
      output = ops->string.concat(interp, output, subcmd_name);

      if (!ops->list.is_nil(interp, subcmd_help) && ops->string.byte_length(interp, subcmd_help) > 0) {
        output = ops->string.concat(interp, output, ops->string.intern(interp, " - ", 3));
        output = ops->string.concat(interp, output, subcmd_help);
      }

      output = ops->string.concat(interp, output, ops->string.intern(interp, "\n", 1));
    }
  }

  ops->interp.set_result(interp, output);
  return TCL_OK;
}

/*
 * Show help for a specific command by delegating to usage help.
 */
static FeatherResult help_show_command(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj args) {
  // Build argument list for "usage help <command> ?subcommand...?"
  FeatherObj usage_args = ops->list.create(interp);
  usage_args = ops->list.push(interp, usage_args, ops->string.intern(interp, "help", 4));

  // Add all arguments (command name and optional subcommands)
  size_t argc = ops->list.length(interp, args);
  for (size_t i = 0; i < argc; i++) {
    usage_args = ops->list.push(interp, usage_args, ops->list.at(interp, args, i));
  }

  // Call usage builtin
  FeatherObj usage_cmd = ops->string.intern(interp, "usage", 5);
  return feather_builtin_usage(ops, interp, usage_cmd, usage_args);
}

/*
 * Main help command implementation.
 */
FeatherResult feather_builtin_help(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    // List all commands with their help strings
    return help_list_all(ops, interp);
  } else {
    // Show help for specific command
    return help_show_command(ops, interp, args);
  }
}

/*
 * Register usage spec for the help command.
 */
void feather_register_help_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Display help for commands",
    "When invoked without arguments, displays a list of all commands with "
    "their short descriptions. Commands with subcommands show the subcommands "
    "indented below.\n\n"
    "When invoked with a command name, displays the full help for that command "
    "by delegating to 'usage help'.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?command?");
  e = feather_usage_help(ops, interp, e, "Command name to show help for");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?subcommand?...");
  e = feather_usage_help(ops, interp, e, "Subcommand path (e.g., 'string match')");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "help",
    "List all commands",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "help string",
    "Show help for the string command",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "help string match",
    "Show help for the string match subcommand",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "help", spec);
}
