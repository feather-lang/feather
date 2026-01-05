#include "feather.h"
#include "internal.h"

// Match modes
typedef enum {
  SWITCH_EXACT = 0,
  SWITCH_GLOB = 1,
  SWITCH_REGEXP = 2
} SwitchMode;

// Helper to check string equality
static int str_eq(const char *a, size_t alen, const char *b, size_t blen) {
  if (alen != blen) return 0;
  for (size_t i = 0; i < alen; i++) {
    if (a[i] != b[i]) return 0;
  }
  return 1;
}

FeatherResult feather_builtin_switch(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"switch ?options? string pattern body ... ?default body?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  SwitchMode mode = SWITCH_EXACT;
  int nocase = 0;
  FeatherObj matchvarName = 0;
  FeatherObj indexvarName = 0;
  size_t idx = 0;

  // Parse options
  while (idx < argc) {
    FeatherObj arg = ops->list.at(interp, args, idx);

    // Check if this is an option (starts with '-')
    if (ops->string.byte_at(interp, arg, 0) != '-') break;

    if (feather_obj_eq_literal(ops, interp, arg, "-exact")) {
      mode = SWITCH_EXACT;
      idx++;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-glob")) {
      mode = SWITCH_GLOB;
      idx++;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-regexp")) {
      mode = SWITCH_REGEXP;
      idx++;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-nocase")) {
      nocase = 1;
      idx++;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-matchvar")) {
      idx++;
      if (idx >= argc) {
        FeatherObj msg = ops->string.intern(interp,
            "missing variable name argument to -matchvar option", 50);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      matchvarName = ops->list.at(interp, args, idx);
      idx++;
    } else if (feather_obj_eq_literal(ops, interp, arg, "-indexvar")) {
      idx++;
      if (idx >= argc) {
        FeatherObj msg = ops->string.intern(interp,
            "missing variable name argument to -indexvar option", 50);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }
      indexvarName = ops->list.at(interp, args, idx);
      idx++;
    } else if (feather_obj_eq_literal(ops, interp, arg, "--")) {
      idx++;
      break;
    } else {
      // Unknown option - build error with original object
      FeatherObj msg = ops->string.intern(interp,
          "bad option \"", 12);
      FeatherObj part3 = ops->string.intern(interp,
          "\": must be -exact, -glob, -indexvar, -matchvar, -nocase, -regexp, or --", 71);
      msg = ops->string.concat(interp, msg, arg);
      msg = ops->string.concat(interp, msg, part3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // -matchvar and -indexvar require -regexp
  if ((matchvarName != 0 || indexvarName != 0) && mode != SWITCH_REGEXP) {
    FeatherObj msg = ops->string.intern(interp,
        matchvarName != 0 ? "-matchvar option requires -regexp option" : "-indexvar option requires -regexp option",
        matchvarName != 0 ? 40 : 40);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Need at least string and one pattern-body pair
  if (idx >= argc) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"switch ?options? string pattern body ... ?default body?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj matchString = ops->list.at(interp, args, idx);
  idx++;

  // Get remaining args (patterns and bodies)
  size_t remaining = argc - idx;

  // Check if remaining is a single braced list (list form)
  FeatherObj patternBodyList;
  size_t numItems;

  if (remaining == 1) {
    // Single argument - must be a list of pattern-body pairs
    FeatherObj listArg = ops->list.at(interp, args, idx);
    patternBodyList = ops->list.from(interp, listArg);
    numItems = ops->list.length(interp, patternBodyList);
  } else {
    // Multiple arguments - inline pattern body pairs
    patternBodyList = ops->list.create(interp);
    for (size_t i = idx; i < argc; i++) {
      patternBodyList = ops->list.push(interp, patternBodyList,
                                        ops->list.at(interp, args, i));
    }
    numItems = remaining;
  }

  // Must have at least 2 items and even count (pattern-body pairs)
  if (numItems < 2 || (numItems % 2) != 0) {
    // Check for trailing pattern without body
    if (numItems > 0 && (numItems % 2) != 0) {
      FeatherObj msg = ops->string.intern(interp,
          "extra switch pattern with no body", 33);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"switch ?options? string pattern body ... ?default body?\"", 81);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Check that 'default' is only used as the last pattern
  for (size_t i = 0; i < numItems - 2; i += 2) {
    FeatherObj pattern = ops->list.at(interp, patternBodyList, i);
    if (feather_obj_eq_literal(ops, interp, pattern, "default")) {
      FeatherObj msg = ops->string.intern(interp,
          "default pattern must be last", 28);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Find matching pattern
  FeatherObj bodyToExecute = 0;
  FeatherObj capturedMatches = 0;
  FeatherObj capturedIndices = 0;
  int inFallthrough = 0;

  for (size_t i = 0; i < numItems; i += 2) {
    FeatherObj pattern = ops->list.at(interp, patternBodyList, i);
    FeatherObj body = ops->list.at(interp, patternBodyList, i + 1);

    // Check for fall-through (body is just "-")
    int isFallthrough = feather_obj_eq_literal(ops, interp, body, "-");

    // Check if last pattern has fall-through (error)
    if (isFallthrough && i + 2 >= numItems) {
      FeatherObj msg = ops->string.intern(interp,
          "extra switch pattern with no body", 33);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // If we're in fallthrough mode, skip pattern matching
    int matched = inFallthrough;

    if (!matched) {
      // Check for default pattern
      int isDefault = feather_obj_eq_literal(ops, interp, pattern, "default");

      if (isDefault) {
        matched = 1;
        // For default, set empty captures
        capturedMatches = ops->list.create(interp);
        capturedIndices = ops->list.create(interp);
      } else {
        switch (mode) {
          case SWITCH_EXACT:
            if (nocase) {
              FeatherObj foldedMatch = ops->rune.fold(interp, matchString);
              FeatherObj foldedPattern = ops->rune.fold(interp, pattern);
              matched = ops->string.equal(interp, foldedMatch, foldedPattern);
            } else {
              matched = ops->string.equal(interp, matchString, pattern);
            }
            break;
          case SWITCH_GLOB:
            if (nocase) {
              FeatherObj foldedMatch = ops->rune.fold(interp, matchString);
              FeatherObj foldedPattern = ops->rune.fold(interp, pattern);
              matched = feather_obj_glob_match(ops, interp, foldedPattern, foldedMatch);
            } else {
              matched = feather_obj_glob_match(ops, interp, pattern, matchString);
            }
            break;
          case SWITCH_REGEXP: {
            int result;
            FeatherObj matches = 0;
            FeatherObj indices = 0;
            FeatherObj *matchesPtr = (matchvarName != 0) ? &matches : NULL;
            FeatherObj *indicesPtr = (indexvarName != 0) ? &indices : NULL;
            FeatherResult rc = ops->string.regex_match(interp, pattern, matchString, nocase, &result, matchesPtr, indicesPtr);
            if (rc != TCL_OK) {
              return rc;
            }
            matched = result;
            if (matched) {
              capturedMatches = matches;
              capturedIndices = indices;
            }
            break;
          }
        }
      }
    }

    if (matched) {
      if (isFallthrough) {
        // Set flag to skip pattern matching on next iteration
        inFallthrough = 1;
        continue;
      }
      bodyToExecute = body;
      break;
    }
  }

  if (bodyToExecute == 0) {
    // No match - set empty variables if requested and return empty string
    if (matchvarName != 0) {
      feather_set_var(ops, interp, matchvarName, ops->list.create(interp));
    }
    if (indexvarName != 0) {
      feather_set_var(ops, interp, indexvarName, ops->list.create(interp));
    }
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Set capture variables before executing body
  if (matchvarName != 0 && capturedMatches != 0) {
    feather_set_var(ops, interp, matchvarName, capturedMatches);
  } else if (matchvarName != 0) {
    // No captures (e.g., default branch)
    feather_set_var(ops, interp, matchvarName, ops->list.create(interp));
  }
  if (indexvarName != 0 && capturedIndices != 0) {
    feather_set_var(ops, interp, indexvarName, capturedIndices);
  } else if (indexvarName != 0) {
    // No captures (e.g., default branch)
    feather_set_var(ops, interp, indexvarName, ops->list.create(interp));
  }

  // Execute the matched body
  return feather_script_eval_obj(ops, interp, bodyToExecute, TCL_EVAL_LOCAL);
}

void feather_register_switch_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Evaluate one of several scripts, depending on a given value",
    "The switch command matches its string argument against each of the pattern arguments in "
    "order. As soon as it finds a pattern that matches string, it evaluates the following body "
    "argument and returns the result of that evaluation. If the last pattern is the keyword "
    "\"default\", it matches anything. If no pattern matches and no default is given, switch "
    "returns an empty string.\n\n"
    "Two syntaxes are provided for the pattern and body arguments. The first uses a separate "
    "argument for each of the patterns and commands; this form is convenient if substitutions "
    "are desired on some of the patterns or commands. The second form places all of the patterns "
    "and commands together into a single argument; the argument must have proper list structure, "
    "with the elements of the list being the patterns and commands. The second form makes it easy "
    "to construct multi-line switch commands, since the braces around the whole list make it "
    "unnecessary to include a backslash at the end of each line.\n\n"
    "If a body is specified as \"-\" it means that the body for the next pattern should also be "
    "used as the body for this pattern (if the next pattern also has a body of \"-\" then the body "
    "after that is used, and so on). This feature makes it possible to share a single body among "
    "several patterns.\n\n"
    "Beware of how you place comments in switch commands. Comments should only be placed inside "
    "the execution body of one of the patterns, and not intermingled with the patterns.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Flags (options)
  e = feather_usage_flag(ops, interp, "-exact", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Use exact matching when comparing string to a pattern. This is the default.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-glob", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "When matching string to the patterns, use glob-style matching (i.e. the same as implemented by the string match command).");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-regexp", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "When matching string to the patterns, use regular expression matching.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-nocase", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Causes comparisons to be handled in a case-insensitive manner.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-matchvar", NULL, "<varName>");
  e = feather_usage_help(ops, interp, e, "This option (only legal when -regexp is also specified) specifies the name of a variable into which the list of matches found by the regular expression engine will be written. The first element of the list written will be the overall substring of the input string matched, the second element of the list will be the substring matched by the first capturing parenthesis in the regular expression that matched, and so on. When a default branch is taken, the variable will have the empty list written to it.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "-indexvar", NULL, "<varName>");
  e = feather_usage_help(ops, interp, e, "This option (only legal when -regexp is also specified) specifies the name of a variable into which the list of indices referring to matching substrings found by the regular expression engine will be written. The first element of the list written will be a two-element list specifying the index of the start and index of the first character after the end of the overall substring of the input string matched. Similarly, the second element of the list refers to the first capturing parenthesis in the regular expression that matched, and so on. When a default branch is taken, the variable will have the empty list written to it.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_flag(ops, interp, "--", NULL, NULL);
  e = feather_usage_help(ops, interp, e, "Marks the end of options. The argument following this one will be treated as string even if it starts with a -.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Positional arguments
  e = feather_usage_arg(ops, interp, "<string>");
  e = feather_usage_help(ops, interp, e, "The value to match against patterns");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<pattern>");
  e = feather_usage_help(ops, interp, e, "Pattern to match (or \"default\" to match anything)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to evaluate if pattern matches (or \"-\" for fall-through)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "?pattern body ...?");
  e = feather_usage_help(ops, interp, e, "Additional pattern-body pairs");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "switch -exact $x {\n"
    "    a { puts \"Found a\" }\n"
    "    b { puts \"Found b\" }\n"
    "    default { puts \"Something else\" }\n"
    "}",
    "Exact string matching with list form syntax:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "switch -glob $filename {\n"
    "    *.txt { puts \"Text file\" }\n"
    "    *.c { puts \"C source\" }\n"
    "    default { puts \"Unknown type\" }\n"
    "}",
    "Glob pattern matching:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "switch -regexp -matchvar matches $input {\n"
    "    {^([0-9]+)$} { puts \"Number: [lindex $matches 1]\" }\n"
    "    {^([a-z]+)$} { puts \"Word: [lindex $matches 1]\" }\n"
    "    default { puts \"Other\" }\n"
    "}",
    "Regular expression matching with capture groups:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "switch $x \\\n"
    "    a { puts \"Found a\" } \\\n"
    "    b { puts \"Found b\" } \\\n"
    "    c - \\\n"
    "    d { puts \"Found c or d\" }",
    "Fall-through using inline form syntax:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "for, if, regexp, string match");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "switch", spec);
}
