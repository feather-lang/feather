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
static const char *K_CLAUSE     = "clause";  /* Subcommand is a syntax clause, not first-arg */
static const char *K_VALUE_TYPE = "value_type";
static const char *K_SHORT      = "short";
static const char *K_LONG       = "long";
static const char *K_HAS_VALUE  = "has_value";
static const char *K_VALUE_REQ  = "value_required";
static const char *K_VAR_NAME   = "var_name";
static const char *K_SPEC       = "spec";
static const char *K_ORIG       = "orig";  /* Original spec string for round-tripping */
static const char *K_ABOUT      = "about"; /* Short description for NAME section */

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
static const char *T_META    = "meta"; /* Spec-level metadata (about, description) */
static const char *T_SECTION = "section"; /* Custom section with header and content */

/* Completion type values */
static const char *T_COMMAND     = "command";
static const char *T_SUBCOMMAND  = "subcommand";
static const char *T_VALUE       = "value";
static const char *T_ARG_PLACEHOLDER = "arg-placeholder";

/* Section entry keys */
static const char *K_SECTION_NAME = "section_name";
static const char *K_CONTENT      = "content";

/* Completion entry keys */
static const char *K_TEXT = "text";

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
 * Lazy usage registration - dispatch table mapping command names to registration functions.
 */
typedef void (*UsageRegistrationFunc)(const FeatherHostOps*, FeatherInterp);

typedef struct {
  const char *name;
  UsageRegistrationFunc register_fn;
} UsageRegistration;

static const UsageRegistration usage_registrations[] = {
  {"set", feather_register_set_usage},
  {"expr", feather_register_expr_usage},
  {"proc", feather_register_proc_usage},
  {"if", feather_register_if_usage},
  {"while", feather_register_while_usage},
  {"for", feather_register_for_usage},
  {"foreach", feather_register_foreach_usage},
  {"lmap", feather_register_lmap_usage},
  {"lassign", feather_register_lassign_usage},
  {"linsert", feather_register_linsert_usage},
  {"switch", feather_register_switch_usage},
  {"tailcall", feather_register_tailcall_usage},
  {"break", feather_register_break_usage},
  {"continue", feather_register_continue_usage},
  {"incr", feather_register_incr_usage},
  {"llength", feather_register_llength_usage},
  {"lindex", feather_register_lindex_usage},
  {"lreplace", feather_register_lreplace_usage},
  {"return", feather_register_return_usage},
  {"error", feather_register_error_usage},
  {"catch", feather_register_catch_usage},
  {"info", feather_register_info_usage},
  {"upvar", feather_register_upvar_usage},
  {"uplevel", feather_register_uplevel_usage},
  {"rename", feather_register_rename_usage},
  {"namespace", feather_register_namespace_usage},
  {"variable", feather_register_variable_usage},
  {"global", feather_register_global_usage},
  {"apply", feather_register_apply_usage},
  {"throw", feather_register_throw_usage},
  {"try", feather_register_try_usage},
  {"trace", feather_register_trace_usage},
  {"list", feather_register_list_usage},
  {"lrange", feather_register_lrange_usage},
  {"lappend", feather_register_lappend_usage},
  {"lset", feather_register_lset_usage},
  {"lreverse", feather_register_lreverse_usage},
  {"lrepeat", feather_register_lrepeat_usage},
  {"lsort", feather_register_lsort_usage},
  {"lsearch", feather_register_lsearch_usage},
  {"string", feather_register_string_usage},
  {"split", feather_register_split_usage},
  {"join", feather_register_join_usage},
  {"concat", feather_register_concat_usage},
  {"append", feather_register_append_usage},
  {"unset", feather_register_unset_usage},
  {"dict", feather_register_dict_usage},
  {"format", feather_register_format_usage},
  {"scan", feather_register_scan_usage},
  {"subst", feather_register_subst_usage},
  {"eval", feather_register_eval_usage},
  {"usage", feather_register_usage_usage},
  {"help", feather_register_help_usage},
  {"tcl::mathfunc", feather_register_mathfunc_usage},
  {NULL, NULL}
};

/**
 * Ensure a command's usage spec is registered (lazy loading).
 * Called before looking up a spec to register it on-demand.
 */
void feather_ensure_usage_registered(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj cmdName) {
  /* Ensure ::usage namespace exists */
  FeatherObj usageNs = ops->string.intern(interp, S(USAGE_NS));
  ops->ns.create(interp, usageNs);

  /* Check if already registered */
  FeatherObj specs = usage_get_specs(ops, interp);
  if (!ops->list.is_nil(interp, ops->dict.get(interp, specs, cmdName))) {
    return; /* Already registered */
  }

  /* Extract command name as C string for lookup */
  size_t len = ops->string.byte_length(interp, cmdName);
  char namebuf[64];
  if (len >= sizeof(namebuf)) return; /* Name too long */

  for (size_t i = 0; i < len; i++) {
    namebuf[i] = (char)ops->string.byte_at(interp, cmdName, i);
  }
  namebuf[len] = '\0';

  /* Look up and call registration function */
  for (const UsageRegistration *r = usage_registrations; r->name != NULL; r++) {
    if (feather_str_eq(namebuf, len, r->name)) {
      r->register_fn(ops, interp);
      return;
    }
  }
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
         feather_obj_eq_literal(ops, interp, token, "example") ||
         feather_obj_eq_literal(ops, interp, token, "help") ||
         feather_obj_eq_literal(ops, interp, token, "long_help");
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

static FeatherObj usage_set_clause(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj entry) {
  return dict_set_int(ops, interp, entry, K_CLAUSE, 1);
}

/**
 * Parse a spec list into a structured representation.
 *
 * New block-based format:
 *   flag -s --long <value> { options }
 *   arg <name> { options }
 *   cmd name { body } { options }
 *
 * Returns a list of dict entries.
 */
static FeatherObj parse_spec_from_list(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj specList) {
  FeatherObj result = ops->list.create(interp);
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

      /* Check for 'hide', 'help', 'long_help' keywords inside body and extract them before parsing */
      int hide = 0;
      FeatherObj helpText = ops->string.intern(interp, "", 0);
      FeatherObj longHelp = ops->string.intern(interp, "", 0);
      FeatherObj bodyList = feather_list_parse_obj(ops, interp, cmdBody);
      size_t bodyLen = ops->list.length(interp, bodyList);
      FeatherObj filteredBody = ops->list.create(interp);

      for (size_t j = 0; j < bodyLen; j++) {
        FeatherObj token = ops->list.at(interp, bodyList, j);
        if (feather_obj_eq_literal(ops, interp, token, "hide")) {
          hide = 1;
        } else if (feather_obj_eq_literal(ops, interp, token, "help")) {
          /* Next token is the help text */
          if (j + 1 < bodyLen) {
            helpText = ops->list.at(interp, bodyList, j + 1);
            j++; /* Skip next token */
          }
        } else if (feather_obj_eq_literal(ops, interp, token, "long_help")) {
          /* Next token is the long help text */
          if (j + 1 < bodyLen) {
            longHelp = ops->list.at(interp, bodyList, j + 1);
            j++; /* Skip next token */
          }
        } else {
          filteredBody = ops->list.push(interp, filteredBody, token);
        }
      }

      /* Check for options block - cmd supports before/after help */
      FeatherObj beforeHelp = ops->string.intern(interp, "", 0);
      FeatherObj afterHelp = ops->string.intern(interp, "", 0);
      FeatherObj beforeLongHelp = ops->string.intern(interp, "", 0);
      FeatherObj afterLongHelp = ops->string.intern(interp, "", 0);

      if (i < specLen) {
        FeatherObj next = ops->list.at(interp, specList, i);
        if (!is_keyword(ops, interp, next)) {
          /* This is the options block */
          parse_options_block(ops, interp, next, &helpText, &longHelp, NULL, NULL, NULL, &hide,
                              &beforeHelp, &afterHelp, &beforeLongHelp, &afterLongHelp);
          i++;
        }
      }

      /* Recursively parse the filtered subcommand body */
      FeatherObj subSpec = parse_spec_from_list(ops, interp, filteredBody);

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
 * Parse a spec string into a structured representation.
 * Wrapper that parses the string and calls parse_spec_from_list.
 */
static FeatherObj parse_spec(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj specStr) {
  FeatherObj specList = feather_list_parse_obj(ops, interp, specStr);
  return parse_spec_from_list(ops, interp, specList);
}

/**
 * Second pass of parse_spec: handle help/long_help keywords and create meta entry.
 */
static FeatherObj parse_spec_meta(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj specStr, FeatherObj entries) {
  FeatherObj specList = feather_list_parse_obj(ops, interp, specStr);
  size_t specLen = ops->list.length(interp, specList);

  FeatherObj helpVal = 0;
  FeatherObj longHelpVal = 0;

  size_t i = 0;
  while (i < specLen) {
    FeatherObj keyword = ops->list.at(interp, specList, i);
    i++;

    if (feather_obj_eq_literal(ops, interp, keyword, "help")) {
      if (i < specLen) {
        helpVal = ops->list.at(interp, specList, i);
        i++;
      }
    } else if (feather_obj_eq_literal(ops, interp, keyword, "long_help")) {
      if (i < specLen) {
        longHelpVal = ops->list.at(interp, specList, i);
        i++;
      }
    }
  }

  /* If we found help or long_help, create a meta entry and prepend it */
  if (!ops->list.is_nil(interp, helpVal) || !ops->list.is_nil(interp, longHelpVal)) {
    FeatherObj meta = ops->dict.create(interp);
    meta = dict_set_str(ops, interp, meta, K_TYPE, ops->string.intern(interp, S(T_META)));

    if (!ops->list.is_nil(interp, helpVal)) {
      meta = dict_set_str(ops, interp, meta, K_ABOUT, helpVal);
    }
    if (!ops->list.is_nil(interp, longHelpVal)) {
      meta = dict_set_str(ops, interp, meta, K_LONG_HELP, longHelpVal);
    }

    /* Prepend meta entry to the result */
    FeatherObj newResult = ops->list.create(interp);
    newResult = ops->list.push(interp, newResult, meta);
    size_t entriesLen = ops->list.length(interp, entries);
    for (size_t j = 0; j < entriesLen; j++) {
      newResult = ops->list.push(interp, newResult, ops->list.at(interp, entries, j));
    }
    return newResult;
  }

  return entries;
}

/**
 * Helper to append a string literal to a builder.
 */
static void append_str(const FeatherHostOps *ops, FeatherInterp interp,
                       FeatherObj builder, const char *s) {
  while (*s) ops->string.builder_append_byte(interp, builder, *s++);
}

/**
 * Append text with word wrapping at specified width.
 * indent: string to prepend to each new line (e.g., "       " for 7 spaces)
 * width: max characters per line (not including indent on continuation lines)
 */
static void append_wrapped(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj builder, FeatherObj text,
                           const char *indent, size_t width) {
  size_t len = ops->string.byte_length(interp, text);
  size_t col = 0;
  size_t i = 0;

  while (i < len) {
    /* Find end of current word */
    size_t wordStart = i;
    int ch;
    while (i < len) {
      ch = ops->string.byte_at(interp, text, i);
      if (ch == ' ' || ch == '\n') break;
      i++;
    }
    size_t wordLen = i - wordStart;

    /* Check if word fits on current line */
    if (col > 0 && col + 1 + wordLen > width) {
      /* Wrap to new line */
      ops->string.builder_append_byte(interp, builder, '\n');
      append_str(ops, interp, builder, indent);
      col = 0;
    } else if (col > 0) {
      /* Add space before word */
      ops->string.builder_append_byte(interp, builder, ' ');
      col++;
    }

    /* Append the word */
    for (size_t j = wordStart; j < wordStart + wordLen; j++) {
      ops->string.builder_append_byte(interp, builder, ops->string.byte_at(interp, text, j));
    }
    col += wordLen;

    /* Skip whitespace, but detect paragraph breaks (\n\n) */
    int newlineCount = 0;
    while (i < len) {
      ch = ops->string.byte_at(interp, text, i);
      if (ch == '\n') {
        newlineCount++;
        i++;
      } else if (ch == ' ') {
        i++;
      } else {
        break;
      }
    }

    /* If we saw 2+ newlines, insert a paragraph break */
    if (newlineCount >= 2 && i < len) {
      ops->string.builder_append_byte(interp, builder, '\n');
      ops->string.builder_append_byte(interp, builder, '\n');
      append_str(ops, interp, builder, indent);
      col = 0;
    }
  }
}

/**
 * Append text verbatim, indenting each line after a newline.
 * Used for code examples where we want to preserve formatting but add indentation.
 */
static void append_indented_verbatim(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj builder, FeatherObj text,
                                      const char *indent) {
  size_t len = ops->string.byte_length(interp, text);
  for (size_t i = 0; i < len; i++) {
    int ch = ops->string.byte_at(interp, text, i);
    ops->string.builder_append_byte(interp, builder, ch);
    if (ch == '\n' && i + 1 < len) {
      append_str(ops, interp, builder, indent);
    }
  }
}

/**
 * Generate usage string for display (--help output)
 * Follows standard Unix manpage format with NAME, SYNOPSIS, DESCRIPTION, etc.
 */
static FeatherObj generate_usage_string(const FeatherHostOps *ops, FeatherInterp interp,
                                         FeatherObj cmdName, FeatherObj parsedSpec) {
  FeatherObj builder = ops->string.builder_new(interp, 512);
  size_t specLen = ops->list.length(interp, parsedSpec);

  /* Check for features in spec and find meta entry */
  int hasFlags = 0;
  int hasArgs = 0;
  int hasSubcmds = 0;  /* True subcommands (appear as first arg) */
  int hasClauses = 0;  /* Clause subcommands (appear after other args) */
  int hasExamples = 0;
  FeatherObj aboutText = ops->string.intern(interp, "", 0);
  FeatherObj descriptionText = ops->string.intern(interp, "", 0);

  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);
    if (entry_is_type(ops, interp, entry, T_META)) {
      aboutText = dict_get_str(ops, interp, entry, K_ABOUT);
      descriptionText = dict_get_str(ops, interp, entry, K_LONG_HELP);
    }
    if (entry_is_type(ops, interp, entry, T_FLAG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (!hide) hasFlags = 1;
    }
    if (entry_is_type(ops, interp, entry, T_ARG)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      if (!hide) hasArgs = 1;
    }
    if (entry_is_type(ops, interp, entry, T_CMD)) {
      int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
      int64_t isClause = dict_get_int(ops, interp, entry, K_CLAUSE);
      if (!hide) {
        if (isClause) hasClauses = 1;
        else hasSubcmds = 1;
      }
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
  if (ops->string.byte_length(interp, aboutText) > 0) {
    append_str(ops, interp, builder, " - ");
    FeatherObj trimmed = trim_text_block(ops, interp, aboutText);
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

  /* === DESCRIPTION section (uses long_help from meta entry) === */
  if (ops->string.byte_length(interp, descriptionText) > 0) {
    append_str(ops, interp, builder, "\n\nDESCRIPTION\n       ");
    FeatherObj trimmed = trim_text_block(ops, interp, descriptionText);
    append_wrapped(ops, interp, builder, trimmed, "       ", 65);
  }

  /* === Custom SECTIONS (appear after DESCRIPTION, except SEE ALSO) === */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);

    if (entry_is_type(ops, interp, entry, T_SECTION)) {
      FeatherObj sectionName = dict_get_str(ops, interp, entry, K_SECTION_NAME);
      FeatherObj content = dict_get_str(ops, interp, entry, K_CONTENT);

      /* Skip SEE ALSO - it will be rendered at the very end */
      FeatherObj lower = ops->rune.to_lower(interp, sectionName);
      if (feather_obj_eq_literal(ops, interp, lower, "see also")) continue;

      if (ops->string.byte_length(interp, sectionName) > 0) {
        append_str(ops, interp, builder, "\n\n");
        /* Output section name in uppercase */
        FeatherObj upper = ops->rune.to_upper(interp, sectionName);
        ops->string.builder_append_obj(interp, builder, upper);
        append_str(ops, interp, builder, "\n       ");
        FeatherObj trimmed = trim_text_block(ops, interp, content);
        append_wrapped(ops, interp, builder, trimmed, "       ", 65);
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
        append_wrapped(ops, interp, builder, trimmed, "              ", 58);
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
          append_wrapped(ops, interp, builder, trimmed, "              ", 58);
        }

        if (ops->string.byte_length(interp, choices) > 0) {
          append_str(ops, interp, builder, "\n              Choices: ");
          ops->string.builder_append_obj(interp, builder, choices);
        }
      }
    }
  }

  /* === COMMANDS section === */
  if (hasSubcmds || hasClauses) {
    append_str(ops, interp, builder, "\n\nCOMMANDS");
    int cmdCount = 0;

    for (size_t i = 0; i < specLen; i++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, i);

      if (entry_is_type(ops, interp, entry, T_CMD)) {
        int64_t hide = dict_get_int(ops, interp, entry, K_HIDE);
        if (hide) continue;

        /* Add blank line between commands for readability */
        if (cmdCount > 0) {
          ops->string.builder_append_byte(interp, builder, '\n');
        }
        cmdCount++;

        FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
        FeatherObj subspec = dict_get_str(ops, interp, entry, K_SPEC);
        FeatherObj helpText = dict_get_str(ops, interp, entry, K_HELP);
        FeatherObj longHelp = dict_get_str(ops, interp, entry, K_LONG_HELP);

        /* Build signature line: cmdName subcmdName ?arg1? <arg2>... */
        append_str(ops, interp, builder, "\n       ");
        ops->string.builder_append_obj(interp, builder, cmdName);
        ops->string.builder_append_byte(interp, builder, ' ');
        ops->string.builder_append_obj(interp, builder, name);

        /* Add arguments from subspec to signature */
        if (!ops->list.is_nil(interp, subspec)) {
          size_t subLen = ops->list.length(interp, subspec);
          for (size_t j = 0; j < subLen; j++) {
            FeatherObj subEntry = ops->list.at(interp, subspec, j);
            if (entry_is_type(ops, interp, subEntry, T_ARG)) {
              int64_t subHide = dict_get_int(ops, interp, subEntry, K_HIDE);
              if (subHide) continue;

              FeatherObj argName = dict_get_str(ops, interp, subEntry, K_NAME);
              int64_t required = dict_get_int(ops, interp, subEntry, K_REQUIRED);
              int64_t variadic = dict_get_int(ops, interp, subEntry, K_VARIADIC);

              ops->string.builder_append_byte(interp, builder, ' ');
              if (required) {
                ops->string.builder_append_byte(interp, builder, '<');
                ops->string.builder_append_obj(interp, builder, argName);
                ops->string.builder_append_byte(interp, builder, '>');
              } else {
                ops->string.builder_append_byte(interp, builder, '?');
                ops->string.builder_append_obj(interp, builder, argName);
                ops->string.builder_append_byte(interp, builder, '?');
              }
              if (variadic) {
                append_str(ops, interp, builder, "...");
              }
            } else if (entry_is_type(ops, interp, subEntry, T_FLAG)) {
              int64_t subHide = dict_get_int(ops, interp, subEntry, K_HIDE);
              if (subHide) continue;

              FeatherObj shortFlag = dict_get_str(ops, interp, subEntry, K_SHORT);
              FeatherObj longFlag = dict_get_str(ops, interp, subEntry, K_LONG);
              int64_t hasValue = dict_get_int(ops, interp, subEntry, K_HAS_VALUE);
              FeatherObj varName = dict_get_str(ops, interp, subEntry, K_VAR_NAME);

              ops->string.builder_append_byte(interp, builder, ' ');
              ops->string.builder_append_byte(interp, builder, '?');
              if (ops->string.byte_length(interp, shortFlag) > 0) {
                ops->string.builder_append_byte(interp, builder, '-');
                ops->string.builder_append_obj(interp, builder, shortFlag);
              } else if (ops->string.byte_length(interp, longFlag) > 0) {
                append_str(ops, interp, builder, "--");
                ops->string.builder_append_obj(interp, builder, longFlag);
              }
              /* Include the value placeholder if the flag takes a value */
              if (hasValue && ops->string.byte_length(interp, varName) > 0) {
                ops->string.builder_append_byte(interp, builder, ' ');
                ops->string.builder_append_obj(interp, builder, varName);
              }
              ops->string.builder_append_byte(interp, builder, '?');
            }
          }
        }

        /* Use long_help if available, otherwise fall back to help */
        FeatherObj descText = longHelp;
        if (ops->string.byte_length(interp, descText) == 0) {
          descText = helpText;
        }

        if (ops->string.byte_length(interp, descText) > 0) {
          append_str(ops, interp, builder, "\n              ");
          FeatherObj trimmed = trim_text_block(ops, interp, descText);
          append_wrapped(ops, interp, builder, trimmed, "              ", 58);
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
          append_indented_verbatim(ops, interp, builder, trimmed, "           ");
        }
      }
    }
  }

  /* === SEE ALSO section (always last) === */
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, parsedSpec, i);

    if (entry_is_type(ops, interp, entry, T_SECTION)) {
      FeatherObj sectionName = dict_get_str(ops, interp, entry, K_SECTION_NAME);
      FeatherObj lower = ops->rune.to_lower(interp, sectionName);
      if (feather_obj_eq_literal(ops, interp, lower, "see also")) {
        FeatherObj content = dict_get_str(ops, interp, entry, K_CONTENT);
        append_str(ops, interp, builder, "\n\nSEE ALSO\n       ");
        FeatherObj trimmed = trim_text_block(ops, interp, content);
        append_wrapped(ops, interp, builder, trimmed, "       ", 65);
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
 * Convert a parsed meta entry back to list format.
 * Returns a list of tokens like: help {short desc} long_help {detailed desc}
 * These are flattened into the result, not nested.
 */
static FeatherObj meta_to_list(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj entry) {
  FeatherObj result = ops->list.create(interp);

  /* about -> help */
  FeatherObj about = dict_get_str(ops, interp, entry, K_ABOUT);
  if (ops->string.byte_length(interp, about) > 0) {
    result = ops->list.push(interp, result, ops->string.intern(interp, S(K_HELP)));
    result = ops->list.push(interp, result, about);
  }

  /* long_help */
  FeatherObj longHelp = dict_get_str(ops, interp, entry, K_LONG_HELP);
  if (ops->string.byte_length(interp, longHelp) > 0) {
    result = ops->list.push(interp, result, ops->string.intern(interp, S(K_LONG_HELP)));
    result = ops->list.push(interp, result, longHelp);
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
    } else if (feather_obj_eq_literal(ops, interp, typeVal, T_META)) {
      entryList = meta_to_list(ops, interp, entry);
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

  if (argc == 1) {
    /* Get mode: lazy-load and return original spec string for round-tripping */
    feather_ensure_usage_registered(ops, interp, cmdName);
    FeatherObj specs = usage_get_specs(ops, interp);
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

  /* Handle top-level help/long_help keywords */
  parsed = parse_spec_meta(ops, interp, specStr, parsed);

  /* Store both original and parsed in a dict for round-tripping */
  FeatherObj specEntry = ops->dict.create(interp);
  specEntry = dict_set_str(ops, interp, specEntry, K_ORIG, specStr);
  specEntry = dict_set_str(ops, interp, specEntry, K_SPEC, parsed);

  FeatherObj specs = usage_get_specs(ops, interp);
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

  /* Lazy-load the usage spec if not already registered */
  feather_ensure_usage_registered(ops, interp, cmdName);

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

  /* Lazy-load the usage spec if not already registered */
  feather_ensure_usage_registered(ops, interp, cmdName);

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
  FeatherObj parentCmdName = cmdName; /* Track parent for SEE ALSO */
  FeatherObj subcmdLongHelp = ops->string.intern(interp, "", 0);
  FeatherObj subcmdHelp = ops->string.intern(interp, "", 0);

  for (size_t i = 1; i < argc; i++) {
    FeatherObj subcmdName = ops->list.at(interp, args, i);
    int found = 0;

    size_t specLen = ops->list.length(interp, parsedSpec);
    for (size_t j = 0; j < specLen; j++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, j);

      if (!entry_is_type(ops, interp, entry, T_CMD)) continue;

      FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
      if (ops->string.equal(interp, name, subcmdName)) {
        /* Found the subcommand - capture its description before descending */
        subcmdLongHelp = dict_get_str(ops, interp, entry, K_LONG_HELP);
        subcmdHelp = dict_get_str(ops, interp, entry, K_HELP);
        parentCmdName = fullCmdName;
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

  /* If we navigated to a subcommand, build a synthetic spec with description */
  if (argc > 1) {
    FeatherObj newSpec = ops->list.create(interp);

    /* Add meta entry with the subcommand's description */
    if (ops->string.byte_length(interp, subcmdLongHelp) > 0 ||
        ops->string.byte_length(interp, subcmdHelp) > 0) {
      FeatherObj meta = ops->dict.create(interp);
      meta = dict_set_str(ops, interp, meta, K_TYPE, ops->string.intern(interp, S(T_META)));

      /* Use long_help if available, otherwise use help */
      FeatherObj desc = subcmdLongHelp;
      if (ops->string.byte_length(interp, desc) == 0) {
        desc = subcmdHelp;
      }
      meta = dict_set_str(ops, interp, meta, K_LONG_HELP, desc);
      newSpec = ops->list.push(interp, newSpec, meta);
    }

    /* Copy all entries from the subspec */
    size_t subLen = ops->list.length(interp, parsedSpec);
    for (size_t j = 0; j < subLen; j++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, j);
      newSpec = ops->list.push(interp, newSpec, entry);
    }

    /* Add a section for SEE ALSO */
    FeatherObj seeAlso = ops->dict.create(interp);
    seeAlso = dict_set_str(ops, interp, seeAlso, K_TYPE, ops->string.intern(interp, S(T_SECTION)));
    seeAlso = dict_set_str(ops, interp, seeAlso, K_SECTION_NAME,
                           ops->string.intern(interp, "See Also", 8));
    FeatherObj seeAlsoContent = ops->string.concat(interp, parentCmdName,
                                                    ops->string.intern(interp, "(1)", 3));
    seeAlso = dict_set_str(ops, interp, seeAlso, K_CONTENT, seeAlsoContent);
    newSpec = ops->list.push(interp, newSpec, seeAlso);

    parsedSpec = newSpec;
  }

  FeatherObj helpStr = generate_usage_string(ops, interp, fullCmdName, parsedSpec);

  ops->interp.set_result(interp, helpStr);
  return TCL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Completion Support Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a completion entry dict: {text <str> type <type> help <help>}
 */
static FeatherObj make_completion(const FeatherHostOps *ops, FeatherInterp interp,
                                   const char *text, const char *type, FeatherObj help) {
  FeatherObj dict = ops->dict.create(interp);

  FeatherObj textObj = ops->string.intern(interp, text, feather_strlen(text));
  dict = dict_set_str(ops, interp, dict, K_TEXT, textObj);

  FeatherObj typeObj = ops->string.intern(interp, type, feather_strlen(type));
  dict = dict_set_str(ops, interp, dict, K_TYPE, typeObj);

  dict = dict_set_str(ops, interp, dict, K_HELP, help);

  return dict;
}

/**
 * Create an arg placeholder entry: {text {} type arg-placeholder name <name> help <help>}
 */
static FeatherObj make_arg_placeholder(const FeatherHostOps *ops, FeatherInterp interp,
                                        const char *name, FeatherObj help) {
  FeatherObj dict = ops->dict.create(interp);

  FeatherObj emptyText = ops->string.intern(interp, "", 0);
  dict = dict_set_str(ops, interp, dict, K_TEXT, emptyText);

  FeatherObj typeObj = ops->string.intern(interp, S(T_ARG_PLACEHOLDER));
  dict = dict_set_str(ops, interp, dict, K_TYPE, typeObj);

  FeatherObj nameObj = ops->string.intern(interp, name, feather_strlen(name));
  dict = dict_set_str(ops, interp, dict, K_NAME, nameObj);

  dict = dict_set_str(ops, interp, dict, K_HELP, help);

  return dict;
}

/**
 * Check if a string starts with a prefix (case-sensitive)
 */
static int str_has_prefix(const char *str, size_t str_len,
                          const char *prefix, size_t prefix_len) {
  if (prefix_len > str_len) {
    return 0;
  }
  for (size_t i = 0; i < prefix_len; i++) {
    if (str[i] != prefix[i]) {
      return 0;
    }
  }
  return 1;
}

/**
 * Check if a FeatherObj string starts with a prefix (case-sensitive)
 */
static int obj_has_prefix(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj obj, FeatherObj prefix) {
  size_t obj_len = ops->string.byte_length(interp, obj);
  size_t prefix_len = ops->string.byte_length(interp, prefix);

  if (prefix_len > obj_len) {
    return 0;
  }

  for (size_t i = 0; i < prefix_len; i++) {
    int obj_ch = ops->string.byte_at(interp, obj, i);
    int prefix_ch = ops->string.byte_at(interp, prefix, i);
    if (obj_ch != prefix_ch) {
      return 0;
    }
  }

  return 1;
}

/**
 * Compare two FeatherObj strings (returns <0, 0, >0 like strcmp)
 */
static int obj_strcmp(const FeatherHostOps *ops, FeatherInterp interp,
                      FeatherObj a, FeatherObj b) {
  size_t lenA = ops->string.byte_length(interp, a);
  size_t lenB = ops->string.byte_length(interp, b);

  size_t minLen = lenA < lenB ? lenA : lenB;
  for (size_t i = 0; i < minLen; i++) {
    int chA = ops->string.byte_at(interp, a, i);
    int chB = ops->string.byte_at(interp, b, i);
    if (chA < chB) return -1;
    if (chA > chB) return 1;
  }

  if (lenA < lenB) return -1;
  if (lenA > lenB) return 1;
  return 0;
}

/**
 * Complete command names from registered usage specs.
 * Returns list of {text <cmd> type command help <...>} dicts.
 * Results are sorted alphabetically.
 */
static FeatherObj complete_commands(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj prefix) {
  FeatherObj result = ops->list.create(interp);
  FeatherObj specs = usage_get_specs(ops, interp);

  /* Get all command names (dict keys) */
  FeatherObj keys = ops->dict.keys(interp, specs);
  size_t numKeys = ops->list.length(interp, keys);

  /* Collect matching commands */
  FeatherObj matches = ops->list.create(interp);
  for (size_t i = 0; i < numKeys; i++) {
    FeatherObj cmdName = ops->list.at(interp, keys, i);

    /* Filter by prefix */
    if (obj_has_prefix(ops, interp, cmdName, prefix)) {
      matches = ops->list.push(interp, matches, cmdName);
    }
  }

  /* Sort matches alphabetically using bubble sort */
  size_t numMatches = ops->list.length(interp, matches);
  if (numMatches > 1) {
    /* We need to rebuild the list for sorting since list.set doesn't exist */
    int sorted = 0;
    while (!sorted) {
      sorted = 1;
      FeatherObj newMatches = ops->list.create(interp);
      for (size_t i = 0; i < numMatches; i++) {
        FeatherObj curr = ops->list.at(interp, matches, i);

        if (i + 1 < numMatches) {
          FeatherObj next = ops->list.at(interp, matches, i + 1);
          if (obj_strcmp(ops, interp, curr, next) > 0) {
            /* Swap - add next, then curr (we'll skip next in next iteration) */
            newMatches = ops->list.push(interp, newMatches, next);
            newMatches = ops->list.push(interp, newMatches, curr);
            i++; /* Skip next element since we already added it */
            sorted = 0;
          } else {
            newMatches = ops->list.push(interp, newMatches, curr);
          }
        } else {
          newMatches = ops->list.push(interp, newMatches, curr);
        }
      }
      matches = newMatches;
    }
  }

  /* Create completion entries */
  for (size_t i = 0; i < numMatches; i++) {
    FeatherObj cmdName = ops->list.at(interp, matches, i);
    FeatherObj specEntry = ops->dict.get(interp, specs, cmdName);

    /* Get help text from spec entry if available */
    FeatherObj help = ops->string.intern(interp, "", 0);
    if (!ops->list.is_nil(interp, specEntry)) {
      help = dict_get_str(ops, interp, specEntry, K_HELP);
    }

    /* Convert cmdName to C string for make_completion */
    size_t cmdLen = ops->string.byte_length(interp, cmdName);
    char cmdbuf[256];
    if (cmdLen >= sizeof(cmdbuf)) cmdLen = sizeof(cmdbuf) - 1;
    for (size_t j = 0; j < cmdLen; j++) {
      cmdbuf[j] = (char)ops->string.byte_at(interp, cmdName, j);
    }
    cmdbuf[cmdLen] = '\0';

    FeatherObj completion = make_completion(ops, interp, cmdbuf, T_COMMAND, help);
    result = ops->list.push(interp, result, completion);
  }

  return result;
}

/**
 * Complete subcommand names from a spec.
 * Returns list of {text <subcmd> type subcommand help <...>} dicts.
 * Results are in spec order (not alphabetical).
 */
static FeatherObj complete_subcommands(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj spec, FeatherObj prefix) {
  FeatherObj result = ops->list.create(interp);

  /* Spec is a list of entries */
  size_t specLen = ops->list.length(interp, spec);
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, spec, i);
    FeatherObj entryType = dict_get_str(ops, interp, entry, K_TYPE);

    /* Only process cmd entries */
    if (!feather_obj_eq_literal(ops, interp, entryType, T_CMD)) {
      continue;
    }

    /* Check hide flag */
    int hide = dict_get_int(ops, interp, entry, K_HIDE);
    if (hide) {
      continue;
    }

    /* Get subcommand name */
    FeatherObj subcmdName = dict_get_str(ops, interp, entry, K_NAME);

    /* Filter by prefix */
    if (obj_has_prefix(ops, interp, subcmdName, prefix)) {
      /* Get help text */
      FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);

      /* Convert to C string */
      size_t nameLen = ops->string.byte_length(interp, subcmdName);
      char namebuf[256];
      if (nameLen >= sizeof(namebuf)) nameLen = sizeof(namebuf) - 1;
      for (size_t j = 0; j < nameLen; j++) {
        namebuf[j] = (char)ops->string.byte_at(interp, subcmdName, j);
      }
      namebuf[nameLen] = '\0';

      FeatherObj completion = make_completion(ops, interp, namebuf, T_SUBCOMMAND, help);
      result = ops->list.push(interp, result, completion);
    }
  }

  return result;
}

/**
 * Complete flag names from a spec.
 * Returns list of {text <flag> type flag help <...>} dicts, sorted alphabetically.
 */
static FeatherObj complete_flags(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj spec, FeatherObj prefix) {
  FeatherObj unsorted = ops->list.create(interp);

  /* Spec is a list of entries */
  size_t specLen = ops->list.length(interp, spec);
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, spec, i);
    FeatherObj entryType = dict_get_str(ops, interp, entry, K_TYPE);

    /* Only process flag entries */
    if (!feather_obj_eq_literal(ops, interp, entryType, T_FLAG)) {
      continue;
    }

    /* Check hide flag */
    int hide = dict_get_int(ops, interp, entry, K_HIDE);
    if (hide) {
      continue;
    }

    /* Get help text */
    FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);

    /* Add short flag if present */
    FeatherObj shortFlag = dict_get_str(ops, interp, entry, K_SHORT);
    if (ops->string.byte_length(interp, shortFlag) > 0) {
      /* Build -X format */
      size_t flagLen = ops->string.byte_length(interp, shortFlag);
      char flagbuf[64];
      flagbuf[0] = '-';
      for (size_t j = 0; j < flagLen && j < sizeof(flagbuf) - 2; j++) {
        flagbuf[j + 1] = (char)ops->string.byte_at(interp, shortFlag, j);
      }
      flagbuf[flagLen + 1] = '\0';

      /* Create flag string and check prefix */
      FeatherObj flagStr = ops->string.intern(interp, flagbuf, flagLen + 1);
      if (obj_has_prefix(ops, interp, flagStr, prefix)) {
        FeatherObj completion = make_completion(ops, interp, flagbuf, T_FLAG, help);
        unsorted = ops->list.push(interp, unsorted, completion);
      }
    }

    /* Add long flag if present */
    FeatherObj longFlag = dict_get_str(ops, interp, entry, K_LONG);
    if (ops->string.byte_length(interp, longFlag) > 0) {
      /* Build --XXX format */
      size_t flagLen = ops->string.byte_length(interp, longFlag);
      char flagbuf[64];
      flagbuf[0] = '-';
      flagbuf[1] = '-';
      for (size_t j = 0; j < flagLen && j < sizeof(flagbuf) - 3; j++) {
        flagbuf[j + 2] = (char)ops->string.byte_at(interp, longFlag, j);
      }
      flagbuf[flagLen + 2] = '\0';

      /* Create flag string and check prefix */
      FeatherObj flagStr = ops->string.intern(interp, flagbuf, flagLen + 2);
      if (obj_has_prefix(ops, interp, flagStr, prefix)) {
        FeatherObj completion = make_completion(ops, interp, flagbuf, T_FLAG, help);
        unsorted = ops->list.push(interp, unsorted, completion);
      }
    }
  }

  /* Sort flags alphabetically by text field */
  size_t numFlags = ops->list.length(interp, unsorted);
  if (numFlags == 0) {
    return unsorted;
  }

  if (numFlags == 1) {
    return unsorted;
  }

  /* Sort: short flags first (-X), then long flags (--XXX), alphabetically within each group */
  FeatherObj result = unsorted;
  int sorted = 0;
  while (!sorted) {
    sorted = 1;
    FeatherObj newResult = ops->list.create(interp);
    for (size_t i = 0; i < numFlags; i++) {
      FeatherObj curr = ops->list.at(interp, result, i);

      if (i + 1 < numFlags) {
        FeatherObj next = ops->list.at(interp, result, i + 1);

        /* Extract text fields for comparison */
        FeatherObj currText = dict_get_str(ops, interp, curr, K_TEXT);
        FeatherObj nextText = dict_get_str(ops, interp, next, K_TEXT);

        /* Determine if short or long flag */
        size_t currLen = ops->string.byte_length(interp, currText);
        size_t nextLen = ops->string.byte_length(interp, nextText);

        int currIsShort = (currLen >= 2 &&
                          ops->string.byte_at(interp, currText, 0) == '-' &&
                          ops->string.byte_at(interp, currText, 1) != '-');
        int nextIsShort = (nextLen >= 2 &&
                          ops->string.byte_at(interp, nextText, 0) == '-' &&
                          ops->string.byte_at(interp, nextText, 1) != '-');

        int shouldSwap = 0;
        if (currIsShort && !nextIsShort) {
          /* Current is short, next is long - keep order (short before long) */
          shouldSwap = 0;
        } else if (!currIsShort && nextIsShort) {
          /* Current is long, next is short - swap (short before long) */
          shouldSwap = 1;
        } else {
          /* Both same type - sort alphabetically */
          shouldSwap = (obj_strcmp(ops, interp, currText, nextText) > 0);
        }

        if (shouldSwap) {
          newResult = ops->list.push(interp, newResult, next);
          newResult = ops->list.push(interp, newResult, curr);
          i++;
          sorted = 0;
        } else {
          newResult = ops->list.push(interp, newResult, curr);
        }
      } else {
        newResult = ops->list.push(interp, newResult, curr);
      }
    }
    result = newResult;
  }

  return result;
}

/**
 * Complete values from choices defined in an arg or flag entry.
 * Returns list of {text <choice> type value help <...>} dicts.
 */
static FeatherObj complete_choices(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj entry, FeatherObj prefix) {
  FeatherObj result = ops->list.create(interp);

  /* Get choices from entry */
  FeatherObj choices = dict_get_str(ops, interp, entry, K_CHOICES);
  if (ops->string.byte_length(interp, choices) == 0) {
    return result; /* No choices defined */
  }

  /* Get help text to inherit */
  FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);

  /* Parse choices as a list */
  FeatherObj choicesList = feather_list_parse_obj(ops, interp, choices);
  size_t numChoices = ops->list.length(interp, choicesList);

  /* Collect matching choices */
  FeatherObj matches = ops->list.create(interp);
  for (size_t i = 0; i < numChoices; i++) {
    FeatherObj choice = ops->list.at(interp, choicesList, i);
    if (obj_has_prefix(ops, interp, choice, prefix)) {
      matches = ops->list.push(interp, matches, choice);
    }
  }

  /* Sort matches alphabetically */
  size_t numMatches = ops->list.length(interp, matches);
  if (numMatches > 1) {
    /* Bubble sort */
    int sorted = 0;
    while (!sorted) {
      sorted = 1;
      FeatherObj newMatches = ops->list.create(interp);
      for (size_t i = 0; i < numMatches; i++) {
        FeatherObj curr = ops->list.at(interp, matches, i);

        if (i + 1 < numMatches) {
          FeatherObj next = ops->list.at(interp, matches, i + 1);
          if (obj_strcmp(ops, interp, curr, next) > 0) {
            newMatches = ops->list.push(interp, newMatches, next);
            newMatches = ops->list.push(interp, newMatches, curr);
            i++;
            sorted = 0;
          } else {
            newMatches = ops->list.push(interp, newMatches, curr);
          }
        } else {
          newMatches = ops->list.push(interp, newMatches, curr);
        }
      }
      matches = newMatches;
    }
  }

  /* Create completions from sorted matches */
  for (size_t i = 0; i < numMatches; i++) {
    FeatherObj choice = ops->list.at(interp, matches, i);

    /* Convert to C string */
    size_t choiceLen = ops->string.byte_length(interp, choice);
    char choicebuf[256];
    if (choiceLen >= sizeof(choicebuf)) choiceLen = sizeof(choicebuf) - 1;
    for (size_t j = 0; j < choiceLen; j++) {
      choicebuf[j] = (char)ops->string.byte_at(interp, choice, j);
    }
    choicebuf[choiceLen] = '\0';

    FeatherObj completion = make_completion(ops, interp, choicebuf, T_VALUE, help);
    result = ops->list.push(interp, result, completion);
  }

  return result;
}

/**
 * Find a flag entry in a spec by matching against short or long form.
 * Returns the flag entry or nil if not found.
 */
static FeatherObj find_flag_entry(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj spec, FeatherObj flagToken) {
  /* Extract flag name (strip leading dashes) */
  size_t flagLen = ops->string.byte_length(interp, flagToken);
  if (flagLen == 0) return ops->list.create(interp);

  int firstChar = ops->string.byte_at(interp, flagToken, 0);
  if (firstChar != '-') return ops->list.create(interp);

  /* Determine if short (-X) or long (--XXX) */
  int isLong = 0;
  size_t nameStart = 1;
  if (flagLen > 1 && ops->string.byte_at(interp, flagToken, 1) == '-') {
    isLong = 1;
    nameStart = 2;
  }

  /* Extract flag name without dashes */
  FeatherObj flagName = ops->string.slice(interp, flagToken, nameStart, flagLen);

  /* Search spec for matching flag */
  size_t specLen = ops->list.length(interp, spec);
  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, spec, i);
    FeatherObj entryType = dict_get_str(ops, interp, entry, K_TYPE);

    if (!feather_obj_eq_literal(ops, interp, entryType, T_FLAG)) {
      continue;
    }

    /* Check if matches short or long form */
    if (isLong) {
      FeatherObj longFlag = dict_get_str(ops, interp, entry, K_LONG);
      if (obj_strcmp(ops, interp, flagName, longFlag) == 0) {
        return entry;
      }
    } else {
      FeatherObj shortFlag = dict_get_str(ops, interp, entry, K_SHORT);
      if (obj_strcmp(ops, interp, flagName, shortFlag) == 0) {
        return entry;
      }
    }
  }

  return ops->list.create(interp); /* Not found */
}

/**
 * Check if a token looks like a flag (starts with dash).
 */
static int token_is_flag(const FeatherHostOps *ops, FeatherInterp interp,
                         FeatherObj token) {
  size_t len = ops->string.byte_length(interp, token);
  if (len == 0) return 0;
  return ops->string.byte_at(interp, token, 0) == '-';
}

/**
 * Strip <> or ?? brackets from argument name.
 * Converts "<name>" -> "name" and "?name?" -> "name"
 */
static void strip_arg_brackets(char *name, size_t *len) {
  if (*len < 2) return;

  /* Check for <name> format */
  if (name[0] == '<' && name[*len - 1] == '>') {
    /* Shift left and reduce length */
    for (size_t i = 0; i < *len - 2; i++) {
      name[i] = name[i + 1];
    }
    *len -= 2;
    name[*len] = '\0';
    return;
  }

  /* Check for ?name? format */
  if (name[0] == '?' && name[*len - 1] == '?') {
    /* Shift left and reduce length */
    for (size_t i = 0; i < *len - 2; i++) {
      name[i] = name[i + 1];
    }
    *len -= 2;
    name[*len] = '\0';
    return;
  }
}

/**
 * Generate argument placeholders for expected positional arguments.
 * Returns list of {text {} type arg-placeholder name <arg> help <help>} dicts.
 */
static FeatherObj get_arg_placeholders(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj spec, FeatherObj tokens) {
  FeatherObj result = ops->list.create(interp);

  /* Count how many positional arguments have been provided */
  size_t numTokens = ops->list.length(interp, tokens);
  size_t posArgCount = 0;

  /* Skip first token (command name), count non-flag tokens */
  for (size_t i = 1; i < numTokens; i++) {
    FeatherObj token = ops->list.at(interp, tokens, i);

    if (token_is_flag(ops, interp, token)) {
      /* Check if this flag consumes next token */
      FeatherObj flagEntry = find_flag_entry(ops, interp, spec, token);
      if (!ops->list.is_nil(interp, flagEntry)) {
        int hasValue = dict_get_int(ops, interp, flagEntry, K_HAS_VALUE);
        if (hasValue && i + 1 < numTokens) {
          i++; /* Skip the flag's value */
        }
      }
    } else {
      /* Positional argument */
      posArgCount++;
    }
  }

  /* Find arg entries in spec and determine which to show */
  size_t specLen = ops->list.length(interp, spec);
  size_t argIndex = 0;
  int variadicSatisfied = 0;

  for (size_t i = 0; i < specLen; i++) {
    FeatherObj entry = ops->list.at(interp, spec, i);
    FeatherObj entryType = dict_get_str(ops, interp, entry, K_TYPE);

    if (!feather_obj_eq_literal(ops, interp, entryType, T_ARG)) {
      continue;
    }

    /* Check hide flag */
    int hide = dict_get_int(ops, interp, entry, K_HIDE);
    if (hide) {
      continue;
    }

    int required = dict_get_int(ops, interp, entry, K_REQUIRED);
    int variadic = dict_get_int(ops, interp, entry, K_VARIADIC);
    FeatherObj name = dict_get_str(ops, interp, entry, K_NAME);
    FeatherObj help = dict_get_str(ops, interp, entry, K_HELP);

    /* Determine if we should show this arg */
    if (variadic) {
      /* Variadic: only show if not yet satisfied */
      if (argIndex < posArgCount) {
        variadicSatisfied = 1;
      }
      if (!variadicSatisfied) {
        /* Convert name to C string and strip brackets */
        size_t nameLen = ops->string.byte_length(interp, name);
        char namebuf[256];
        if (nameLen >= sizeof(namebuf)) nameLen = sizeof(namebuf) - 1;
        for (size_t j = 0; j < nameLen; j++) {
          namebuf[j] = (char)ops->string.byte_at(interp, name, j);
        }
        namebuf[nameLen] = '\0';
        strip_arg_brackets(namebuf, &nameLen);

        FeatherObj placeholder = make_arg_placeholder(ops, interp, namebuf, help);
        result = ops->list.push(interp, result, placeholder);
      }
      /* Don't increment argIndex for variadic - it consumes all remaining */
    } else {
      /* Regular arg: show if required and not provided, or optional and next */
      if (argIndex == posArgCount) {
        /* This is the next expected arg */
        if (required || argIndex == posArgCount) {
          /* Show placeholder and strip brackets from name */
          size_t nameLen = ops->string.byte_length(interp, name);
          char namebuf[256];
          if (nameLen >= sizeof(namebuf)) nameLen = sizeof(namebuf) - 1;
          for (size_t j = 0; j < nameLen; j++) {
            namebuf[j] = (char)ops->string.byte_at(interp, name, j);
          }
          namebuf[nameLen] = '\0';
          strip_arg_brackets(namebuf, &nameLen);

          FeatherObj placeholder = make_arg_placeholder(ops, interp, namebuf, help);
          result = ops->list.push(interp, result, placeholder);
        }
      }
      argIndex++;
    }
  }

  return result;
}

/**
 * Enhanced completion implementation (v3).
 *
 * Handles command, subcommand, flag, value, and argument placeholder completion.
 * Determines completion context and returns appropriate candidates.
 */
static FeatherObj usage_complete_impl(const FeatherHostOps *ops, FeatherInterp interp,
                                       FeatherObj scriptObj, size_t pos) {
  size_t script_len = ops->string.byte_length(interp, scriptObj);

  /* Clamp position to script length */
  if (pos > script_len) {
    pos = script_len;
  }

  /* Find the start of the current token by scanning backwards */
  size_t token_start = pos;
  while (token_start > 0) {
    int c = ops->string.byte_at(interp, scriptObj, token_start - 1);
    /* Stop at whitespace, newline, semicolon (command separators) */
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';') {
      break;
    }
    token_start--;
  }

  /* Extract the partial token being completed */
  FeatherObj prefix = ops->string.slice(interp, scriptObj, token_start, pos);

  /* Tokenize the script up to the cursor to understand context */
  /* Simple tokenization: split by whitespace, track tokens before cursor */
  FeatherObj tokens = ops->list.create(interp);
  size_t i = 0;
  while (i < token_start) {
    /* Skip whitespace */
    while (i < token_start) {
      int c = ops->string.byte_at(interp, scriptObj, i);
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';') {
        i++;
      } else {
        break;
      }
    }
    if (i >= token_start) break;

    /* Find end of token */
    size_t tok_start = i;
    while (i < token_start) {
      int c = ops->string.byte_at(interp, scriptObj, i);
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';') {
        break;
      }
      i++;
    }

    /* Extract token */
    FeatherObj token = ops->string.slice(interp, scriptObj, tok_start, i);
    tokens = ops->list.push(interp, tokens, token);
  }

  size_t numTokens = ops->list.length(interp, tokens);

  /* Case 1: No tokens yet - complete commands */
  if (numTokens == 0) {
    return complete_commands(ops, interp, prefix);
  }

  /* Get the first token (command name) */
  FeatherObj cmdName = ops->list.at(interp, tokens, 0);

  /* Look up the command's spec */
  FeatherObj specs = usage_get_specs(ops, interp);
  FeatherObj specEntry = ops->dict.get(interp, specs, cmdName);

  /* If command not found, no completions */
  if (ops->list.is_nil(interp, specEntry)) {
    return ops->list.create(interp);
  }

  /* Get the parsed spec (list of entries) from the spec entry dict */
  FeatherObj parsedSpec = dict_get_str(ops, interp, specEntry, K_SPEC);

  /* Case 2: One token (the command) - complete subcommands or flags/args */
  if (numTokens == 1) {
    /* Check if spec has subcommands */
    int hasSubcmds = 0;
    size_t specLen = ops->list.length(interp, parsedSpec);
    for (size_t j = 0; j < specLen; j++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, j);
      FeatherObj entryType = dict_get_str(ops, interp, entry, K_TYPE);
      if (feather_obj_eq_literal(ops, interp, entryType, T_CMD)) {
        hasSubcmds = 1;
        break;
      }
    }

    if (hasSubcmds) {
      /* Complete subcommands */
      return complete_subcommands(ops, interp, parsedSpec, prefix);
    } else {
      /* Complete flags and argument placeholders */
      FeatherObj flags = complete_flags(ops, interp, parsedSpec, prefix);
      FeatherObj placeholders = get_arg_placeholders(ops, interp, parsedSpec, tokens);

      /* Combine flags and placeholders */
      size_t numPlaceholders = ops->list.length(interp, placeholders);
      for (size_t i = 0; i < numPlaceholders; i++) {
        FeatherObj placeholder = ops->list.at(interp, placeholders, i);
        flags = ops->list.push(interp, flags, placeholder);
      }

      return flags;
    }
  }

  /* Case 3: Multiple tokens - check context */
  if (numTokens >= 2) {
    /* First, check if previous token was a flag that expects a value */
    if (numTokens >= 2) {
      FeatherObj prevToken = ops->list.at(interp, tokens, numTokens - 1);

      /* Try to find this flag in the spec */
      FeatherObj flagEntry = find_flag_entry(ops, interp, parsedSpec, prevToken);
      if (!ops->list.is_nil(interp, flagEntry)) {
        /* Check if flag has a value */
        int hasValue = dict_get_int(ops, interp, flagEntry, K_HAS_VALUE);
        if (hasValue) {
          /* Complete from choices if defined */
          FeatherObj result = complete_choices(ops, interp, flagEntry, prefix);
          /* If no choices, return empty (host should handle file/dir completion) */
          return result;
        }
      }
    }

    /* Check if second token is a subcommand */
    FeatherObj secondToken = ops->list.at(interp, tokens, 1);
    FeatherObj activeSpec = parsedSpec;

    /* Look for matching subcommand in spec */
    size_t specLen = ops->list.length(interp, parsedSpec);
    for (size_t j = 0; j < specLen; j++) {
      FeatherObj entry = ops->list.at(interp, parsedSpec, j);
      FeatherObj entryType = dict_get_str(ops, interp, entry, K_TYPE);

      if (feather_obj_eq_literal(ops, interp, entryType, T_CMD)) {
        FeatherObj subcmdName = dict_get_str(ops, interp, entry, K_NAME);
        if (obj_strcmp(ops, interp, secondToken, subcmdName) == 0) {
          /* Found matching subcommand, use its spec */
          FeatherObj subspec = dict_get_str(ops, interp, entry, K_SPEC);
          if (!ops->list.is_nil(interp, subspec)) {
            activeSpec = subspec;

            /* Check if we're completing a flag value in the subcommand */
            if (numTokens >= 3) {
              FeatherObj lastToken = ops->list.at(interp, tokens, numTokens - 1);
              FeatherObj subflagEntry = find_flag_entry(ops, interp, activeSpec, lastToken);
              if (!ops->list.is_nil(interp, subflagEntry)) {
                int hasValue = dict_get_int(ops, interp, subflagEntry, K_HAS_VALUE);
                if (hasValue) {
                  return complete_choices(ops, interp, subflagEntry, prefix);
                }
              }
            }
            break;
          }
        }
      }
    }

    /* Complete flags from active spec */
    return complete_flags(ops, interp, activeSpec, prefix);
  }

  /* Default: return empty list */
  return ops->list.create(interp);
}

/**
 * Main usage command dispatcher
 *
 * Usage:
 *   usage for command ?spec?   - define or get usage spec
 *   usage parse command args   - parse args and set local vars
 *   usage help command         - generate help text
 *   usage complete script pos  - get completion candidates
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

  if (feather_obj_eq_literal(ops, interp, subcmd, "complete")) {
    /* usage complete script pos */
    size_t complete_argc = ops->list.length(interp, args);
    if (complete_argc != 2) {
      ops->interp.set_result(interp,
          ops->string.intern(interp, S("wrong # args: should be \"usage complete script pos\"")));
      return TCL_ERROR;
    }

    /* Get script string */
    FeatherObj scriptObj = ops->list.at(interp, args, 0);

    /* Get position */
    FeatherObj posObj = ops->list.at(interp, args, 1);
    int64_t pos_i = 0;
    if (ops->integer.get(interp, posObj, &pos_i) == TCL_ERROR || pos_i < 0) {
      ops->interp.set_result(interp,
          ops->string.intern(interp, S("usage complete: pos must be a non-negative integer")));
      return TCL_ERROR;
    }
    size_t pos = (size_t)pos_i;

    /* Perform completion */
    FeatherObj result = usage_complete_impl(ops, interp, scriptObj, pos);
    ops->interp.set_result(interp, result);
    return TCL_OK;
  }

  /* Unknown subcommand */
  FeatherObj msg = ops->string.intern(interp, "unknown subcommand \"", 20);
  msg = ops->string.concat(interp, msg, subcmd);
  msg = ops->string.concat(interp, msg, ops->string.intern(interp, S("\": must be complete, for, help, or parse")));
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
 * Create a custom section entry.
 * name is the section header (e.g., "STRING INDICES")
 * content is the section body text
 */
FeatherObj feather_usage_section(const FeatherHostOps *ops, FeatherInterp interp,
                                 const char *name, const char *content) {
  FeatherObj entry = ops->dict.create(interp);
  entry = dict_set_str(ops, interp, entry, K_TYPE, ops->string.intern(interp, S(T_SECTION)));
  entry = dict_set_str(ops, interp, entry, K_SECTION_NAME,
                       ops->string.intern(interp, name, feather_strlen(name)));
  entry = dict_set_str(ops, interp, entry, K_CONTENT,
                       ops->string.intern(interp, content, feather_strlen(content)));
  return entry;
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
 * Mark a subcommand entry as a clause (syntax element that appears after other arguments).
 * Clause subcommands appear in the COMMANDS section but do not trigger <COMMAND> in synopsis.
 * Use this for commands like "try" where handlers (on/trap/finally) appear after the body,
 * not as the first argument.
 */
FeatherObj feather_usage_clause(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj entry) {
  return usage_set_clause(ops, interp, entry);
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
 * Create a meta entry with command description.
 * about: Short description for NAME section (e.g., "Read and write variables")
 * description: Detailed description for DESCRIPTION section
 */
FeatherObj feather_usage_about(const FeatherHostOps *ops, FeatherInterp interp,
                               const char *about, const char *description) {
  FeatherObj entry = ops->dict.create(interp);
  entry = dict_set_str(ops, interp, entry, K_TYPE, ops->string.intern(interp, S(T_META)));
  if (about) {
    entry = dict_set_str(ops, interp, entry, K_ABOUT,
                         ops->string.intern(interp, about, feather_strlen(about)));
  }
  if (description) {
    entry = dict_set_str(ops, interp, entry, K_LONG_HELP,
                         ops->string.intern(interp, description, feather_strlen(description)));
  }
  return entry;
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

/**
 * Register usage help for the 'usage' command itself.
 */
void feather_register_usage_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Define and query command-line argument specifications",
    "The usage command provides a declarative way to specify command-line "
    "arguments, flags, and subcommands for procedures. It supports automatic "
    "parsing of argument lists into local variables, validation of required "
    "arguments and flag values, and generation of help text.\n\n"
    "Usage specs are defined using a TCL-native block syntax with entry types "
    "for arguments (arg), flags (flag), subcommands (cmd), and examples "
    "(example). Each entry can have additional options like help text, default "
    "values, and valid choices.\n\n"
    "Note: This is a Feather-specific command and is not part of standard TCL.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: for ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<command>");
  e = feather_usage_help(ops, interp, e, "The command name to define or query");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?spec?");
  e = feather_usage_help(ops, interp, e, "The usage specification (if defining)");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "for", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Defines or retrieves a usage specification for a command. When called with "
    "both command and spec arguments, stores the specification for later use "
    "with parse and help subcommands. When called with only the command name, "
    "returns the stored specification in a format that can be passed back to "
    "usage for (round-trippable).\n\n"
    "The spec uses a TCL-native block syntax with these entry types:\n\n"
    "arg <name>              Required positional argument\n\n"
    "arg ?name?              Optional positional argument\n\n"
    "arg <name>...           Variadic required (1 or more)\n\n"
    "arg ?name?...           Variadic optional (0 or more)\n\n"
    "flag -s --long          Boolean flag (short and/or long form)\n\n"
    "flag -f --file <path>   Flag with required value\n\n"
    "cmd name {...}          Subcommand with nested spec\n\n"
    "example {code}          Usage example");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: parse ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<command>");
  e = feather_usage_help(ops, interp, e, "The command whose spec to use for parsing");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<args>");
  e = feather_usage_help(ops, interp, e, "The argument list to parse");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "parse", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Parses an argument list according to a previously defined usage "
    "specification and creates local variables in the caller's scope for each "
    "argument and flag.\n\n"
    "Flags can appear anywhere in the argument list and are parsed first. The "
    "special \"--\" separator stops flag parsing, treating all subsequent "
    "arguments as positional. Boolean flags are set to 1 when present, 0 when "
    "absent. Flags that take values store the provided value.\n\n"
    "Positional arguments are matched in order after flag processing. Variadic "
    "arguments collect all remaining positional values into a list.\n\n"
    "A special variable $subcommand is set to a list containing the path of "
    "matched subcommands (e.g., {remote add} for nested commands).\n\n"
    "Returns an error if required arguments are missing, unknown flags are "
    "provided, or values fail validation (such as choices or type constraints).");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: help ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<command>");
  e = feather_usage_help(ops, interp, e, "The command to generate help for");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?subcommand?...");
  e = feather_usage_help(ops, interp, e, "Optional subcommand path for specific help");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "help", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Generates help text for a command based on its usage specification. The "
    "output follows the standard Unix manpage format with sections for NAME, "
    "SYNOPSIS, DESCRIPTION, OPTIONS, ARGUMENTS, COMMANDS, and EXAMPLES.\n\n"
    "If optional subcommand arguments are provided, generates help specific to "
    "that subcommand path. For example, \"usage help git remote\" would show "
    "help for the \"remote\" subcommand of \"git\".\n\n"
    "Help text is automatically word-wrapped and formatted for terminal "
    "display. Multi-line text in specifications is dedented and trimmed for "
    "consistent output.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Custom section: Spec Options ---
  e = feather_usage_section(ops, interp, "Spec Options",
    "Each entry in a spec can have an options block with additional "
    "configuration:\n\n"
    "help {text}         Short help text displayed in usage output\n\n"
    "long_help {text}    Extended help for detailed documentation\n\n"
    "default {value}     Default value when argument is omitted (arg only)\n\n"
    "choices {a b c}     Space-separated list of valid values\n\n"
    "type script         Validates value is syntactically complete TCL\n\n"
    "hide                Hide from help output");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Examples ---
  e = feather_usage_example(ops, interp,
    "usage for mycommand {\n"
    "    arg <input>\n"
    "    arg ?output?\n"
    "    flag -v --verbose\n"
    "}",
    "Define a simple command spec",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc mycommand {args} {\n"
    "    usage parse mycommand $args\n"
    "    puts \"Input: $input\"\n"
    "}",
    "Parse arguments in a procedure",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "puts [usage help mycommand]",
    "Display help for a command",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // --- SEE ALSO ---
  e = feather_usage_section(ops, interp, "See Also",
    "proc(1)");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "usage", spec);
}
