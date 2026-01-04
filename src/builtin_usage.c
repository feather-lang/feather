#include "feather.h"
#include "internal.h"

/**
 * builtin_usage.c - Implements the 'usage' command for CLI argument parsing.
 *
 * Based on the usage specification from https://usage.jdx.dev
 *
 * TCL Interface:
 *   usage for $command ?spec?   - Define or get usage spec for a command
 *   usage parse $command $args  - Parse args list and create local variables
 *
 * Spec Format (TCL-friendly KDL-inspired):
 *   arg "<name>"                - Required positional argument
 *   arg "?name?"                - Optional positional argument
 *   arg "<name>..."             - Variadic required (1 or more)
 *   arg "?name?..."             - Variadic optional (0 or more)
 *   flag "-s --long"            - Boolean flag (short and/or long)
 *   flag "-s --long <value>"    - Flag with required value
 *   flag "-s --long ?value?"    - Flag with optional value
 *
 * Options on arg/flag:
 *   help="description"          - Help text
 *   default="value"             - Default value
 *
 * Example:
 *   usage for mycommand {
 *     arg "<input>" help="input file"
 *     arg "?output?" default="out.txt"
 *     flag "-v --verbose" help="verbose output"
 *     flag "-n --count <num>" help="repeat count"
 *   }
 *
 *   proc mycommand {args} {
 *     usage parse mycommand $args
 *     # Now have local vars: input, output, verbose, count
 *   }
 *
 * Note: Uses ?arg? instead of [arg] for optional args because []
 * triggers command substitution in TCL.
 */

/* Storage namespace for usage specs: ::tcl::usage */
#define USAGE_NS "::tcl::usage"

/**
 * Convert hyphens to underscores in a string for valid TCL variable names.
 * E.g., "ignore-case" becomes "ignore_case"
 */
static FeatherObj sanitize_var_name(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj name) {
  size_t len = ops->string.byte_length(interp, name);
  int hasHyphen = 0;

  /* Check if we need to convert */
  for (size_t i = 0; i < len; i++) {
    if (ops->string.byte_at(interp, name, i) == '-') {
      hasHyphen = 1;
      break;
    }
  }

  if (!hasHyphen) {
    return name;
  }

  /* Build new string with hyphens replaced */
  FeatherObj builder = ops->string.builder_new(interp, len);
  for (size_t i = 0; i < len; i++) {
    int c = ops->string.byte_at(interp, name, i);
    if (c == '-') {
      ops->string.builder_append_byte(interp, builder, '_');
    } else {
      ops->string.builder_append_byte(interp, builder, (char)c);
    }
  }
  return ops->string.builder_finish(interp, builder);
}

/**
 * Get the usage specs dictionary from ::tcl::usage::specs
 */
static FeatherObj usage_get_specs(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj ns = ops->string.intern(interp, USAGE_NS, 12);
  FeatherObj varName = ops->string.intern(interp, "specs", 5);
  FeatherObj specs = ops->ns.get_var(interp, ns, varName);
  if (ops->list.is_nil(interp, specs)) {
    specs = ops->dict.create(interp);
  }
  return specs;
}

/**
 * Store the usage specs dictionary
 */
static void usage_set_specs(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj specs) {
  FeatherObj ns = ops->string.intern(interp, USAGE_NS, 12);
  FeatherObj varName = ops->string.intern(interp, "specs", 5);
  ops->ns.set_var(interp, ns, varName, specs);
}

/**
 * Helper to check if an option string starts with a prefix and extract value.
 * Returns the value (with quotes or braces stripped) or NULL if no match.
 * Supports: key="value", key={value}, key=value
 */
static FeatherObj parse_option_value(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj opt, const char *prefix, size_t prefixLen) {
  size_t optLen = ops->string.byte_length(interp, opt);
  if (optLen <= prefixLen) return 0;

  for (size_t k = 0; k < prefixLen; k++) {
    if (ops->string.byte_at(interp, opt, k) != prefix[k]) return 0;
  }

  FeatherObj val = ops->string.slice(interp, opt, prefixLen, optLen);
  size_t valLen = ops->string.byte_length(interp, val);

  /* Strip quotes if present */
  if (valLen >= 2 && ops->string.byte_at(interp, val, 0) == '"' &&
      ops->string.byte_at(interp, val, valLen - 1) == '"') {
    return ops->string.slice(interp, val, 1, valLen - 1);
  }
  /* Strip braces if present */
  if (valLen >= 2 && ops->string.byte_at(interp, val, 0) == '{' &&
      ops->string.byte_at(interp, val, valLen - 1) == '}') {
    return ops->string.slice(interp, val, 1, valLen - 1);
  }
  return val;
}

/**
 * Parse a spec string into a structured representation.
 *
 * The spec is a script-like format with 'arg' and 'flag' declarations.
 * Returns a list of {type details...} entries.
 *
 * Entry formats:
 *   arg:  {arg name required variadic help default long_help choices hide}
 *   flag: {flag short long hasValue valueRequired varName help long_help choices hide}
 *   cmd:  {cmd name subSpec help long_help hide}
 */
static FeatherObj parse_spec(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj specStr) {
  FeatherObj result = ops->list.create(interp);

  /* Parse the spec as a TCL list/script */
  FeatherObj specList = feather_list_parse_obj(ops, interp, specStr);
  size_t specLen = ops->list.length(interp, specList);

  size_t i = 0;
  while (i < specLen) {
    FeatherObj keyword = ops->list.at(interp, specList, i);
    i++;

    if (feather_obj_eq_literal(ops, interp, keyword, "arg")) {
      /* arg "<name>" or arg "?name?" with options */
      if (i >= specLen) break;

      FeatherObj argName = ops->list.at(interp, specList, i);
      i++;

      /* Create arg entry: {arg name required variadic help default} */
      FeatherObj entry = ops->list.create(interp);
      entry = ops->list.push(interp, entry, ops->string.intern(interp, "arg", 3));

      /* Parse the arg name format: <name>, ?name?, <name>..., ?name?... */
      size_t nameLen = ops->string.byte_length(interp, argName);
      int required = 0;
      int variadic = 0;
      FeatherObj cleanName;

      if (nameLen >= 2) {
        int first = ops->string.byte_at(interp, argName, 0);
        int last = ops->string.byte_at(interp, argName, nameLen - 1);

        /* Check for variadic (...) */
        if (nameLen >= 5) {
          int dot1 = ops->string.byte_at(interp, argName, nameLen - 3);
          int dot2 = ops->string.byte_at(interp, argName, nameLen - 2);
          int dot3 = ops->string.byte_at(interp, argName, nameLen - 1);
          if (dot1 == '.' && dot2 == '.' && dot3 == '.') {
            variadic = 1;
            nameLen -= 3;
            last = ops->string.byte_at(interp, argName, nameLen - 1);
          }
        }

        if (first == '<' && last == '>') {
          required = 1;
          cleanName = ops->string.slice(interp, argName, 1, nameLen - 1);
        } else if (first == '?' && last == '?') {
          required = 0;
          cleanName = ops->string.slice(interp, argName, 1, nameLen - 1);
        } else {
          cleanName = argName;
          required = 1;
        }
      } else {
        cleanName = argName;
        required = 1;
      }

      entry = ops->list.push(interp, entry, cleanName);
      entry = ops->list.push(interp, entry, ops->integer.create(interp, required));
      entry = ops->list.push(interp, entry, ops->integer.create(interp, variadic));

      /* Look for options: help, default, long_help, choices, hide */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj defaultVal = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      FeatherObj choices = ops->string.intern(interp, "", 0);
      int hide = 0;

      while (i < specLen) {
        FeatherObj opt = ops->list.at(interp, specList, i);
        FeatherObj val;

        /* Check for help="..." */
        if ((val = parse_option_value(ops, interp, opt, "help=", 5))) {
          helpText = val;
          i++;
          continue;
        }

        /* Check for default="..." */
        if ((val = parse_option_value(ops, interp, opt, "default=", 8))) {
          defaultVal = val;
          i++;
          continue;
        }

        /* Check for long_help="..." */
        if ((val = parse_option_value(ops, interp, opt, "long_help=", 10))) {
          longHelp = val;
          i++;
          continue;
        }

        /* Check for choices="..." or choices={...} */
        if ((val = parse_option_value(ops, interp, opt, "choices=", 8))) {
          choices = val;
          i++;
          continue;
        }

        /* Check for hide (boolean keyword) */
        if (feather_obj_eq_literal(ops, interp, opt, "hide")) {
          hide = 1;
          i++;
          continue;
        }

        /* Not an option, break out */
        break;
      }

      entry = ops->list.push(interp, entry, helpText);
      entry = ops->list.push(interp, entry, defaultVal);
      entry = ops->list.push(interp, entry, longHelp);
      entry = ops->list.push(interp, entry, choices);
      entry = ops->list.push(interp, entry, ops->integer.create(interp, hide));

      result = ops->list.push(interp, result, entry);

    } else if (feather_obj_eq_literal(ops, interp, keyword, "cmd")) {
      /* cmd "name" { body } - subcommand definition */
      if (i >= specLen) break;

      FeatherObj cmdName = ops->list.at(interp, specList, i);
      i++;

      /* Get the body (next element should be braced body) */
      FeatherObj cmdBody = ops->string.intern(interp, "", 0);
      if (i < specLen) {
        cmdBody = ops->list.at(interp, specList, i);
        i++;
      }

      /* Look for options: help, long_help, hide */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      int hide = 0;

      while (i < specLen) {
        FeatherObj opt = ops->list.at(interp, specList, i);
        FeatherObj val;

        if ((val = parse_option_value(ops, interp, opt, "help=", 5))) {
          helpText = val;
          i++;
          continue;
        }
        if ((val = parse_option_value(ops, interp, opt, "long_help=", 10))) {
          longHelp = val;
          i++;
          continue;
        }
        if (feather_obj_eq_literal(ops, interp, opt, "hide")) {
          hide = 1;
          i++;
          continue;
        }
        break;
      }

      /* Recursively parse the subcommand body */
      FeatherObj subSpec = parse_spec(ops, interp, cmdBody);

      /* Create cmd entry: {cmd name subSpec help long_help hide} */
      FeatherObj entry = ops->list.create(interp);
      entry = ops->list.push(interp, entry, ops->string.intern(interp, "cmd", 3));
      entry = ops->list.push(interp, entry, cmdName);
      entry = ops->list.push(interp, entry, subSpec);
      entry = ops->list.push(interp, entry, helpText);
      entry = ops->list.push(interp, entry, longHelp);
      entry = ops->list.push(interp, entry, ops->integer.create(interp, hide));

      result = ops->list.push(interp, result, entry);

    } else if (feather_obj_eq_literal(ops, interp, keyword, "flag")) {
      /* flag "-s --long" or flag "-s --long <value>" with options */
      if (i >= specLen) break;

      FeatherObj flagSpec = ops->list.at(interp, specList, i);
      i++;

      /* Create flag entry: {flag short long hasValue valueRequired varName help} */
      FeatherObj entry = ops->list.create(interp);
      entry = ops->list.push(interp, entry, ops->string.intern(interp, "flag", 4));

      /* Parse flag spec: "-s", "--long", "-s --long", "-s --long <value>" */
      FeatherObj flagParts = feather_list_parse_obj(ops, interp, flagSpec);
      size_t partsLen = ops->list.length(interp, flagParts);

      FeatherObj shortFlag = ops->string.intern(interp, "", 0);
      FeatherObj longFlag = ops->string.intern(interp, "", 0);
      FeatherObj valueName = ops->string.intern(interp, "", 0);
      int hasValue = 0;
      int valueRequired = 0;

      for (size_t j = 0; j < partsLen; j++) {
        FeatherObj part = ops->list.at(interp, flagParts, j);
        size_t partLen = ops->string.byte_length(interp, part);

        if (partLen >= 1) {
          int c0 = ops->string.byte_at(interp, part, 0);

          if (c0 == '-' && partLen >= 2) {
            int c1 = ops->string.byte_at(interp, part, 1);
            if (c1 == '-' && partLen > 2) {
              /* Long flag: --name */
              longFlag = ops->string.slice(interp, part, 2, partLen);
            } else if (c1 != '-') {
              /* Short flag: -x */
              shortFlag = ops->string.slice(interp, part, 1, partLen);
            }
          } else if (c0 == '<' && partLen >= 2) {
            /* Required value: <name> */
            hasValue = 1;
            valueRequired = 1;
            valueName = ops->string.slice(interp, part, 1, partLen - 1);
          } else if (c0 == '?' && partLen >= 2) {
            /* Optional value: ?name? */
            hasValue = 1;
            valueRequired = 0;
            valueName = ops->string.slice(interp, part, 1, partLen - 1);
          }
        }
      }

      /* Derive variable name from long flag or short flag */
      /* Convert hyphens to underscores for valid TCL variable names */
      FeatherObj varName;
      if (ops->string.byte_length(interp, longFlag) > 0) {
        varName = sanitize_var_name(ops, interp, longFlag);
      } else {
        varName = sanitize_var_name(ops, interp, shortFlag);
      }

      entry = ops->list.push(interp, entry, shortFlag);
      entry = ops->list.push(interp, entry, longFlag);
      entry = ops->list.push(interp, entry, ops->integer.create(interp, hasValue));
      entry = ops->list.push(interp, entry, ops->integer.create(interp, valueRequired));
      entry = ops->list.push(interp, entry, varName);

      /* Look for options: help, long_help, choices, hide */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      FeatherObj choices = ops->string.intern(interp, "", 0);
      int hide = 0;

      while (i < specLen) {
        FeatherObj opt = ops->list.at(interp, specList, i);
        FeatherObj val;

        if ((val = parse_option_value(ops, interp, opt, "help=", 5))) {
          helpText = val;
          i++;
          continue;
        }
        if ((val = parse_option_value(ops, interp, opt, "long_help=", 10))) {
          longHelp = val;
          i++;
          continue;
        }
        if ((val = parse_option_value(ops, interp, opt, "choices=", 8))) {
          choices = val;
          i++;
          continue;
        }
        if (feather_obj_eq_literal(ops, interp, opt, "hide")) {
          hide = 1;
          i++;
          continue;
        }

        break;
      }

      entry = ops->list.push(interp, entry, helpText);
      entry = ops->list.push(interp, entry, longHelp);
      entry = ops->list.push(interp, entry, choices);
      entry = ops->list.push(interp, entry, ops->integer.create(interp, hide));

      result = ops->list.push(interp, result, entry);
    }
  }

  return result;
}

/**
 * Generate usage string for display (--help output)
 *
 * Entry indices:
 *   arg:  {arg(0) name(1) required(2) variadic(3) help(4) default(5) long_help(6) choices(7) hide(8)}
 *   flag: {flag(0) short(1) long(2) hasValue(3) valueRequired(4) varName(5) help(6) long_help(7) choices(8) hide(9)}
 *   cmd:  {cmd(0) name(1) subSpec(2) help(3) long_help(4) hide(5)}
 */
static FeatherObj generate_usage_string(const FeatherHostOps *ops, FeatherInterp interp,
                                         FeatherObj cmdName, FeatherObj parsedSpec) {
  FeatherObj builder = ops->string.builder_new(interp, 256);

  /* Usage: cmdname */
  const char *usage = "Usage: ";
  while (*usage) ops->string.builder_append_byte(interp, builder, *usage++);
  ops->string.builder_append_obj(interp, builder, cmdName);

  /* Add flags summary */
  size_t specLen = ops->list.length(interp, parsedSpec);
  int hasFlags = 0;
  int hasSubcmds = 0;

  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "flag")) {
      /* Check hide flag (index 9) */
      int64_t hide = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 9), &hide);
      if (!hide && !hasFlags) {
        const char *opts = " ?options?";
        while (*opts) ops->string.builder_append_byte(interp, builder, *opts++);
        hasFlags = 1;
      }
    }
    if (feather_obj_eq_literal(ops, interp, type, "cmd")) {
      /* Check hide flag (index 5) */
      int64_t hide = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 5), &hide);
      if (!hide) hasSubcmds = 1;
    }
  }

  /* Add subcommand placeholder if any */
  if (hasSubcmds) {
    const char *subcmd = " <command>";
    while (*subcmd) ops->string.builder_append_byte(interp, builder, *subcmd++);
  }

  /* Add positional args */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
      /* Check hide flag (index 8) */
      int64_t hide = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 8), &hide);
      if (hide) continue;

      FeatherObj name = ops->list.at(interp, entry, 1);
      int64_t required = 0;
      int64_t variadic = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 2), &required);
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &variadic);

      ops->string.builder_append_byte(interp, builder, ' ');

      if (required) {
        ops->string.builder_append_byte(interp, builder, '<');
        ops->string.builder_append_obj(interp, builder, name);
        ops->string.builder_append_byte(interp, builder, '>');
      } else {
        ops->string.builder_append_byte(interp, builder, '?');
        ops->string.builder_append_obj(interp, builder, name);
        ops->string.builder_append_byte(interp, builder, '?');
      }

      if (variadic) {
        const char *dots = "...";
        while (*dots) ops->string.builder_append_byte(interp, builder, *dots++);
      }
    }
  }

  /* Add subcommands section */
  if (hasSubcmds) {
    const char *nl = "\n\nCommands:";
    while (*nl) ops->string.builder_append_byte(interp, builder, *nl++);

    for (size_t i = 0; i < specLen; i++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, i);
      FeatherObj type = ops->list.at(interp, entry, 0);

      if (feather_obj_eq_literal(ops, interp, type, "cmd")) {
        /* Check hide flag (index 5) */
        int64_t hide = 0;
        ops->integer.get(interp, ops->list.at(interp, entry, 5), &hide);
        if (hide) continue;

        FeatherObj name = ops->list.at(interp, entry, 1);
        FeatherObj helpText = ops->list.at(interp, entry, 3);

        ops->string.builder_append_byte(interp, builder, '\n');
        const char *indent = "  ";
        while (*indent) ops->string.builder_append_byte(interp, builder, *indent++);
        ops->string.builder_append_obj(interp, builder, name);

        /* Show help text for subcommand if present */
        if (ops->string.byte_length(interp, helpText) > 0) {
          const char *sep = "  ";
          while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
          ops->string.builder_append_obj(interp, builder, helpText);
        }
      }
    }
  }

  /* Add newlines and details for flags */
  int flagsShown = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "flag")) {
      /* Check hide flag (index 9) */
      int64_t hide = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 9), &hide);
      if (hide) continue;

      if (!flagsShown) {
        const char *nl = "\n\nOptions:";
        while (*nl) ops->string.builder_append_byte(interp, builder, *nl++);
        flagsShown = 1;
      }

      ops->string.builder_append_byte(interp, builder, '\n');
      const char *indent = "  ";
      while (*indent) ops->string.builder_append_byte(interp, builder, *indent++);

      FeatherObj shortFlag = ops->list.at(interp, entry, 1);
      FeatherObj longFlag = ops->list.at(interp, entry, 2);
      int64_t hasValue = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &hasValue);
      FeatherObj varName = ops->list.at(interp, entry, 5);
      FeatherObj helpText = ops->list.at(interp, entry, 6);
      FeatherObj choices = ops->list.at(interp, entry, 8);

      if (ops->string.byte_length(interp, shortFlag) > 0) {
        ops->string.builder_append_byte(interp, builder, '-');
        ops->string.builder_append_obj(interp, builder, shortFlag);
        if (ops->string.byte_length(interp, longFlag) > 0) {
          const char *sep = ", ";
          while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
        }
      }

      if (ops->string.byte_length(interp, longFlag) > 0) {
        const char *dd = "--";
        while (*dd) ops->string.builder_append_byte(interp, builder, *dd++);
        ops->string.builder_append_obj(interp, builder, longFlag);
      }

      if (hasValue) {
        ops->string.builder_append_byte(interp, builder, ' ');
        ops->string.builder_append_byte(interp, builder, '<');
        ops->string.builder_append_obj(interp, builder, varName);
        ops->string.builder_append_byte(interp, builder, '>');
      }

      if (ops->string.byte_length(interp, helpText) > 0) {
        const char *sep = "  ";
        while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
        ops->string.builder_append_obj(interp, builder, helpText);
      }

      /* Show choices if present */
      if (ops->string.byte_length(interp, choices) > 0) {
        const char *ch = " [choices: ";
        while (*ch) ops->string.builder_append_byte(interp, builder, *ch++);
        ops->string.builder_append_obj(interp, builder, choices);
        ops->string.builder_append_byte(interp, builder, ']');
      }
    }
  }

  /* Add details for args */
  int argsShown = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
      /* Check hide flag (index 8) */
      int64_t hide = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 8), &hide);
      if (hide) continue;

      FeatherObj helpText = ops->list.at(interp, entry, 4);
      FeatherObj choices = ops->list.at(interp, entry, 7);

      /* Show arg if it has help text or choices */
      if (ops->string.byte_length(interp, helpText) > 0 ||
          ops->string.byte_length(interp, choices) > 0) {
        if (!argsShown) {
          const char *nl = "\n\nArguments:";
          while (*nl) ops->string.builder_append_byte(interp, builder, *nl++);
          argsShown = 1;
        }

        ops->string.builder_append_byte(interp, builder, '\n');
        const char *indent = "  ";
        while (*indent) ops->string.builder_append_byte(interp, builder, *indent++);

        FeatherObj name = ops->list.at(interp, entry, 1);
        ops->string.builder_append_obj(interp, builder, name);

        if (ops->string.byte_length(interp, helpText) > 0) {
          const char *sep = "  ";
          while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
          ops->string.builder_append_obj(interp, builder, helpText);
        }

        /* Show choices if present */
        if (ops->string.byte_length(interp, choices) > 0) {
          const char *ch = " [choices: ";
          while (*ch) ops->string.builder_append_byte(interp, builder, *ch++);
          ops->string.builder_append_obj(interp, builder, choices);
          ops->string.builder_append_byte(interp, builder, ']');
        }
      }
    }
  }

  return ops->string.builder_finish(interp, builder);
}

/**
 * usage for command ?spec?
 *
 * Define or get usage spec for a command.
 * If spec is provided, stores it. Otherwise returns the stored spec.
 */
static FeatherResult usage_for(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc < 1 || argc > 2) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"usage for command ?spec?\"", 50));
    return TCL_ERROR;
  }

  FeatherObj cmdName = ops->list.at(interp, args, 0);
  FeatherObj specs = usage_get_specs(ops, interp);

  if (argc == 1) {
    /* Get mode: return the raw spec string for this command */
    FeatherObj specEntry = ops->dict.get(interp, specs, cmdName);
    if (ops->list.is_nil(interp, specEntry)) {
      FeatherObj msg = ops->string.intern(interp, "no usage defined for \"", 22);
      msg = ops->string.concat(interp, msg, cmdName);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    /* Return just the original spec string, not the parsed form */
    FeatherObj rawSpec = ops->list.at(interp, specEntry, 0);
    ops->interp.set_result(interp, rawSpec);
    return TCL_OK;
  }

  /* Set mode: store the spec */
  FeatherObj specStr = ops->list.at(interp, args, 1);

  /* Parse the spec into structured form */
  FeatherObj parsed = parse_spec(ops, interp, specStr);

  /* Store both the original string and parsed form */
  FeatherObj entry = ops->list.create(interp);
  entry = ops->list.push(interp, entry, specStr);
  entry = ops->list.push(interp, entry, parsed);

  specs = ops->dict.set(interp, specs, cmdName, entry);
  usage_set_specs(ops, interp, specs);

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

/**
 * Check if a parsed spec has any subcommand definitions.
 */
static int spec_has_subcommands(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj parsedSpec) {
  size_t specLen = ops->list.length(interp, parsedSpec);
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);
    if (feather_obj_eq_literal(ops, interp, type, "cmd")) {
      return 1;
    }
  }
  return 0;
}

/**
 * Build "missing subcommand" error message listing available subcommands.
 */
static FeatherObj build_missing_subcmd_error(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj parsedSpec) {
  FeatherObj builder = ops->string.builder_new(interp, 64);
  const char *prefix = "missing subcommand: must be ";
  while (*prefix) ops->string.builder_append_byte(interp, builder, *prefix++);

  size_t specLen = ops->list.length(interp, parsedSpec);
  int first = 1;
  int count = 0;

  /* Count subcommands first */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);
    if (feather_obj_eq_literal(ops, interp, type, "cmd")) {
      count++;
    }
  }

  /* Build list */
  int idx = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);
    if (feather_obj_eq_literal(ops, interp, type, "cmd")) {
      FeatherObj name = ops->list.at(interp, entry, 1);
      if (!first) {
        if (idx == count - 1) {
          const char *sep = " or ";
          while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
        } else {
          const char *sep = ", ";
          while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
        }
      }
      ops->string.builder_append_obj(interp, builder, name);
      first = 0;
      idx++;
    }
  }

  return ops->string.builder_finish(interp, builder);
}

/**
 * Initialize variables from a spec (flags to defaults, args to empty/defaults).
 */
static void init_spec_vars(const FeatherHostOps *ops, FeatherInterp interp,
                            FeatherObj parsedSpec) {
  size_t specLen = ops->list.length(interp, parsedSpec);

  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
      FeatherObj name = ops->list.at(interp, entry, 1);
      FeatherObj defaultVal = ops->list.at(interp, entry, 5);
      int64_t variadic = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &variadic);

      if (variadic) {
        ops->var.set(interp, name, ops->list.create(interp));
      } else {
        ops->var.set(interp, name, defaultVal);
      }
    } else if (feather_obj_eq_literal(ops, interp, type, "flag")) {
      FeatherObj varName = ops->list.at(interp, entry, 5);
      int64_t hasValue = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &hasValue);

      if (hasValue) {
        ops->var.set(interp, varName, ops->string.intern(interp, "", 0));
      } else {
        ops->var.set(interp, varName, ops->integer.create(interp, 0));
      }
    } else if (feather_obj_eq_literal(ops, interp, type, "cmd")) {
      /* Recursively init subcommand vars */
      FeatherObj subSpec = ops->list.at(interp, entry, 2);
      init_spec_vars(ops, interp, subSpec);
    }
  }
}

/**
 * Try to match a flag in a list of specs (for handling flags from multiple levels).
 * Returns 1 if matched, 0 if not found, -1 on error.
 * If matched and flag takes a value, *argIdxPtr is advanced.
 */
static int try_match_flag(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj specs[], int numSpecs,
                           FeatherObj arg, FeatherObj argsListParsed,
                           size_t *argIdxPtr, size_t argsLen) {
  size_t argLen = ops->string.byte_length(interp, arg);
  int isLong = (argLen >= 2 && ops->string.byte_at(interp, arg, 1) == '-');

  for (int s = 0; s < numSpecs; s++) {
    FeatherObj parsedSpec = specs[s];
    size_t specLen = ops->list.length(interp, parsedSpec);

    for (size_t i = 0; i < specLen; i++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, i);
      FeatherObj type = ops->list.at(interp, entry, 0);

      if (!feather_obj_eq_literal(ops, interp, type, "flag")) continue;

      FeatherObj shortFlag = ops->list.at(interp, entry, 1);
      FeatherObj longFlag = ops->list.at(interp, entry, 2);
      int64_t hasValue = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &hasValue);
      FeatherObj varName = ops->list.at(interp, entry, 5);

      if (isLong) {
        FeatherObj flagName = ops->string.slice(interp, arg, 2, argLen);
        size_t flagNameLen = ops->string.byte_length(interp, flagName);

        long eqPos = -1;
        for (size_t j = 0; j < flagNameLen; j++) {
          if (ops->string.byte_at(interp, flagName, j) == '=') {
            eqPos = (long)j;
            break;
          }
        }

        FeatherObj cmpName;
        FeatherObj inlineValue = 0;
        if (eqPos >= 0) {
          cmpName = ops->string.slice(interp, flagName, 0, (size_t)eqPos);
          inlineValue = ops->string.slice(interp, flagName, (size_t)eqPos + 1, flagNameLen);
        } else {
          cmpName = flagName;
        }

        if (ops->string.equal(interp, cmpName, longFlag)) {
          if (hasValue) {
            if (inlineValue) {
              ops->var.set(interp, varName, inlineValue);
            } else if (*argIdxPtr + 1 < argsLen) {
              (*argIdxPtr)++;
              ops->var.set(interp, varName, ops->list.at(interp, argsListParsed, *argIdxPtr));
            } else {
              FeatherObj msg = ops->string.intern(interp, "flag --", 7);
              msg = ops->string.concat(interp, msg, longFlag);
              msg = ops->string.concat(interp, msg, ops->string.intern(interp, " requires a value", 17));
              ops->interp.set_result(interp, msg);
              return -1;
            }
          } else {
            ops->var.set(interp, varName, ops->integer.create(interp, 1));
          }
          return 1;
        }
      } else {
        FeatherObj flagChar = ops->string.slice(interp, arg, 1, argLen);

        if (ops->string.equal(interp, flagChar, shortFlag)) {
          if (hasValue) {
            if (*argIdxPtr + 1 < argsLen) {
              (*argIdxPtr)++;
              ops->var.set(interp, varName, ops->list.at(interp, argsListParsed, *argIdxPtr));
            } else {
              FeatherObj msg = ops->string.intern(interp, "flag -", 6);
              msg = ops->string.concat(interp, msg, shortFlag);
              msg = ops->string.concat(interp, msg, ops->string.intern(interp, " requires a value", 17));
              ops->interp.set_result(interp, msg);
              return -1;
            }
          } else {
            ops->var.set(interp, varName, ops->integer.create(interp, 1));
          }
          return 1;
        }
      }
    }
  }

  return 0;
}

/**
 * usage parse command argsList
 *
 * Parse arguments according to the usage spec and create local variables.
 * Supports nested subcommands up to 8 levels deep.
 */
static FeatherResult usage_parse(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"usage parse command args\"", 50));
    return TCL_ERROR;
  }

  FeatherObj cmdName = ops->list.at(interp, args, 0);
  FeatherObj argsList = ops->list.at(interp, args, 1);

  /* Get the spec */
  FeatherObj specs = usage_get_specs(ops, interp);
  FeatherObj specEntry = ops->dict.get(interp, specs, cmdName);

  if (ops->list.is_nil(interp, specEntry)) {
    FeatherObj msg = ops->string.intern(interp, "no usage defined for \"", 22);
    msg = ops->string.concat(interp, msg, cmdName);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj parsedSpec = ops->list.at(interp, specEntry, 1);

  /* Parse the args list */
  FeatherObj argsListParsed = ops->list.from(interp, argsList);
  size_t argsLen = ops->list.length(interp, argsListParsed);

  /* Initialize all variables (including nested subcommand vars) to defaults */
  init_spec_vars(ops, interp, parsedSpec);

  /* Track spec stack for nested subcommands (up to 8 levels) */
  FeatherObj specStack[8];
  int specDepth = 0;
  specStack[specDepth++] = parsedSpec;

  /* Track subcommand path */
  FeatherObj subcmdPath = ops->list.create(interp);

  /* Process arguments */
  size_t argIdx = 0;
  size_t posArgIdx = 0;  /* Index into positional args in current spec */
  FeatherObj variadicList = 0;
  FeatherObj variadicName = 0;
  int flagsEnded = 0;

  /* Get current active spec (deepest in stack) */
  FeatherObj activeSpec = specStack[specDepth - 1];
  size_t activeSpecLen = ops->list.length(interp, activeSpec);

  while (argIdx < argsLen) {
    FeatherObj arg = ops->list.at(interp, argsListParsed, argIdx);
    size_t argLen = ops->string.byte_length(interp, arg);

    /* Check if it's a flag */
    if (!flagsEnded && argLen >= 1 && ops->string.byte_at(interp, arg, 0) == '-') {
      /* Check for -- (end of flags) */
      if (argLen == 2 && ops->string.byte_at(interp, arg, 1) == '-') {
        argIdx++;
        flagsEnded = 1;
        continue;
      }

      /* Try to match flag in all active spec levels */
      int result = try_match_flag(ops, interp, specStack, specDepth,
                                   arg, argsListParsed, &argIdx, argsLen);
      if (result == -1) {
        return TCL_ERROR;  /* Error already set */
      }
      if (result == 0) {
        FeatherObj msg = ops->string.intern(interp, "unknown flag \"", 14);
        msg = ops->string.concat(interp, msg, arg);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      argIdx++;
      continue;
    }

    /* Positional argument - first check if it matches a subcommand */
    int foundSubcmd = 0;
    if (spec_has_subcommands(ops, interp, activeSpec)) {
      for (size_t i = 0; i < activeSpecLen; i++) {
        FeatherObj entry = ops->list.at(interp, activeSpec, i);
        FeatherObj type = ops->list.at(interp, entry, 0);

        if (!feather_obj_eq_literal(ops, interp, type, "cmd")) continue;

        FeatherObj subcmdName = ops->list.at(interp, entry, 1);
        if (ops->string.equal(interp, arg, subcmdName)) {
          /* Found matching subcommand */
          foundSubcmd = 1;
          subcmdPath = ops->list.push(interp, subcmdPath, subcmdName);

          /* Descend into subcommand spec */
          FeatherObj subSpec = ops->list.at(interp, entry, 2);
          if (specDepth < 8) {
            specStack[specDepth++] = subSpec;
          }
          activeSpec = subSpec;
          activeSpecLen = ops->list.length(interp, activeSpec);
          posArgIdx = 0;  /* Reset positional arg index for new spec */
          variadicList = 0;
          variadicName = 0;

          argIdx++;
          break;
        }
      }

      if (!foundSubcmd && !flagsEnded) {
        /* Might still be a flag that looks like a positional, or unknown subcommand */
        /* Check if there are any args in this spec - if not, it's definitely a subcommand error */
        int hasArgs = 0;
        for (size_t i = 0; i < activeSpecLen; i++) {
          FeatherObj entry = ops->list.at(interp, activeSpec, i);
          FeatherObj type = ops->list.at(interp, entry, 0);
          if (feather_obj_eq_literal(ops, interp, type, "arg")) {
            hasArgs = 1;
            break;
          }
        }
        if (!hasArgs) {
          /* No args defined, so this must be an unknown subcommand */
          FeatherObj msg = ops->string.intern(interp, "unknown subcommand \"", 20);
          msg = ops->string.concat(interp, msg, arg);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
      }
    }

    if (foundSubcmd) {
      continue;
    }

    /* Not a subcommand - treat as positional argument */
    int foundPos = 0;
    for (size_t i = posArgIdx; i < activeSpecLen && !foundPos; i++) {
      FeatherObj entry = ops->list.at(interp, activeSpec, i);
      FeatherObj type = ops->list.at(interp, entry, 0);

      if (!feather_obj_eq_literal(ops, interp, type, "arg")) continue;

      FeatherObj name = ops->list.at(interp, entry, 1);
      int64_t variadic = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &variadic);

      foundPos = 1;
      posArgIdx = i + 1;

      if (variadic) {
        variadicList = ops->list.create(interp);
        variadicList = ops->list.push(interp, variadicList, arg);
        variadicName = name;
        posArgIdx = activeSpecLen;
      } else {
        ops->var.set(interp, name, arg);
      }
    }

    if (!foundPos && variadicName) {
      variadicList = ops->list.push(interp, variadicList, arg);
    } else if (!foundPos) {
      /* Check if this was supposed to be a subcommand */
      if (spec_has_subcommands(ops, interp, activeSpec)) {
        FeatherObj msg = ops->string.intern(interp, "unknown subcommand \"", 20);
        msg = ops->string.concat(interp, msg, arg);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      FeatherObj msg = ops->string.intern(interp, "unexpected argument \"", 21);
      msg = ops->string.concat(interp, msg, arg);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    argIdx++;
  }

  /* Store variadic list if collected */
  if (variadicName) {
    ops->var.set(interp, variadicName, variadicList);
  }

  /* Check if subcommand was required but not provided */
  if (spec_has_subcommands(ops, interp, activeSpec) && ops->list.length(interp, subcmdPath) == 0) {
    /* Check if we're at root and subcommands exist */
    if (specDepth == 1 && spec_has_subcommands(ops, interp, parsedSpec)) {
      FeatherObj msg = build_missing_subcmd_error(ops, interp, parsedSpec);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  /* Set $_cmd variable with subcommand path */
  FeatherObj cmdVar = ops->string.intern(interp, "_cmd", 4);
  if (ops->list.length(interp, subcmdPath) > 0) {
    /* Build space-separated subcommand path */
    FeatherObj pathStr = ops->string.builder_new(interp, 32);
    size_t pathLen = ops->list.length(interp, subcmdPath);
    for (size_t i = 0; i < pathLen; i++) {
      if (i > 0) ops->string.builder_append_byte(interp, pathStr, ' ');
      ops->string.builder_append_obj(interp, pathStr, ops->list.at(interp, subcmdPath, i));
    }
    ops->var.set(interp, cmdVar, ops->string.builder_finish(interp, pathStr));
  } else {
    ops->var.set(interp, cmdVar, ops->string.intern(interp, "", 0));
  }

  /* Check required args were provided (in the active spec) */
  for (size_t i = 0; i < activeSpecLen; i++) {
    FeatherObj entry = ops->list.at(interp, activeSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
      FeatherObj name = ops->list.at(interp, entry, 1);
      int64_t required = 0;
      int64_t variadic = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 2), &required);
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &variadic);

      if (required) {
        FeatherObj value = ops->var.get(interp, name);
        if (ops->list.is_nil(interp, value)) {
          FeatherObj msg = ops->string.intern(interp, "missing required argument \"", 27);
          msg = ops->string.concat(interp, msg, name);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        if (variadic && ops->list.length(interp, value) == 0) {
          FeatherObj msg = ops->string.intern(interp, "missing required argument \"", 27);
          msg = ops->string.concat(interp, msg, name);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
        size_t valLen = ops->string.byte_length(interp, value);
        if (!variadic && valLen == 0 && i < posArgIdx) {
          continue;
        }
        if (!variadic && valLen == 0 && i >= posArgIdx) {
          FeatherObj msg = ops->string.intern(interp, "missing required argument \"", 27);
          msg = ops->string.concat(interp, msg, name);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
      }
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

/**
 * usage help command ?subcommand...?
 *
 * Generate help text for a command based on its usage spec.
 * Can take optional subcommand path to show help for specific subcommand.
 */
static FeatherResult usage_help(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"usage help command ?subcommand...?\"", 60));
    return TCL_ERROR;
  }

  FeatherObj cmdName = ops->list.at(interp, args, 0);

  /* Get the spec */
  FeatherObj specs = usage_get_specs(ops, interp);
  FeatherObj specEntry = ops->dict.get(interp, specs, cmdName);

  if (ops->list.is_nil(interp, specEntry)) {
    FeatherObj msg = ops->string.intern(interp, "no usage defined for \"", 22);
    msg = ops->string.concat(interp, msg, cmdName);
    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj parsedSpec = ops->list.at(interp, specEntry, 1);

  /* Build full command name and navigate to subcommand spec */
  FeatherObj fullCmdName = cmdName;

  for (size_t i = 1; i < argc; i++) {
    FeatherObj subcmdName = ops->list.at(interp, args, i);
    int found = 0;

    size_t specLen = ops->list.length(interp, parsedSpec);
    for (size_t j = 0; j < specLen; j++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, j);
      FeatherObj type = ops->list.at(interp, entry, 0);

      if (!feather_obj_eq_literal(ops, interp, type, "cmd")) continue;

      FeatherObj name = ops->list.at(interp, entry, 1);
      if (ops->string.equal(interp, name, subcmdName)) {
        /* Found the subcommand - descend into it */
        parsedSpec = ops->list.at(interp, entry, 2);
        fullCmdName = ops->string.concat(interp, fullCmdName, ops->string.intern(interp, " ", 1));
        fullCmdName = ops->string.concat(interp, fullCmdName, subcmdName);
        found = 1;
        break;
      }
    }

    if (!found) {
      FeatherObj msg = ops->string.intern(interp, "unknown subcommand \"", 20);
      msg = ops->string.concat(interp, msg, subcmdName);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  FeatherObj helpStr = generate_usage_string(ops, interp, fullCmdName, parsedSpec);

  ops->interp.set_result(interp, helpStr);
  return TCL_OK;
}

/**
 * Main usage command dispatcher
 *
 * Usage:
 *   usage for command ?spec?   - define or get usage spec
 *   usage parse command args   - parse args and set local vars
 *   usage help command         - generate help text
 */
FeatherResult feather_builtin_usage(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc == 0) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"usage subcommand ?arg ...?\"", 52));
    return TCL_ERROR;
  }

  /* Ensure ::tcl::usage namespace exists */
  FeatherObj usageNs = ops->string.intern(interp, USAGE_NS, 12);
  ops->ns.create(interp, usageNs);

  FeatherObj subcmd = ops->list.shift(interp, args);

  if (feather_obj_eq_literal(ops, interp, subcmd, "for")) {
    return usage_for(ops, interp, args);
  }

  if (feather_obj_eq_literal(ops, interp, subcmd, "parse")) {
    return usage_parse(ops, interp, args);
  }

  if (feather_obj_eq_literal(ops, interp, subcmd, "help")) {
    return usage_help(ops, interp, args);
  }

  /* Unknown subcommand */
  FeatherObj msg = ops->string.intern(interp, "unknown subcommand \"", 20);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\": must be for, help, or parse", 30));
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
