#include "tclc.h"

TclResult tcl_eval_obj(const TclHostOps *ops, TclInterp interp, TclObj script,
                       TclEvalFlags flags) {
  // script is a list of tokens from the parser
  // First token is the command name, rest are arguments

  // Check if the list is empty
  if (ops->list.length(interp, script) == 0) {
    return TCL_OK;
  }

  // Extract the command name (first element)
  TclObj cmd = ops->list.shift(interp, script);
  if (ops->list.is_nil(interp, cmd)) {
    return TCL_OK;
  }

  // The remaining list is the arguments
  TclObj args = script;

  // Try to invoke via bind.unknown (host command lookup)
  TclObj result;
  TclResult code = ops->bind.unknown(interp, cmd, args, &result);

  if (code == TCL_OK) {
    ops->interp.set_result(interp, result);
  }

  return code;
}

static int is_space(char c) {
  return c == ' ' || c == '\t';
}

TclResult tcl_eval_string(const TclHostOps *ops, TclInterp interp,
                          const char *script, size_t len, TclEvalFlags flags) {
  TclResult result = TCL_OK;
  const char *start = script;
  const char *end = script + len;

  while (start < end) {
    // Skip leading whitespace
    while (start < end && is_space(*start)) {
      start++;
    }

    // Check for comment - if line starts with #, skip to end of line
    if (start < end && *start == '#') {
      while (start < end && *start != '\n') {
        start++;
      }
      if (start < end && *start == '\n') {
        start++;
      }
      continue;
    }

    // Find the end of the current command (newline or end of input)
    // Must account for braces and quotes - newlines inside them don't end commands
    // Also handle backslash-newline continuation
    const char *cmd_end = start;
    int brace_depth = 0;
    int in_quotes = 0;
    while (cmd_end < end) {
      if (*cmd_end == '\\' && cmd_end + 1 < end && cmd_end[1] == '\n') {
        // Backslash-newline: skip both and continue (not a command separator)
        cmd_end += 2;
        // Skip any following whitespace (spaces/tabs)
        while (cmd_end < end && is_space(*cmd_end)) {
          cmd_end++;
        }
        continue;
      }
      if (!in_quotes && *cmd_end == '{') {
        brace_depth++;
      } else if (!in_quotes && *cmd_end == '}') {
        brace_depth--;
      } else if (brace_depth == 0 && *cmd_end == '"') {
        in_quotes = !in_quotes;
      } else if ((*cmd_end == '\n' || *cmd_end == ';') && brace_depth == 0 && !in_quotes) {
        break;
      }
      cmd_end++;
    }

    size_t cmd_len = cmd_end - start;

    // Parse and evaluate this command
    TclParseStatus status = tcl_parse(ops, interp, start, cmd_len);
    if (status != TCL_PARSE_OK) {
      return TCL_ERROR;
    }

    // The parser stores its result (a list of tokens) in the interpreter's result slot
    TclObj parsed = ops->interp.get_result(interp);

    // Only evaluate non-empty commands (list with at least one token)
    if (ops->list.length(interp, parsed) > 0) {
      result = tcl_eval_obj(ops, interp, parsed, flags);
      if (result != TCL_OK) {
        return result;
      }
    }

    // Move past the command separator (newline or semicolon)
    start = cmd_end;
    if (start < end && (*start == '\n' || *start == ';')) {
      start++;
    }
  }

  return result;
}
