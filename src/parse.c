#include "tclc.h"

static int is_whitespace(char c) {
  return c == ' ' || c == '\t';
}

static int is_command_terminator(char c) {
  return c == '\n' || c == '\r' || c == '\0';
}

TclParseStatus tcl_parse(const TclHostOps *ops, TclInterp interp,
                         const char *script, size_t len) {
  // Skip leading whitespace (but not newlines - those are command separators)
  while (len > 0 && is_whitespace(*script)) {
    script++;
    len--;
  }

  // Skip trailing whitespace
  while (len > 0 && is_whitespace(script[len - 1])) {
    len--;
  }

  if (len == 0) {
    // Empty command - return empty list
    TclObj list = ops->list.create(interp);
    ops->interp.set_result(interp, list);
    return TCL_PARSE_OK;
  }

  // Create a list to hold the tokens (words)
  TclObj words = ops->list.create(interp);

  const char *pos = script;
  const char *end = script + len;

  while (pos < end) {
    // Skip whitespace between words
    while (pos < end && is_whitespace(*pos)) {
      pos++;
    }

    if (pos >= end) {
      break;
    }

    // Find end of current word
    const char *word_start = pos;
    while (pos < end && !is_whitespace(*pos) && !is_command_terminator(*pos)) {
      pos++;
    }

    // Create token for this word
    size_t word_len = pos - word_start;
    if (word_len > 0) {
      TclObj word = ops->string.intern(interp, word_start, word_len);
      words = ops->list.push(interp, words, word);
    }
  }

  ops->interp.set_result(interp, words);
  return TCL_PARSE_OK;
}
