#include "feather.h"
#include "internal.h"

#define S(lit) (lit), feather_strlen(lit)

/**
 * builtin_usage.c - Implements the 'usage' command for CLI argument parsing.
 *
 * Based on the usage specification from https://usage.jdx.dev
 *
 * TCL Interface:
 *   usage for $command ?spec?   - Define or get usage spec for a command
 *   usage parse $command $args  - Parse args list and create local variables
 *
 * Spec Format (TCL-native block syntax):
 *   arg <name>                  - Required positional argument
 *   arg ?name?                  - Optional positional argument
 *   arg <name>...               - Variadic required (1 or more)
 *   arg ?name?...               - Variadic optional (0 or more)
 *   flag -s --long              - Boolean flag (short and/or long)
 *   flag -s --long <value>      - Flag with required value
 *   flag -s --long ?value?      - Flag with optional value
 *
 * Options block for arg/flag (follows the declaration):
 *   {
 *     help {description}
 *     long_help {extended description}
 *     choices {a b c}
 *     default {value}           (arg only)
 *     type {typename}           (e.g., script, file, dir)
 *     hide
 *   }
 *
 * Internal Entry Format (dicts):
 *   arg:  {type arg name <n> required 0|1 variadic 0|1 help <t> default <v>
 *          long_help <t> choices {a b} hide 0|1 value_type <t>}
 *   flag: {type flag short <s> long <l> has_value 0|1 value_required 0|1
 *          var_name <n> help <t> long_help <t> choices {a b} hide 0|1 value_type <t>}
 *   cmd:  {type cmd name <n> spec <entries> help <t> long_help <t> hide 0|1}
 *
 * Note: Uses ?arg? instead of [arg] for optional args because []
 * triggers command substitution in TCL.
 */

/* Storage namespace for usage specs: ::usage */
static const char *USAGE_NS = "::usage";

/* Dict keys */
static const char *K_TYPE       = "type";
static const char *K_NAME       = "name";
static const char *K_REQUIRED   = "required";
static const char *K_VARIADIC   = "variadic";
static const char *K_HELP       = "help";
static const char *K_DEFAULT    = "default";
static const char *K_LONG_HELP  = "long_help";
static const char *K_CHOICES    = "choices";
static const char *K_HIDE       = "hide";
static const char *K_VALUE_TYPE = "value_type";
static const char *K_SHORT      = "short";
static const char *K_LONG       = "long";
static const char *K_HAS_VALUE  = "has_value";
static const char *K_VALUE_REQ  = "value_required";
static const char *K_VAR_NAME   = "var_name";
static const char *K_SPEC       = "spec";
static const char *K_ORIG       = "orig";  /* Original spec string for round-tripping */

/* Example entry keys */
static const char *K_CODE       = "code";
static const char *K_HEADER     = "header";
static const char *K_EXAMPLES   = "examples";

/* Before/after help keys */
static const char *K_BEFORE_HELP      = "before_help";
static const char *K_AFTER_HELP       = "after_help";
static const char *K_BEFORE_LONG_HELP = "before_long_help";
static const char *K_AFTER_LONG_HELP  = "after_long_help";

/* Entry type values */
static const char *T_ARG     = "arg";
static const char *T_FLAG    = "flag";
static const char *T_CMD     = "cmd";
static const char *T_EXAMPLE = "example";

/**
 * Helper to get a string key from a dict, returning empty string if not found.
 */
static FeatherObj dict_get_str(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj dict, const char *key) {
  FeatherObj k = ops->string.intern(interp, key, feather_strlen(key));
  FeatherObj v = ops->dict.get(interp, dict, k);
  if (ops->list.is_nil(interp, v)) {
    return ops->string.intern(interp, "", 0);
  }
  return v;
}

/**
 * Helper to get an int key from a dict, returning 0 if not found.
 */
static int64_t dict_get_int(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj dict, const char *key) {
  FeatherObj k = ops->string.intern(interp, key, feather_strlen(key));
  FeatherObj v = ops->dict.get(interp, dict, k);
  if (ops->list.is_nil(interp, v)) {
    return 0;
  }
  int64_t result = 0;
  ops->integer.get(interp, v, &result);
  return result;
}

/**
 * Helper to set a string value in a dict.
 */
static FeatherObj dict_set_str(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj dict, const char *key, FeatherObj value) {
  FeatherObj k = ops->string.intern(interp, key, feather_strlen(key));
  return ops->dict.set(interp, dict, k, value);
}

/**
 * Helper to set an int value in a dict.
 */
static FeatherObj dict_set_int(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj dict, const char *key, int64_t value) {
  FeatherObj k = ops->string.intern(interp, key, feather_strlen(key));
  return ops->dict.set(interp, dict, k, ops->integer.create(interp, value));
}

/**
 * Check if entry is of a given type.
 */
static int entry_is_type(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj entry, const char *typeName) {
  FeatherObj t = dict_get_str(ops, interp, entry, K_TYPE);
  return feather_obj_eq_literal(ops, interp, t, typeName);
}

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
 * Get the usage specs dictionary from ::usage::specs
 */
static FeatherObj usage_get_specs(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj ns = ops->string.intern(interp, S(USAGE_NS));
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
  FeatherObj ns = ops->string.intern(interp, S(USAGE_NS));
  FeatherObj varName = ops->string.intern(interp, "specs", 5);
  ops->ns.set_var(interp, ns, varName, specs);
}

/**
 * Trim leading/trailing newlines and dedent: remove common leading whitespace from each line.
 * This normalizes multi-line help text, long_help, and example bodies for consistent display.
 */
static FeatherObj trim_text_block(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj text) {
  size_t len = ops->string.byte_length(interp, text);
  if (len == 0) return text;

  /* Find first non-newline character */
  size_t start = 0;
  while (start < len) {
    int c = ops->string.byte_at(interp, text, start);
    if (c != '\n' && c != '\r') break;
    start++;
  }

  /* Find last non-newline character */
  size_t end = len;
  while (end > start) {
    int c = ops->string.byte_at(interp, text, end - 1);
    if (c != '\n' && c != '\r') break;
    end--;
  }

  if (start >= end) {
    return ops->string.intern(interp, "", 0);
  }

  /* Find minimum indentation (spaces/tabs at start of non-empty lines) */
  size_t min_indent = (size_t)-1;  /* Large value */
  size_t line_start = start;
  int at_line_start = 1;
  size_t current_indent = 0;

  for (size_t i = start; i < end; i++) {
    int c = ops->string.byte_at(interp, text, i);
    if (c == '\n') {
      /* End of line - only count if line had content */
      if (!at_line_start && current_indent < min_indent) {
        min_indent = current_indent;
      }
      line_start = i + 1;
      at_line_start = 1;
      current_indent = 0;
    } else if (at_line_start && (c == ' ' || c == '\t')) {
      current_indent++;
    } else if (at_line_start) {
      /* First non-whitespace character on line */
      if (current_indent < min_indent) {
        min_indent = current_indent;
      }
      at_line_start = 0;
    }
  }
  /* Handle last line if no trailing newline */
  if (!at_line_start && current_indent < min_indent) {
    min_indent = current_indent;
  }

  if (min_indent == (size_t)-1) {
    min_indent = 0;
  }

  /* Build dedented string */
  FeatherObj builder = ops->string.builder_new(interp, end - start);
  at_line_start = 1;
  size_t skip_count = 0;

  for (size_t i = start; i < end; i++) {
    int c = ops->string.byte_at(interp, text, i);
    if (c == '\n') {
      ops->string.builder_append_byte(interp, builder, (char)c);
      at_line_start = 1;
      skip_count = 0;
    } else if (at_line_start && skip_count < min_indent && (c == ' ' || c == '\t')) {
      skip_count++;
    } else {
      at_line_start = 0;
      ops->string.builder_append_byte(interp, builder, (char)c);
    }
  }

  return ops->string.builder_finish(interp, builder);
}

/**
 * Check if a token is a keyword (arg, flag, cmd, example).
 */
static int is_keyword(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj token) {
  return feather_obj_eq_literal(ops, interp, token, "flag") ||
         feather_obj_eq_literal(ops, interp, token, "arg") ||
         feather_obj_eq_literal(ops, interp, token, "cmd") ||
         feather_obj_eq_literal(ops, interp, token, "example");
}

/**
 * Check if a token is a flag part (-x, --long, <value>, ?value?).
 */
static int is_flag_part(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj token) {
  size_t len = ops->string.byte_length(interp, token);
  if (len == 0) return 0;
  int c = ops->string.byte_at(interp, token, 0);
  return (c == '-' || c == '<' || c == '?');
}

/**
 * Parse an options block for arg/flag/cmd.
 * Block format:
 *   help {text}
 *   long_help {text}
 *   choices {a b c}
 *   default {value}  (for arg only)
 *   type {typename}  (e.g., script, file, dir)
 *   hide
 *   before_help {text}      (for top-level/cmd specs)
 *   after_help {text}
 *   before_long_help {text}
 *   after_long_help {text}
 */
static void parse_options_block(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj block,
                                 FeatherObj *helpOut, FeatherObj *longHelpOut,
                                 FeatherObj *choicesOut, FeatherObj *defaultOut,
                                 FeatherObj *typeOut, int *hideOut,
                                 FeatherObj *beforeHelpOut, FeatherObj *afterHelpOut,
                                 FeatherObj *beforeLongHelpOut, FeatherObj *afterLongHelpOut) {
  FeatherObj optsList = feather_list_parse_obj(ops, interp, block);
  size_t optsLen = ops->list.length(interp, optsList);

  size_t i = 0;
  while (i < optsLen) {
    FeatherObj key = ops->list.at(interp, optsList, i);

    if (feather_obj_eq_literal(ops, interp, key, "hide")) {
      *hideOut = 1;
      i++;
      continue;
    }

    /* Other options need a value */
    if (i + 1 >= optsLen) break;
    FeatherObj value = ops->list.at(interp, optsList, i + 1);

    if (feather_obj_eq_literal(ops, interp, key, "help")) {
      *helpOut = value;
    } else if (feather_obj_eq_literal(ops, interp, key, "long_help")) {
      *longHelpOut = value;
    } else if (feather_obj_eq_literal(ops, interp, key, "choices")) {
      *choicesOut = value;
    } else if (defaultOut && feather_obj_eq_literal(ops, interp, key, "default")) {
      *defaultOut = value;
    } else if (typeOut && feather_obj_eq_literal(ops, interp, key, "type")) {
      *typeOut = value;
    } else if (beforeHelpOut && feather_obj_eq_literal(ops, interp, key, "before_help")) {
      *beforeHelpOut = value;
    } else if (afterHelpOut && feather_obj_eq_literal(ops, interp, key, "after_help")) {
      *afterHelpOut = value;
    } else if (beforeLongHelpOut && feather_obj_eq_literal(ops, interp, key, "before_long_help")) {
      *beforeLongHelpOut = value;
    } else if (afterLongHelpOut && feather_obj_eq_literal(ops, interp, key, "after_long_help")) {
      *afterLongHelpOut = value;
    }

    i += 2;
  }
}

/**
 * Create an example entry from parsed parts.
 */
static FeatherObj usage_example_from_parts(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj code, FeatherObj header, FeatherObj help) {
  FeatherObj entry = ops->dict.create(interp);
  entry = dict_set_str(ops, interp, entry, K_TYPE, ops->string.intern(interp, S(T_EXAMPLE)));
  entry = dict_set_str(ops, interp, entry, K_CODE, code);
  if (ops->string.byte_length(interp, header) > 0) {
    entry = dict_set_str(ops, interp, entry, K_HEADER, header);
  }
  if (ops->string.byte_length(interp, help) > 0) {
    entry = dict_set_str(ops, interp, entry, K_HELP, help);
  }
  return entry;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal API for building usage specs (works with FeatherObj)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create an arg entry from a FeatherObj name.
 * Name format: "<name>" (required), "?name?" (optional), with optional "..." suffix.
 */
static FeatherObj usage_arg_from_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj nameObj) {
  size_t nameLen = ops->string.byte_length(interp, nameObj);
  int required = 0;
  int variadic = 0;
  FeatherObj cleanName;

  /* Check for variadic (...) */
  if (nameLen >= 5) {
    int dot1 = ops->string.byte_at(interp, nameObj, nameLen - 3);
    int dot2 = ops->string.byte_at(interp, nameObj, nameLen - 2);
    int dot3 = ops->string.byte_at(interp, nameObj, nameLen - 1);
    if (dot1 == '.' && dot2 == '.' && dot3 == '.') {
      variadic = 1;
      nameLen -= 3;
    }
  }

  /* Check for <required> or ?optional? */
  if (nameLen >= 2) {
    int first = ops->string.byte_at(interp, nameObj, 0);
    int last = ops->string.byte_at(interp, nameObj, nameLen - 1);
    if (first == '<' && last == '>') {
      required = 1;
      cleanName = ops->string.slice(interp, nameObj, 1, nameLen - 1);
    } else if (first == '?' && last == '?') {
      required = 0;
      cleanName = ops->string.slice(interp, nameObj, 1, nameLen - 1);
    } else {
      cleanName = variadic ? ops->string.slice(interp, nameObj, 0, nameLen) : nameObj;
      required = 1;
    }
  } else {
    cleanName = nameObj;
    required = 1;
  }

  FeatherObj entry = ops->dict.create(interp);
  entry = dict_set_str(ops, interp, entry, K_TYPE, ops->string.intern(interp, S(T_ARG)));
  entry = dict_set_str(ops, interp, entry, K_NAME, cleanName);
  entry = dict_set_int(ops, interp, entry, K_REQUIRED, required);
  if (variadic) {
    entry = dict_set_int(ops, interp, entry, K_VARIADIC, 1);
  }

  return entry;
}

/**
 * Create a flag entry from pre-parsed parts (already stripped of dashes).
 */
static FeatherObj usage_flag_from_parts(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj shortFlag, FeatherObj longFlag,
                                        int hasValue, int valueRequired) {
  /* Derive variable name from long flag or short flag */
  FeatherObj varName;
  if (ops->string.byte_length(interp, longFlag) > 0) {
    varName = sanitize_var_name(ops, interp, longFlag);
  } else {
    varName = sanitize_var_name(ops, interp, shortFlag);
  }

  FeatherObj entry = ops->dict.create(interp);
  entry = dict_set_str(ops, interp, entry, K_TYPE, ops->string.intern(interp, S(T_FLAG)));
  if (ops->string.byte_length(interp, shortFlag) > 0) {
    entry = dict_set_str(ops, interp, entry, K_SHORT, shortFlag);
  }
  if (ops->string.byte_length(interp, longFlag) > 0) {
    entry = dict_set_str(ops, interp, entry, K_LONG, longFlag);
  }
  entry = dict_set_int(ops, interp, entry, K_HAS_VALUE, hasValue);
  entry = dict_set_int(ops, interp, entry, K_VALUE_REQ, valueRequired);
  entry = dict_set_str(ops, interp, entry, K_VAR_NAME, varName);

  return entry;
}

/**
 * Create a cmd entry from FeatherObj name.
 */
static FeatherObj usage_cmd_from_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj nameObj, FeatherObj subspec) {
  FeatherObj entry = ops->dict.create(interp);
  entry = dict_set_str(ops, interp, entry, K_TYPE, ops->string.intern(interp, S(T_CMD)));
  entry = dict_set_str(ops, interp, entry, K_NAME, nameObj);
  entry = dict_set_str(ops, interp, entry, K_SPEC, subspec);
  return entry;
}

/* Internal setters that take FeatherObj values */
static FeatherObj usage_set_help(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry, FeatherObj text) {
  return dict_set_str(ops, interp, entry, K_HELP, text);
}

static FeatherObj usage_set_long_help(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj entry, FeatherObj text) {
  return dict_set_str(ops, interp, entry, K_LONG_HELP, text);
}

static FeatherObj usage_set_default(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj entry, FeatherObj value) {
  return dict_set_str(ops, interp, entry, K_DEFAULT, value);
}

static FeatherObj usage_set_choices(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj entry, FeatherObj choices) {
  return dict_set_str(ops, interp, entry, K_CHOICES, choices);
}

static FeatherObj usage_set_type(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry, FeatherObj type) {
  return dict_set_str(ops, interp, entry, K_VALUE_TYPE, type);
}

static FeatherObj usage_set_hide(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry) {
  return dict_set_int(ops, interp, entry, K_HIDE, 1);
}

/**
 * Parse a spec string into a structured representation.
 *
 * New block-based format:
 *   flag -s --long <value> { options }
 *   arg <name> { options }
 *   cmd name { body } { options }
 *
 * Returns a list of dict entries.
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
      /* arg <name> or arg ?name? with optional options block */
      if (i >= specLen) break;

      FeatherObj argName = ops->list.at(interp, specList, i);
      i++;

      /* Create arg entry using internal API */
      FeatherObj entry = usage_arg_from_obj(ops, interp, argName);

      /* Check for options block */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj defaultVal = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      FeatherObj choices = ops->string.intern(interp, "", 0);
      FeatherObj typeVal = ops->string.intern(interp, "", 0);
      int hide = 0;

      if (i < specLen) {
        FeatherObj next = ops->list.at(interp, specList, i);
        if (!is_keyword(ops, interp, next)) {
          /* This is the options block - arg doesn't use before/after help */
          parse_options_block(ops, interp, next, &helpText, &longHelp, &choices, &defaultVal, &typeVal, &hide,
                              NULL, NULL, NULL, NULL);
          i++;
        }
      }

      /* Apply options using internal API */
      if (ops->string.byte_length(interp, helpText) > 0)
        entry = usage_set_help(ops, interp, entry, helpText);
      if (ops->string.byte_length(interp, defaultVal) > 0)
        entry = usage_set_default(ops, interp, entry, defaultVal);
      if (ops->string.byte_length(interp, longHelp) > 0)
        entry = usage_set_long_help(ops, interp, entry, longHelp);
      if (ops->string.byte_length(interp, choices) > 0)
        entry = usage_set_choices(ops, interp, entry, choices);
      if (hide)
        entry = usage_set_hide(ops, interp, entry);
      if (ops->string.byte_length(interp, typeVal) > 0)
        entry = usage_set_type(ops, interp, entry, typeVal);

      result = ops->list.push(interp, result, entry);

    } else if (feather_obj_eq_literal(ops, interp, keyword, "cmd")) {
      /* cmd name { body } { options } */
      if (i >= specLen) break;

      FeatherObj cmdName = ops->list.at(interp, specList, i);
      i++;

      /* Get the body */
      FeatherObj cmdBody = ops->string.intern(interp, "", 0);
      if (i < specLen) {
        cmdBody = ops->list.at(interp, specList, i);
        i++;
      }

      /* Check for options block - cmd supports before/after help */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      FeatherObj beforeHelp = ops->string.intern(interp, "", 0);
      FeatherObj afterHelp = ops->string.intern(interp, "", 0);
      FeatherObj beforeLongHelp = ops->string.intern(interp, "", 0);
      FeatherObj afterLongHelp = ops->string.intern(interp, "", 0);
      int hide = 0;

      if (i < specLen) {
        FeatherObj next = ops->list.at(interp, specList, i);
        if (!is_keyword(ops, interp, next)) {
          /* This is the options block */
          parse_options_block(ops, interp, next, &helpText, &longHelp, NULL, NULL, NULL, &hide,
                              &beforeHelp, &afterHelp, &beforeLongHelp, &afterLongHelp);
          i++;
        }
      }

      /* Recursively parse the subcommand body */
      FeatherObj subSpec = parse_spec(ops, interp, cmdBody);

      /* Build cmd entry using internal API */
      FeatherObj entry = usage_cmd_from_obj(ops, interp, cmdName, subSpec);

      /* Apply options using internal API */
      if (ops->string.byte_length(interp, helpText) > 0)
        entry = usage_set_help(ops, interp, entry, helpText);
      if (ops->string.byte_length(interp, longHelp) > 0)
        entry = usage_set_long_help(ops, interp, entry, longHelp);
      if (hide)
        entry = usage_set_hide(ops, interp, entry);
      if (ops->string.byte_length(interp, beforeHelp) > 0)
        entry = dict_set_str(ops, interp, entry, K_BEFORE_HELP, beforeHelp);
      if (ops->string.byte_length(interp, afterHelp) > 0)
        entry = dict_set_str(ops, interp, entry, K_AFTER_HELP, afterHelp);
      if (ops->string.byte_length(interp, beforeLongHelp) > 0)
        entry = dict_set_str(ops, interp, entry, K_BEFORE_LONG_HELP, beforeLongHelp);
      if (ops->string.byte_length(interp, afterLongHelp) > 0)
        entry = dict_set_str(ops, interp, entry, K_AFTER_LONG_HELP, afterLongHelp);

      result = ops->list.push(interp, result, entry);

    } else if (feather_obj_eq_literal(ops, interp, keyword, "flag")) {
      /* flag -s --long <value> { options } */
      /* Collect flag parts until we hit a non-flag-part or keyword */
      FeatherObj shortFlag = ops->string.intern(interp, "", 0);
      FeatherObj longFlag = ops->string.intern(interp, "", 0);
      int hasValue = 0;
      int valueRequired = 0;

      while (i < specLen) {
        FeatherObj part = ops->list.at(interp, specList, i);
        if (!is_flag_part(ops, interp, part)) break;

        size_t partLen = ops->string.byte_length(interp, part);
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
        } else if (c0 == '?' && partLen >= 2) {
          /* Optional value: ?name? */
          hasValue = 1;
          valueRequired = 0;
        }

        i++;
      }

      /* Build flag entry using internal API */
      FeatherObj entry = usage_flag_from_parts(ops, interp, shortFlag, longFlag, hasValue, valueRequired);

      /* Check for options block - flag doesn't use before/after help */
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      FeatherObj choices = ops->string.intern(interp, "", 0);
      FeatherObj typeVal = ops->string.intern(interp, "", 0);
      int hide = 0;

      if (i < specLen) {
        FeatherObj next = ops->list.at(interp, specList, i);
        if (!is_keyword(ops, interp, next)) {
          /* This is the options block */
          parse_options_block(ops, interp, next, &helpText, &longHelp, &choices, NULL, &typeVal, &hide,
                              NULL, NULL, NULL, NULL);
          i++;
        }
      }

      /* Apply options using internal API */
      if (ops->string.byte_length(interp, helpText) > 0)
        entry = usage_set_help(ops, interp, entry, helpText);
      if (ops->string.byte_length(interp, longHelp) > 0)
        entry = usage_set_long_help(ops, interp, entry, longHelp);
      if (ops->string.byte_length(interp, choices) > 0)
        entry = usage_set_choices(ops, interp, entry, choices);
      if (hide)
        entry = usage_set_hide(ops, interp, entry);
      if (ops->string.byte_length(interp, typeVal) > 0)
        entry = usage_set_type(ops, interp, entry, typeVal);

      result = ops->list.push(interp, result, entry);

    } else if (feather_obj_eq_literal(ops, interp, keyword, "example")) {
      /* example <code> { options } */
      if (i >= specLen) break;

      FeatherObj code = ops->list.at(interp, specList, i);
      i++;

      /* Check for options block */
      FeatherObj header = ops->string.intern(interp, "", 0);
      FeatherObj helpText = ops->string.intern(interp, "", 0);

      if (i < specLen) {
        FeatherObj next = ops->list.at(interp, specList, i);
        if (!is_keyword(ops, interp, next)) {
          /* This is the options block - parse header and help */
          FeatherObj optsList = feather_list_parse_obj(ops, interp, next);
          size_t optsLen = ops->list.length(interp, optsList);
          size_t j = 0;
          while (j < optsLen) {
            FeatherObj key = ops->list.at(interp, optsList, j);
            if (j + 1 >= optsLen) break;
            FeatherObj value = ops->list.at(interp, optsList, j + 1);
            if (feather_obj_eq_literal(ops, interp, key, "header")) {
              header = value;
            } else if (feather_obj_eq_literal(ops, interp, key, "help")) {
              helpText = value;
            }
            j += 2;
          }
          i++;
        }
      }

      /* Create example entry */
      FeatherObj entry = usage_example_from_parts(ops, interp, code, header, helpText);
      result = ops->list.push(interp, result, entry);
    }
  }

  return result;
}

/**
 * Helper to append a string literal to a builder.
 */
static void append_str(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj builder, const char *s) {
  while (*s) ops->string.builder_append_byte(interp, builder, *s++);
}

/**
 * Generate usage string for display (--help output)
 * Follows standard Unix manpage format with NAME, SYNOPSIS, DESCRIPTION, etc.
 */
static FeatherObj generate_usage_string(const FeatherHostOps *ops, FeatherInterp interp,
                                         FeatherObj cmdName, FeatherObj parsedSpec) {
  FeatherObj builder = ops->string.builder_new(interp, 512);
  size_t specLen = ops->list.length(interp, parsedSpec);

  /* Check for features in spec */
  int hasFlags = 0;
  int hasArgs = 0;
  int hasSubcmds = 0;
  int hasExamples = 0;
  FeatherObj firstArgHelp = ops->string.intern(interp, "", 0);

  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    if (entry_is_type(ops, interp, entry, T_FLAG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (!hide) hasFlags = 1;
    }
    if (entry_is_type(ops, interp, entry, T_ARG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (!hide) {
        if (!hasArgs) {
          /* Capture first arg's help for NAME section */
          firstArgHelp = dict_get_str(ops, interp, entry, K_HELP);
        }
        hasArgs = 1;
      }
    }
    if (entry_is_type(ops, interp, entry, T_CMD)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (!hide) hasSubcmds = 1;
    }
    if (entry_is_type(ops, interp, entry, T_EXAMPLE)) {
      hasExamples = 1;
    }
  }

  /* === Header line === */
  /* Format: cmdname(1)         General Commands Manual         cmdname(1) */
  ops->string.builder_append_obj(interp, builder, cmdName);
  append_str(ops, interp, builder, "(1)");
  append_str(ops, interp, builder, "                    General Commands Manual                   ");
  ops->string.builder_append_obj(interp, builder, cmdName);
  append_str(ops, interp, builder, "(1)");

  /* === NAME section === */
  append_str(ops, interp, builder, "\n\nNAME\n       ");
  ops->string.builder_append_obj(interp, builder, cmdName);
  if (ops->string.byte_length(interp, firstArgHelp) > 0) {
    append_str(ops, interp, builder, " - ");
    FeatherObj trimmed = trim_text_block(ops, interp, firstArgHelp);
    ops->string.builder_append_obj(interp, builder, trimmed);
  }

  /* === SYNOPSIS section === */
  append_str(ops, interp, builder, "\n\nSYNOPSIS\n       ");
  ops->string.builder_append_obj(interp, builder, cmdName);

  if (hasFlags) {
    append_str(ops, interp, builder, " [OPTIONS]");
  }

  if (hasSubcmds) {
    append_str(ops, interp, builder, " <COMMAND>");
  }

  /* Add positional args to synopsis */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);

    if (entry_is_type(ops, interp, entry, T_ARG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (hide) continue;

      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
      int64_t required = dict_get_int(ops, interp, entry, K_REQUIRED);
      int64_t variadic = dict_get_int(ops, interp, entry, K_VARIADIC);

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
        append_str(ops, interp, builder, "...");
      }
    }
  }

  /* === DESCRIPTION section (uses long_help from first arg if available) === */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    if (entry_is_type(ops, interp, entry, T_ARG)) {
      FeatherObj longHelp = dict_get_str(ops, interp, entry, K_LONG_HELP);
      if (ops->string.byte_length(interp, longHelp) > 0) {
        append_str(ops, interp, builder, "\n\nDESCRIPTION\n       ");
        FeatherObj trimmed = trim_text_block(ops, interp, longHelp);
        ops->string.builder_append_obj(interp, builder, trimmed);
        break;
      }
    }
  }

  /* === OPTIONS section === */
  int flagsShown = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);

    if (entry_is_type(ops, interp, entry, T_FLAG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (hide) continue;

      if (!flagsShown) {
        append_str(ops, interp, builder, "\n\nOPTIONS");
        flagsShown = 1;
      }

      append_str(ops, interp, builder, "\n       ");

      FeatherObj shortFlag = dict_get_str(ops, interp, entry, K_SHORT);
      FeatherObj longFlag = dict_get_str(ops, interp, entry, K_LONG);
      int64_t hasValue = dict_get_int(ops, interp, entry, K_HAS_VALUE);
      FeatherObj varName = dict_get_str(ops, interp, entry, K_VAR_NAME);
      FeatherObj helpText = dict_get_str(ops, interp, entry, K_HELP);
      FeatherObj choices = dict_get_str(ops, interp, entry, K_CHOICES);

      if (ops->string.byte_length(interp, shortFlag) > 0) {
        ops->string.builder_append_byte(interp, builder, '-');
        ops->string.builder_append_obj(interp, builder, shortFlag);
        if (ops->string.byte_length(interp, longFlag) > 0) {
          append_str(ops, interp, builder, ", ");
        }
      }

      if (ops->string.byte_length(interp, longFlag) > 0) {
        append_str(ops, interp, builder, "--");
        ops->string.builder_append_obj(interp, builder, longFlag);
      }

      if (hasValue) {
        append_str(ops, interp, builder, " <");
        ops->string.builder_append_obj(interp, builder, varName);
        ops->string.builder_append_byte(interp, builder, '>');
      }

      /* Help text on next line, indented */
      if (ops->string.byte_length(interp, helpText) > 0) {
        append_str(ops, interp, builder, "\n              ");
        FeatherObj trimmed = trim_text_block(ops, interp, helpText);
        ops->string.builder_append_obj(interp, builder, trimmed);
      }

      /* Choices on next line, indented */
      if (ops->string.byte_length(interp, choices) > 0) {
        append_str(ops, interp, builder, "\n              Choices: ");
        ops->string.builder_append_obj(interp, builder, choices);
      }
    }
  }

  /* === ARGUMENTS section === */
  int argsShown = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);

    if (entry_is_type(ops, interp, entry, T_ARG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (hide) continue;

      FeatherObj helpText = dict_get_str(ops, interp, entry, K_HELP);
      FeatherObj choices = dict_get_str(ops, interp, entry, K_CHOICES);

      /* Show arg if it has help text or choices */
      if (ops->string.byte_length(interp, helpText) > 0 ||
          ops->string.byte_length(interp, choices) > 0) {
        if (!argsShown) {
          append_str(ops, interp, builder, "\n\nARGUMENTS");
          argsShown = 1;
        }

        append_str(ops, interp, builder, "\n       ");

        FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
        int64_t required = dict_get_int(ops, interp, entry, K_REQUIRED);

        if (required) {
          ops->string.builder_append_byte(interp, builder, '<');
          ops->string.builder_append_obj(interp, builder, name);
          ops->string.builder_append_byte(interp, builder, '>');
        } else {
          ops->string.builder_append_byte(interp, builder, '?');
          ops->string.builder_append_obj(interp, builder, name);
          ops->string.builder_append_byte(interp, builder, '?');
        }

        /* Help text on next line, indented */
        if (ops->string.byte_length(interp, helpText) > 0) {
          append_str(ops, interp, builder, "\n              ");
          FeatherObj trimmed = trim_text_block(ops, interp, helpText);
          ops->string.builder_append_obj(interp, builder, trimmed);
        }

        if (ops->string.byte_length(interp, choices) > 0) {
          append_str(ops, interp, builder, "\n              Choices: ");
          ops->string.builder_append_obj(interp, builder, choices);
        }
      }
    }
  }

  /* === COMMANDS section === */
  if (hasSubcmds) {
    append_str(ops, interp, builder, "\n\nCOMMANDS");

    for (size_t i = 0; i < specLen; i++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, i);

      if (entry_is_type(ops, interp, entry, T_CMD)) {
        int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
        if (hide) continue;

        FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
        FeatherObj helpText = dict_get_str(ops, interp, entry, K_HELP);

        append_str(ops, interp, builder, "\n       ");
        ops->string.builder_append_obj(interp, builder, name);

        if (ops->string.byte_length(interp, helpText) > 0) {
          append_str(ops, interp, builder, "\n              ");
          FeatherObj trimmed = trim_text_block(ops, interp, helpText);
          ops->string.builder_append_obj(interp, builder, trimmed);
        }
      }
    }
  }

  /* === EXAMPLES section === */
  if (hasExamples) {
    append_str(ops, interp, builder, "\n\nEXAMPLES");
    int exampleCount = 0;

    for (size_t i = 0; i < specLen; i++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, i);

      if (entry_is_type(ops, interp, entry, T_EXAMPLE)) {
        FeatherObj code = dict_get_str(ops, interp, entry, K_CODE);
        FeatherObj header = dict_get_str(ops, interp, entry, K_HEADER);
        FeatherObj helpText = dict_get_str(ops, interp, entry, K_HELP);

        /* Build description: prefer header, fallback to help */
        FeatherObj description = header;
        if (ops->string.byte_length(interp, header) == 0) {
          description = helpText;
        }

        /* Add blank line between examples */
        if (exampleCount > 0) {
          ops->string.builder_append_byte(interp, builder, '\n');
        }
        exampleCount++;

        /* Description followed by colon */
        if (ops->string.byte_length(interp, description) > 0) {
          append_str(ops, interp, builder, "\n       ");
          FeatherObj trimmed = trim_text_block(ops, interp, description);
          ops->string.builder_append_obj(interp, builder, trimmed);
          ops->string.builder_append_byte(interp, builder, ':');
        }

        /* The example code, indented on new line with blank line before */
        if (ops->string.byte_length(interp, code) > 0) {
          append_str(ops, interp, builder, "\n\n           ");
          FeatherObj trimmed = trim_text_block(ops, interp, code);
          ops->string.builder_append_obj(interp, builder, trimmed);
        }
      }
    }
  }

  ops->string.builder_append_byte(interp, builder, '\n');

  return ops->string.builder_finish(interp, builder);
}

/* Forward declaration for recursive conversion */
static FeatherObj spec_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj parsedSpec);

/**
 * Build options block list from an entry's optional fields.
 * Returns nil if no options are set.
 */
static FeatherObj entry_options_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj entry, int is_arg) {
  FeatherObj opts = ops->list.create(interp);
  int has_opts = 0;

  /* help */
  FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);
  if (ops->string.byte_length(interp, help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_HELP)));
    opts = ops->list.push(interp, opts, help);
    has_opts = 1;
  }

  /* long_help */
  FeatherObj long_help = dict_get_str(ops, interp, entry, K_LONG_HELP);
  if (ops->string.byte_length(interp, long_help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_LONG_HELP)));
    opts = ops->list.push(interp, opts, long_help);
    has_opts = 1;
  }

  /* default (arg only) */
  if (is_arg) {
    FeatherObj def = dict_get_str(ops, interp, entry, K_DEFAULT);
    if (ops->string.byte_length(interp, def) > 0) {
      opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_DEFAULT)));
      opts = ops->list.push(interp, opts, def);
      has_opts = 1;
    }
  }

  /* choices */
  FeatherObj choices = dict_get_str(ops, interp, entry, K_CHOICES);
  if (ops->string.byte_length(interp, choices) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_CHOICES)));
    opts = ops->list.push(interp, opts, choices);
    has_opts = 1;
  }

  /* value_type -> type */
  FeatherObj vtype = dict_get_str(ops, interp, entry, K_VALUE_TYPE);
  if (ops->string.byte_length(interp, vtype) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, "type", 4));
    opts = ops->list.push(interp, opts, vtype);
    has_opts = 1;
  }

  /* hide */
  if (dict_get_int(ops, interp, entry, K_HIDE)) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_HIDE)));
    has_opts = 1;
  }

  return has_opts ? opts : 0;
}

/**
 * Convert a parsed arg entry back to list format: {arg <name> ?{options}?}
 */
static FeatherObj arg_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry) {
  FeatherObj result = ops->list.create(interp);

  /* "arg" keyword */
  result = ops->list.push(interp, result, ops->string.intern(interp, S(T_ARG)));

  /* Build name with delimiters: <name>, ?name?, <name>..., ?name?... */
  FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
  int required = dict_get_int(ops, interp, entry, K_REQUIRED);
  int variadic = dict_get_int(ops, interp, entry, K_VARIADIC);

  FeatherObj builder = ops->string.builder_new(interp, 32);
  ops->string.builder_append_byte(interp, builder, required ? '<' : '?');
  ops->string.builder_append_obj(interp, builder, name);
  ops->string.builder_append_byte(interp, builder, required ? '>' : '?');
  if (variadic) {
    ops->string.builder_append_byte(interp, builder, '.');
    ops->string.builder_append_byte(interp, builder, '.');
    ops->string.builder_append_byte(interp, builder, '.');
  }
  result = ops->list.push(interp, result, ops->string.builder_finish(interp, builder));

  /* Options block if any */
  FeatherObj opts = entry_options_to_list(ops, interp, entry, 1);
  if (!ops->list.is_nil(interp, opts)) {
    result = ops->list.push(interp, result, opts);
  }

  return result;
}

/**
 * Convert a parsed flag entry back to list format: {flag -s --long ?<val>? ?{options}?}
 */
static FeatherObj flag_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj entry) {
  FeatherObj result = ops->list.create(interp);

  /* "flag" keyword */
  result = ops->list.push(interp, result, ops->string.intern(interp, S(T_FLAG)));

  /* Short flag: -X */
  FeatherObj shortFlag = dict_get_str(ops, interp, entry, K_SHORT);
  if (ops->string.byte_length(interp, shortFlag) > 0) {
    FeatherObj builder = ops->string.builder_new(interp, 4);
    ops->string.builder_append_byte(interp, builder, '-');
    ops->string.builder_append_obj(interp, builder, shortFlag);
    result = ops->list.push(interp, result, ops->string.builder_finish(interp, builder));
  }

  /* Long flag: --XXX */
  FeatherObj longFlag = dict_get_str(ops, interp, entry, K_LONG);
  if (ops->string.byte_length(interp, longFlag) > 0) {
    FeatherObj builder = ops->string.builder_new(interp, 32);
    ops->string.builder_append_byte(interp, builder, '-');
    ops->string.builder_append_byte(interp, builder, '-');
    ops->string.builder_append_obj(interp, builder, longFlag);
    result = ops->list.push(interp, result, ops->string.builder_finish(interp, builder));
  }

  /* Value spec if flag takes a value */
  int has_value = dict_get_int(ops, interp, entry, K_HAS_VALUE);
  if (has_value) {
    int value_required = dict_get_int(ops, interp, entry, K_VALUE_REQ);
    /* Use var_name for the value placeholder, or "value" as default */
    FeatherObj var = dict_get_str(ops, interp, entry, K_VAR_NAME);
    if (ops->string.byte_length(interp, var) == 0) {
      var = ops->string.intern(interp, "value", 5);
    }
    FeatherObj builder = ops->string.builder_new(interp, 32);
    ops->string.builder_append_byte(interp, builder, value_required ? '<' : '?');
    ops->string.builder_append_obj(interp, builder, var);
    ops->string.builder_append_byte(interp, builder, value_required ? '>' : '?');
    result = ops->list.push(interp, result, ops->string.builder_finish(interp, builder));
  }

  /* Options block if any */
  FeatherObj opts = entry_options_to_list(ops, interp, entry, 0);
  if (!ops->list.is_nil(interp, opts)) {
    result = ops->list.push(interp, result, opts);
  }

  return result;
}

/**
 * Convert a parsed cmd entry back to list format: {cmd name {subspec} ?{options}?}
 */
static FeatherObj cmd_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry) {
  FeatherObj result = ops->list.create(interp);

  /* "cmd" keyword */
  result = ops->list.push(interp, result, ops->string.intern(interp, S(T_CMD)));

  /* Subcommand name */
  FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
  result = ops->list.push(interp, result, name);

  /* Subspec (recursively convert) */
  FeatherObj subspec = dict_get_str(ops, interp, entry, K_SPEC);
  if (!ops->list.is_nil(interp, subspec)) {
    result = ops->list.push(interp, result, spec_to_list(ops, interp, subspec));
  } else {
    result = ops->list.push(interp, result, ops->list.create(interp));
  }

  /* Options block if any (cmd supports help, long_help, hide, before/after help) */
  FeatherObj opts = ops->list.create(interp);
  int has_opts = 0;

  FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);
  if (ops->string.byte_length(interp, help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_HELP)));
    opts = ops->list.push(interp, opts, help);
    has_opts = 1;
  }

  FeatherObj long_help = dict_get_str(ops, interp, entry, K_LONG_HELP);
  if (ops->string.byte_length(interp, long_help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_LONG_HELP)));
    opts = ops->list.push(interp, opts, long_help);
    has_opts = 1;
  }

  if (dict_get_int(ops, interp, entry, K_HIDE)) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_HIDE)));
    has_opts = 1;
  }

  /* before/after help */
  FeatherObj before_help = dict_get_str(ops, interp, entry, K_BEFORE_HELP);
  if (ops->string.byte_length(interp, before_help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_BEFORE_HELP)));
    opts = ops->list.push(interp, opts, before_help);
    has_opts = 1;
  }

  FeatherObj after_help = dict_get_str(ops, interp, entry, K_AFTER_HELP);
  if (ops->string.byte_length(interp, after_help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_AFTER_HELP)));
    opts = ops->list.push(interp, opts, after_help);
    has_opts = 1;
  }

  FeatherObj before_long_help = dict_get_str(ops, interp, entry, K_BEFORE_LONG_HELP);
  if (ops->string.byte_length(interp, before_long_help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_BEFORE_LONG_HELP)));
    opts = ops->list.push(interp, opts, before_long_help);
    has_opts = 1;
  }

  FeatherObj after_long_help = dict_get_str(ops, interp, entry, K_AFTER_LONG_HELP);
  if (ops->string.byte_length(interp, after_long_help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_AFTER_LONG_HELP)));
    opts = ops->list.push(interp, opts, after_long_help);
    has_opts = 1;
  }

  if (has_opts) {
    result = ops->list.push(interp, result, opts);
  }

  return result;
}

/**
 * Convert a parsed example entry back to list format: {example <code> ?{options}?}
 */
static FeatherObj example_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj entry) {
  FeatherObj result = ops->list.create(interp);

  /* "example" keyword */
  result = ops->list.push(interp, result, ops->string.intern(interp, S(T_EXAMPLE)));

  /* Code */
  FeatherObj code = dict_get_str(ops, interp, entry, K_CODE);
  result = ops->list.push(interp, result, code);

  /* Options block if header or help is present */
  FeatherObj opts = ops->list.create(interp);
  int has_opts = 0;

  FeatherObj header = dict_get_str(ops, interp, entry, K_HEADER);
  if (ops->string.byte_length(interp, header) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_HEADER)));
    opts = ops->list.push(interp, opts, header);
    has_opts = 1;
  }

  FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);
  if (ops->string.byte_length(interp, help) > 0) {
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S(K_HELP)));
    opts = ops->list.push(interp, opts, help);
    has_opts = 1;
  }

  if (has_opts) {
    result = ops->list.push(interp, result, opts);
  }

  return result;
}

/**
 * Convert a parsed spec (list of entry dicts) back to input format (list of entry lists).
 */
static FeatherObj spec_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj parsedSpec) {
  FeatherObj result = ops->list.create(interp);
  size_t len = ops->list.length(interp, parsedSpec);

  for (size_t i = 0; i < len; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    FeatherObj typeVal = dict_get_str(ops, interp, entry, K_TYPE);

    FeatherObj entryList;
    if (feather_obj_eq_literal(ops, interp, typeVal, T_ARG)) {
      entryList = arg_to_list(ops, interp, entry);
    } else if (feather_obj_eq_literal(ops, interp, typeVal, T_FLAG)) {
      entryList = flag_to_list(ops, interp, entry);
    } else if (feather_obj_eq_literal(ops, interp, typeVal, T_CMD)) {
      entryList = cmd_to_list(ops, interp, entry);
    } else if (feather_obj_eq_literal(ops, interp, typeVal, T_EXAMPLE)) {
      entryList = example_to_list(ops, interp, entry);
    } else {
      continue;  /* Unknown entry type, skip */
    }

    /* Flatten: append each element of entryList to result */
    size_t entryLen = ops->list.length(interp, entryList);
    for (size_t j = 0; j < entryLen; j++) {
      result = ops->list.push(interp, result, ops->list.at(interp, entryList, j));
    }
  }

  return result;
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
    /* Get mode: return original spec string for round-tripping */
    FeatherObj specEntry = ops->dict.get(interp, specs, cmdName);
    if (ops->list.is_nil(interp, specEntry)) {
      FeatherObj msg = ops->string.intern(interp, "no usage defined for \"", 22);
      msg = ops->string.concat(interp, msg, cmdName);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    /* Return the original spec string (preserves formatting) */
    FeatherObj origSpec = dict_get_str(ops, interp, specEntry, K_ORIG);
    ops->interp.set_result(interp, origSpec);
    return TCL_OK;
  }

  /* Set mode: store the spec */
  FeatherObj specStr = ops->list.at(interp, args, 1);

  /* Parse the spec into structured form */
  FeatherObj parsed = parse_spec(ops, interp, specStr);

  /* Store both original and parsed in a dict for round-tripping */
  FeatherObj specEntry = ops->dict.create(interp);
  specEntry = dict_set_str(ops, interp, specEntry, K_ORIG, specStr);
  specEntry = dict_set_str(ops, interp, specEntry, K_SPEC, parsed);

  specs = ops->dict.set(interp, specs, cmdName, specEntry);
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
    if (entry_is_type(ops, interp, entry, T_CMD)) {
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
    if (entry_is_type(ops, interp, entry, T_CMD)) {
      count++;
    }
  }

  /* Build list */
  int idx = 0;
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    if (entry_is_type(ops, interp, entry, T_CMD)) {
      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
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

    if (entry_is_type(ops, interp, entry, T_ARG)) {
      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
      FeatherObj defaultVal = dict_get_str(ops, interp, entry, K_DEFAULT);
      int64_t variadic = dict_get_int(ops, interp, entry, K_VARIADIC);

      if (variadic) {
        ops->var.set(interp, name, ops->list.create(interp));
      } else {
        ops->var.set(interp, name, defaultVal);
      }
    } else if (entry_is_type(ops, interp, entry, T_FLAG)) {
      FeatherObj varName = dict_get_str(ops, interp, entry, K_VAR_NAME);
      int64_t hasValue = dict_get_int(ops, interp, entry, K_HAS_VALUE);

      if (hasValue) {
        ops->var.set(interp, varName, ops->string.intern(interp, "", 0));
      } else {
        ops->var.set(interp, varName, ops->integer.create(interp, 0));
      }
    } else if (entry_is_type(ops, interp, entry, T_CMD)) {
      /* Recursively init subcommand vars */
      FeatherObj subSpec = dict_get_str(ops, interp, entry, K_SPEC);
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

      if (!entry_is_type(ops, interp, entry, T_FLAG)) continue;

      FeatherObj shortFlag = dict_get_str(ops, interp, entry, K_SHORT);
      FeatherObj longFlag = dict_get_str(ops, interp, entry, K_LONG);
      int64_t hasValue = dict_get_int(ops, interp, entry, K_HAS_VALUE);
      FeatherObj varName = dict_get_str(ops, interp, entry, K_VAR_NAME);

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
 * Check if a string is a syntactically complete TCL script.
 * A script is complete when all braces, brackets, and quotes are balanced.
 * Returns 1 if complete, 0 if incomplete.
 */
static int is_script_complete(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj script) {
  size_t len = ops->string.byte_length(interp, script);
  if (len == 0) return 1;  /* Empty script is complete */

  FeatherParseContextObj ctx;
  feather_parse_init_obj(&ctx, script, len);

  FeatherParseStatus status;
  while ((status = feather_parse_command_obj(ops, interp, &ctx)) == TCL_PARSE_OK) {
    /* Continue parsing commands */
  }

  /* TCL_PARSE_DONE means we successfully reached the end of the script */
  /* TCL_PARSE_INCOMPLETE means unbalanced braces/quotes */
  /* TCL_PARSE_ERROR means syntax error */
  return status == TCL_PARSE_DONE;
}

/**
 * usage parse command argsList
 *
 * Parse arguments according to the usage spec and create local variables.
 * Supports nested subcommands up to 8 levels deep.
 * Validates type constraints (e.g., type script requires complete TCL script).
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

  /* Extract parsed spec from storage dict */
  FeatherObj parsedSpec = dict_get_str(ops, interp, specEntry, K_SPEC);

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

        if (!entry_is_type(ops, interp, entry, T_CMD)) continue;

        FeatherObj subcmdName = dict_get_str(ops, interp, entry, K_NAME);
        if (ops->string.equal(interp, arg, subcmdName)) {
          /* Found matching subcommand */
          foundSubcmd = 1;
          subcmdPath = ops->list.push(interp, subcmdPath, subcmdName);

          /* Descend into subcommand spec */
          FeatherObj subSpec = dict_get_str(ops, interp, entry, K_SPEC);
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
          if (entry_is_type(ops, interp, entry, T_ARG)) {
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

      if (!entry_is_type(ops, interp, entry, T_ARG)) continue;

      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
      int64_t variadic = dict_get_int(ops, interp, entry, K_VARIADIC);

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

  /* Set $subcommand variable with subcommand path as a list */
  FeatherObj subcmdVar = ops->string.intern(interp, "subcommand", 10);
  ops->var.set(interp, subcmdVar, subcmdPath);

  /* Check required args were provided (in the active spec) */
  for (size_t i = 0; i < activeSpecLen; i++) {
    FeatherObj entry = ops->list.at(interp, activeSpec, i);

    if (entry_is_type(ops, interp, entry, T_ARG)) {
      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
      int64_t required = dict_get_int(ops, interp, entry, K_REQUIRED);
      int64_t variadic = dict_get_int(ops, interp, entry, K_VARIADIC);

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

  /* Validate type constraints (e.g., type script requires complete TCL script) */
  for (size_t i = 0; i < activeSpecLen; i++) {
    FeatherObj entry = ops->list.at(interp, activeSpec, i);

    if (entry_is_type(ops, interp, entry, T_ARG)) {
      FeatherObj argType = dict_get_str(ops, interp, entry, K_VALUE_TYPE);
      if (ops->string.byte_length(interp, argType) > 0 &&
          feather_obj_eq_literal(ops, interp, argType, "script")) {
        FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
        FeatherObj value = ops->var.get(interp, name);
        if (!ops->list.is_nil(interp, value) &&
            ops->string.byte_length(interp, value) > 0 &&
            !is_script_complete(ops, interp, value)) {
          FeatherObj msg = ops->string.intern(interp, "argument \"", 10);
          msg = ops->string.concat(interp, msg, name);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\" must be a complete script", 27));
          ops->interp.set_result(interp, msg);
          return TCL_ERROR;
        }
      }
    } else if (entry_is_type(ops, interp, entry, T_FLAG)) {
      FeatherObj flagType = dict_get_str(ops, interp, entry, K_VALUE_TYPE);
      if (ops->string.byte_length(interp, flagType) > 0 &&
          feather_obj_eq_literal(ops, interp, flagType, "script")) {
        FeatherObj varName = dict_get_str(ops, interp, entry, K_VAR_NAME);
        FeatherObj value = ops->var.get(interp, varName);
        if (!ops->list.is_nil(interp, value) &&
            ops->string.byte_length(interp, value) > 0 &&
            !is_script_complete(ops, interp, value)) {
          FeatherObj longFlag = dict_get_str(ops, interp, entry, K_LONG);
          FeatherObj shortFlag = dict_get_str(ops, interp, entry, K_SHORT);
          FeatherObj flagName = ops->string.byte_length(interp, longFlag) > 0 ? longFlag : shortFlag;
          FeatherObj msg = ops->string.intern(interp, "flag ", 5);
          if (ops->string.byte_length(interp, longFlag) > 0) {
            msg = ops->string.concat(interp, msg, ops->string.intern(interp, "--", 2));
          } else {
            msg = ops->string.concat(interp, msg, ops->string.intern(interp, "-", 1));
          }
          msg = ops->string.concat(interp, msg, flagName);
          msg = ops->string.concat(interp, msg, ops->string.intern(interp, " value must be a complete script", 32));
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

  /* Extract parsed spec from storage dict */
  FeatherObj parsedSpec = dict_get_str(ops, interp, specEntry, K_SPEC);

  /* Build full command name and navigate to subcommand spec */
  FeatherObj fullCmdName = cmdName;

  for (size_t i = 1; i < argc; i++) {
    FeatherObj subcmdName = ops->list.at(interp, args, i);
    int found = 0;

    size_t specLen = ops->list.length(interp, parsedSpec);
    for (size_t j = 0; j < specLen; j++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, j);

      if (!entry_is_type(ops, interp, entry, T_CMD)) continue;

      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
      if (ops->string.equal(interp, name, subcmdName)) {
        /* Found the subcommand - descend into it */
        parsedSpec = dict_get_str(ops, interp, entry, K_SPEC);
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

  /* Ensure ::usage namespace exists */
  FeatherObj usageNs = ops->string.intern(interp, S(USAGE_NS));
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Public C API for building usage specs (wraps internal API)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create an argument entry.
 * Name format: "<name>" (required), "?name?" (optional), with optional "..." suffix for variadic.
 */
FeatherObj feather_usage_arg(const FeatherHostOps *ops, FeatherInterp interp,
                             const char *name) {
  FeatherObj nameObj = ops->string.intern(interp, name, feather_strlen(name));
  return usage_arg_from_obj(ops, interp, nameObj);
}

/**
 * Create a flag entry.
 * short_flag: "-v" or NULL
 * long_flag: "--verbose" or NULL
 * value: "<val>" (required), "?val?" (optional), or NULL (boolean)
 */
FeatherObj feather_usage_flag(const FeatherHostOps *ops, FeatherInterp interp,
                              const char *short_flag,
                              const char *long_flag,
                              const char *value) {
  FeatherObj shortObj = ops->string.intern(interp, "", 0);
  FeatherObj longObj = ops->string.intern(interp, "", 0);
  int hasValue = 0;
  int valueRequired = 0;

  /* Parse short flag: skip leading dash */
  if (short_flag && short_flag[0] == '-' && short_flag[1] != '\0') {
    shortObj = ops->string.intern(interp, short_flag + 1, feather_strlen(short_flag) - 1);
  }

  /* Parse long flag: skip leading dashes */
  if (long_flag && long_flag[0] == '-' && long_flag[1] == '-' && long_flag[2] != '\0') {
    longObj = ops->string.intern(interp, long_flag + 2, feather_strlen(long_flag) - 2);
  }

  /* Parse value spec */
  if (value) {
    size_t valLen = feather_strlen(value);
    if (valLen >= 2) {
      if (value[0] == '<' && value[valLen - 1] == '>') {
        hasValue = 1;
        valueRequired = 1;
      } else if (value[0] == '?' && value[valLen - 1] == '?') {
        hasValue = 1;
        valueRequired = 0;
      }
    }
  }

  return usage_flag_from_parts(ops, interp, shortObj, longObj, hasValue, valueRequired);
}

/**
 * Create a subcommand entry.
 */
FeatherObj feather_usage_cmd(const FeatherHostOps *ops, FeatherInterp interp,
                             const char *name,
                             FeatherObj subspec) {
  FeatherObj nameObj = ops->string.intern(interp, name, feather_strlen(name));
  return usage_cmd_from_obj(ops, interp, nameObj, subspec);
}

/**
 * Create an example entry.
 */
FeatherObj feather_usage_example(const FeatherHostOps *ops, FeatherInterp interp,
                                 const char *code,
                                 const char *header,
                                 const char *help) {
  FeatherObj codeObj = ops->string.intern(interp, code, feather_strlen(code));
  FeatherObj headerObj = ops->string.intern(interp, "", 0);
  FeatherObj helpObj = ops->string.intern(interp, "", 0);
  if (header) {
    headerObj = ops->string.intern(interp, header, feather_strlen(header));
  }
  if (help) {
    helpObj = ops->string.intern(interp, help, feather_strlen(help));
  }
  return usage_example_from_parts(ops, interp, codeObj, headerObj, helpObj);
}

/**
 * Set help text on an entry.
 */
FeatherObj feather_usage_help(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry, const char *text) {
  return usage_set_help(ops, interp, entry,
                        ops->string.intern(interp, text, feather_strlen(text)));
}

/**
 * Set extended help text on an entry.
 */
FeatherObj feather_usage_long_help(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj entry, const char *text) {
  return usage_set_long_help(ops, interp, entry,
                             ops->string.intern(interp, text, feather_strlen(text)));
}

/**
 * Set default value on an arg entry.
 */
FeatherObj feather_usage_default(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry, const char *value) {
  return usage_set_default(ops, interp, entry,
                           ops->string.intern(interp, value, feather_strlen(value)));
}

/**
 * Set valid choices on an entry.
 */
FeatherObj feather_usage_choices(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj entry, FeatherObj choices) {
  return usage_set_choices(ops, interp, entry, choices);
}

/**
 * Set value type hint on an entry.
 */
FeatherObj feather_usage_type(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry, const char *type) {
  return usage_set_type(ops, interp, entry,
                        ops->string.intern(interp, type, feather_strlen(type)));
}

/**
 * Mark an entry as hidden from help output.
 */
FeatherObj feather_usage_hide(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj entry) {
  return usage_set_hide(ops, interp, entry);
}

/**
 * Create an empty usage spec.
 */
FeatherObj feather_usage_spec(const FeatherHostOps *ops, FeatherInterp interp) {
  return ops->list.create(interp);
}

/**
 * Add an entry to a spec.
 */
FeatherObj feather_usage_add(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj spec, FeatherObj entry) {
  return ops->list.push(interp, spec, entry);
}

/**
 * Register a spec for a command.
 */
void feather_usage_register(const FeatherHostOps *ops, FeatherInterp interp,
                            const char *cmdname, FeatherObj spec) {
  /* Ensure ::usage namespace exists */
  FeatherObj usageNs = ops->string.intern(interp, S(USAGE_NS));
  ops->ns.create(interp, usageNs);

  /* Get existing specs dict */
  FeatherObj specs = usage_get_specs(ops, interp);

  /* Generate string representation for round-tripping */
  FeatherObj origSpec = spec_to_list(ops, interp, spec);

  /* Store both original (generated) and parsed in a dict */
  FeatherObj specEntry = ops->dict.create(interp);
  specEntry = dict_set_str(ops, interp, specEntry, K_ORIG, origSpec);
  specEntry = dict_set_str(ops, interp, specEntry, K_SPEC, spec);

  FeatherObj cmdKey = ops->string.intern(interp, cmdname, feather_strlen(cmdname));
  specs = ops->dict.set(interp, specs, cmdKey, specEntry);

  /* Save back */
  usage_set_specs(ops, interp, specs);
}
