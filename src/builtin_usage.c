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
 * Parse a spec string into a structured representation.
 *
 * The spec is a script-like format with 'arg' and 'flag' declarations.
 * Returns a list of {type details...} entries.
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

      /* Look for options: help="..." default="..." */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj defaultVal = ops->string.intern(interp, "", 0);

      while (i < specLen) {
        FeatherObj opt = ops->list.at(interp, specList, i);
        size_t optLen = ops->string.byte_length(interp, opt);

        /* Check for help="..." */
        if (optLen > 5 && ops->string.byte_at(interp, opt, 0) == 'h' &&
            ops->string.byte_at(interp, opt, 1) == 'e' &&
            ops->string.byte_at(interp, opt, 2) == 'l' &&
            ops->string.byte_at(interp, opt, 3) == 'p' &&
            ops->string.byte_at(interp, opt, 4) == '=') {
          /* Extract value after help= */
          FeatherObj val = ops->string.slice(interp, opt, 5, optLen);
          /* Strip quotes if present */
          size_t valLen = ops->string.byte_length(interp, val);
          if (valLen >= 2 && ops->string.byte_at(interp, val, 0) == '"' &&
              ops->string.byte_at(interp, val, valLen - 1) == '"') {
            helpText = ops->string.slice(interp, val, 1, valLen - 1);
          } else {
            helpText = val;
          }
          i++;
          continue;
        }

        /* Check for default="..." */
        if (optLen > 8 && ops->string.byte_at(interp, opt, 0) == 'd' &&
            ops->string.byte_at(interp, opt, 1) == 'e' &&
            ops->string.byte_at(interp, opt, 2) == 'f' &&
            ops->string.byte_at(interp, opt, 3) == 'a' &&
            ops->string.byte_at(interp, opt, 4) == 'u' &&
            ops->string.byte_at(interp, opt, 5) == 'l' &&
            ops->string.byte_at(interp, opt, 6) == 't' &&
            ops->string.byte_at(interp, opt, 7) == '=') {
          FeatherObj val = ops->string.slice(interp, opt, 8, optLen);
          size_t valLen = ops->string.byte_length(interp, val);
          if (valLen >= 2 && ops->string.byte_at(interp, val, 0) == '"' &&
              ops->string.byte_at(interp, val, valLen - 1) == '"') {
            defaultVal = ops->string.slice(interp, val, 1, valLen - 1);
          } else {
            defaultVal = val;
          }
          i++;
          continue;
        }

        /* Not an option, break out */
        break;
      }

      entry = ops->list.push(interp, entry, helpText);
      entry = ops->list.push(interp, entry, defaultVal);

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
      FeatherObj varName;
      if (ops->string.byte_length(interp, longFlag) > 0) {
        varName = longFlag;
      } else {
        varName = shortFlag;
      }

      entry = ops->list.push(interp, entry, shortFlag);
      entry = ops->list.push(interp, entry, longFlag);
      entry = ops->list.push(interp, entry, ops->integer.create(interp, hasValue));
      entry = ops->list.push(interp, entry, ops->integer.create(interp, valueRequired));
      entry = ops->list.push(interp, entry, varName);

      /* Look for options: help="..." */
      FeatherObj helpText = ops->string.intern(interp, "", 0);

      while (i < specLen) {
        FeatherObj opt = ops->list.at(interp, specList, i);
        size_t optLen = ops->string.byte_length(interp, opt);

        if (optLen > 5 && ops->string.byte_at(interp, opt, 0) == 'h' &&
            ops->string.byte_at(interp, opt, 1) == 'e' &&
            ops->string.byte_at(interp, opt, 2) == 'l' &&
            ops->string.byte_at(interp, opt, 3) == 'p' &&
            ops->string.byte_at(interp, opt, 4) == '=') {
          FeatherObj val = ops->string.slice(interp, opt, 5, optLen);
          size_t valLen = ops->string.byte_length(interp, val);
          if (valLen >= 2 && ops->string.byte_at(interp, val, 0) == '"' &&
              ops->string.byte_at(interp, val, valLen - 1) == '"') {
            helpText = ops->string.slice(interp, val, 1, valLen - 1);
          } else {
            helpText = val;
          }
          i++;
          continue;
        }

        break;
      }

      entry = ops->list.push(interp, entry, helpText);

      result = ops->list.push(interp, result, entry);
    }
  }

  return result;
}

/**
 * Generate usage string for display (--help output)
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

  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "flag")) {
      if (!hasFlags) {
        const char *opts = " ?options?";
        while (*opts) ops->string.builder_append_byte(interp, builder, *opts++);
        hasFlags = 1;
      }
    }
  }

  /* Add positional args */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
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

  /* Add newlines and details for flags */
  int flagsShown = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "flag")) {
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
    }
  }

  /* Add details for args */
  int argsShown = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
      FeatherObj helpText = ops->list.at(interp, entry, 4);

      if (ops->string.byte_length(interp, helpText) > 0) {
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
        const char *sep = "  ";
        while (*sep) ops->string.builder_append_byte(interp, builder, *sep++);
        ops->string.builder_append_obj(interp, builder, helpText);
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
    /* Get mode: return the spec for this command */
    FeatherObj spec = ops->dict.get(interp, specs, cmdName);
    if (ops->list.is_nil(interp, spec)) {
      FeatherObj msg = ops->string.intern(interp, "no usage defined for \"", 22);
      msg = ops->string.concat(interp, msg, cmdName);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    ops->interp.set_result(interp, spec);
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
 * usage parse command argsList
 *
 * Parse arguments according to the usage spec and create local variables.
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
  size_t specLen = ops->list.length(interp, parsedSpec);

  /* Initialize all variables to defaults first */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj type = ops->list.at(interp, entry, 0);

    if (feather_obj_eq_literal(ops, interp, type, "arg")) {
      FeatherObj name = ops->list.at(interp, entry, 1);
      FeatherObj defaultVal = ops->list.at(interp, entry, 5);
      int64_t variadic = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &variadic);

      if (variadic) {
        /* Variadic args default to empty list */
        ops->var.set(interp, name, ops->list.create(interp));
      } else {
        ops->var.set(interp, name, defaultVal);
      }
    } else if (feather_obj_eq_literal(ops, interp, type, "flag")) {
      FeatherObj varName = ops->list.at(interp, entry, 5);
      int64_t hasValue = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &hasValue);

      if (hasValue) {
        /* Flag with value defaults to empty string */
        ops->var.set(interp, varName, ops->string.intern(interp, "", 0));
      } else {
        /* Boolean flag defaults to 0 */
        ops->var.set(interp, varName, ops->integer.create(interp, 0));
      }
    }
  }

  /* Process arguments */
  size_t argIdx = 0;
  size_t posArgIdx = 0;  /* Index into positional args in spec */
  FeatherObj variadicList = 0;  /* For collecting variadic args */
  FeatherObj variadicName = 0;

  while (argIdx < argsLen) {
    FeatherObj arg = ops->list.at(interp, argsListParsed, argIdx);
    size_t argLen = ops->string.byte_length(interp, arg);

    /* Check if it's a flag (starts with -) */
    if (argLen >= 1 && ops->string.byte_at(interp, arg, 0) == '-') {
      /* Check for -- (end of flags) */
      if (argLen == 2 && ops->string.byte_at(interp, arg, 1) == '-') {
        argIdx++;
        /* Remaining args are all positional */
        continue;
      }

      /* Look for matching flag */
      int found = 0;
      int isLong = (argLen >= 2 && ops->string.byte_at(interp, arg, 1) == '-');

      for (size_t i = 0; i < specLen && !found; i++) {
        FeatherObj entry = ops->list.at(interp, parsedSpec, i);
        FeatherObj type = ops->list.at(interp, entry, 0);

        if (!feather_obj_eq_literal(ops, interp, type, "flag")) continue;

        FeatherObj shortFlag = ops->list.at(interp, entry, 1);
        FeatherObj longFlag = ops->list.at(interp, entry, 2);
        int64_t hasValue = 0;
        ops->integer.get(interp, ops->list.at(interp, entry, 3), &hasValue);
        FeatherObj varName = ops->list.at(interp, entry, 5);

        if (isLong) {
          /* Check long flag: --name or --name=value */
          FeatherObj flagName = ops->string.slice(interp, arg, 2, argLen);
          size_t flagNameLen = ops->string.byte_length(interp, flagName);

          /* Check for = in flag (--flag=value) */
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
            found = 1;
            if (hasValue) {
              if (inlineValue) {
                ops->var.set(interp, varName, inlineValue);
              } else if (argIdx + 1 < argsLen) {
                argIdx++;
                ops->var.set(interp, varName, ops->list.at(interp, argsListParsed, argIdx));
              } else {
                FeatherObj msg = ops->string.intern(interp, "flag --", 7);
                msg = ops->string.concat(interp, msg, longFlag);
                msg = ops->string.concat(interp, msg, ops->string.intern(interp, " requires a value", 17));
                ops->interp.set_result(interp, msg);
                return TCL_ERROR;
              }
            } else {
              ops->var.set(interp, varName, ops->integer.create(interp, 1));
            }
          }
        } else {
          /* Check short flag: -x or -x value */
          FeatherObj flagChar = ops->string.slice(interp, arg, 1, argLen);

          if (ops->string.equal(interp, flagChar, shortFlag)) {
            found = 1;
            if (hasValue) {
              if (argIdx + 1 < argsLen) {
                argIdx++;
                ops->var.set(interp, varName, ops->list.at(interp, argsListParsed, argIdx));
              } else {
                FeatherObj msg = ops->string.intern(interp, "flag -", 6);
                msg = ops->string.concat(interp, msg, shortFlag);
                msg = ops->string.concat(interp, msg, ops->string.intern(interp, " requires a value", 17));
                ops->interp.set_result(interp, msg);
                return TCL_ERROR;
              }
            } else {
              ops->var.set(interp, varName, ops->integer.create(interp, 1));
            }
          }
        }
      }

      if (!found) {
        FeatherObj msg = ops->string.intern(interp, "unknown flag \"", 14);
        msg = ops->string.concat(interp, msg, arg);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      argIdx++;
      continue;
    }

    /* Positional argument - find next positional spec */
    int foundPos = 0;
    for (size_t i = posArgIdx; i < specLen && !foundPos; i++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, i);
      FeatherObj type = ops->list.at(interp, entry, 0);

      if (!feather_obj_eq_literal(ops, interp, type, "arg")) continue;

      FeatherObj name = ops->list.at(interp, entry, 1);
      int64_t variadic = 0;
      ops->integer.get(interp, ops->list.at(interp, entry, 3), &variadic);

      foundPos = 1;
      posArgIdx = i + 1;

      if (variadic) {
        /* Start collecting variadic args */
        variadicList = ops->list.create(interp);
        variadicList = ops->list.push(interp, variadicList, arg);
        variadicName = name;
        posArgIdx = specLen;  /* No more positional specs to consume */
      } else {
        ops->var.set(interp, name, arg);
      }
    }

    if (!foundPos && variadicName) {
      /* Add to variadic list */
      variadicList = ops->list.push(interp, variadicList, arg);
    } else if (!foundPos) {
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

  /* Check required args were provided */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
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
          continue;  /* Value was set */
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
 * usage help command
 *
 * Generate help text for a command based on its usage spec.
 */
static FeatherResult usage_help(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    ops->interp.set_result(
        interp,
        ops->string.intern(interp, "wrong # args: should be \"usage help command\"", 45));
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
  FeatherObj helpStr = generate_usage_string(ops, interp, cmdName, parsedSpec);

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
