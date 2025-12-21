#include "tclc.h"

static int is_whitespace(char c) {
  return c == ' ' || c == '\t';
}

static int is_command_terminator(char c) {
  return c == '\n' || c == '\r' || c == '\0';
}

static int is_word_char(char c) {
  return !is_whitespace(c) && !is_command_terminator(c);
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

    // Check for command terminator (newline) - stop parsing this command
    if (is_command_terminator(*pos)) {
      break;
    }

    // Check for braced string
    if (*pos == '{') {
      // Find matching close brace, counting nesting depth
      int depth = 1;
      const char *brace_start = pos;
      const char *content_start = pos + 1;
      pos++;
      while (pos < end && depth > 0) {
        if (*pos == '{') {
          depth++;
        } else if (*pos == '}') {
          depth--;
        }
        pos++;
      }

      // Check if we reached end of input with unclosed braces
      if (depth > 0) {
        // Build result: {INCOMPLETE start_offset end_offset}
        TclObj result = ops->list.create(interp);
        TclObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        TclObj start_pos = ops->integer.create(interp, (int64_t)(brace_start - script));
        TclObj end_pos = ops->integer.create(interp, (int64_t)len);
        result = ops->list.push(interp, result, incomplete);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        ops->interp.set_result(interp, result);
        return TCL_PARSE_INCOMPLETE;
      }

      // pos now points past the closing brace
      // Content is between content_start and pos-1 (excluding the final })
      size_t content_len = (pos - 1) - content_start;
      TclObj word = ops->string.intern(interp, content_start, content_len);
      words = ops->list.push(interp, words, word);
    } else {
      // Find end of current word
      const char *word_start = pos;
      while (pos < end && is_word_char(*pos)) {
        pos++;
      }

      // Create token for this word
      size_t word_len = pos - word_start;
      if (word_len > 0) {
        TclObj word = ops->string.intern(interp, word_start, word_len);
        words = ops->list.push(interp, words, word);
      }
    }
  }

  ops->interp.set_result(interp, words);
  return TCL_PARSE_OK;
}
