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
    } else if (feather_obj_eq_literal(ops, interp, arg, "--")) {
      idx++;
      break;
    } else {
      // Unknown option - build error with original object
      FeatherObj msg = ops->string.intern(interp,
          "bad option \"", 12);
      FeatherObj part3 = ops->string.intern(interp,
          "\": must be -exact, -glob, -regexp, or --", 40);
      msg = ops->string.concat(interp, msg, arg);
      msg = ops->string.concat(interp, msg, part3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
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
      } else {
        switch (mode) {
          case SWITCH_EXACT:
            matched = ops->string.equal(interp, matchString, pattern);
            break;
          case SWITCH_GLOB:
            // Use feather_obj_glob_match which handles character classes [...]
            matched = feather_obj_glob_match(ops, interp, pattern, matchString);
            break;
          case SWITCH_REGEXP: {
            int result;
            FeatherResult rc = ops->string.regex_match(interp, pattern, matchString, &result);
            if (rc != TCL_OK) {
              return rc;
            }
            matched = result;
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
    // No match - return empty string
    ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
    return TCL_OK;
  }

  // Execute the matched body
  return feather_script_eval_obj(ops, interp, bodyToExecute, TCL_EVAL_LOCAL);
}
