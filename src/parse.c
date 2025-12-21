#include "tclc.h"

static int is_whitespace(char c) {
  return c == ' ' || c == '\t';
}

static int is_command_terminator(char c) {
  return c == '\n' || c == '\r' || c == '\0' || c == ';';
}

static int is_word_terminator(char c) {
  return is_whitespace(c) || is_command_terminator(c);
}

static int is_octal_digit(char c) {
  return c >= '0' && c <= '7';
}

static int is_hex_digit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  return 0;
}

/**
 * Process a backslash escape sequence starting at pos.
 * Returns the number of characters consumed from input.
 * Writes the resulting character(s) to out_buf (must have space for 4 bytes for UTF-8).
 * Returns the number of bytes written to out_buf via out_len.
 */
static size_t process_backslash(const char *pos, const char *end,
                                char *out_buf, size_t *out_len) {
  if (pos >= end) {
    *out_len = 0;
    return 0;
  }

  // pos points at the character after backslash
  char c = *pos;

  switch (c) {
    case 'a': *out_buf = '\a'; *out_len = 1; return 1;
    case 'b': *out_buf = '\b'; *out_len = 1; return 1;
    case 'f': *out_buf = '\f'; *out_len = 1; return 1;
    case 'n': *out_buf = '\n'; *out_len = 1; return 1;
    case 'r': *out_buf = '\r'; *out_len = 1; return 1;
    case 't': *out_buf = '\t'; *out_len = 1; return 1;
    case 'v': *out_buf = '\v'; *out_len = 1; return 1;
    case '\\': *out_buf = '\\'; *out_len = 1; return 1;
    case '\n': {
      // Backslash-newline: consume newline and following whitespace, produce space
      size_t consumed = 1;
      const char *p = pos + 1;
      while (p < end && is_whitespace(*p)) {
        p++;
        consumed++;
      }
      *out_buf = ' ';
      *out_len = 1;
      return consumed;
    }
    case 'x': {
      // Hex escape: \xhh (1-2 hex digits)
      size_t consumed = 1;
      int value = 0;
      const char *p = pos + 1;
      int digits = 0;
      while (p < end && is_hex_digit(*p) && digits < 2) {
        value = value * 16 + hex_value(*p);
        p++;
        consumed++;
        digits++;
      }
      if (digits > 0) {
        *out_buf = (char)value;
        *out_len = 1;
        return consumed;
      }
      // No hex digits - just return 'x'
      *out_buf = 'x';
      *out_len = 1;
      return 1;
    }
    case 'u': {
      // Unicode escape: \uhhhh (1-4 hex digits)
      size_t consumed = 1;
      int value = 0;
      const char *p = pos + 1;
      int digits = 0;
      while (p < end && is_hex_digit(*p) && digits < 4) {
        value = value * 16 + hex_value(*p);
        p++;
        consumed++;
        digits++;
      }
      if (digits > 0) {
        // Encode as UTF-8
        if (value < 0x80) {
          *out_buf = (char)value;
          *out_len = 1;
        } else if (value < 0x800) {
          out_buf[0] = (char)(0xC0 | (value >> 6));
          out_buf[1] = (char)(0x80 | (value & 0x3F));
          *out_len = 2;
        } else {
          out_buf[0] = (char)(0xE0 | (value >> 12));
          out_buf[1] = (char)(0x80 | ((value >> 6) & 0x3F));
          out_buf[2] = (char)(0x80 | (value & 0x3F));
          *out_len = 3;
        }
        return consumed;
      }
      *out_buf = 'u';
      *out_len = 1;
      return 1;
    }
    case 'U': {
      // Unicode escape: \Uhhhhhhhh (1-8 hex digits, max 0x10FFFF)
      size_t consumed = 1;
      unsigned int value = 0;
      const char *p = pos + 1;
      int digits = 0;
      while (p < end && is_hex_digit(*p) && digits < 8) {
        unsigned int new_val = value * 16 + hex_value(*p);
        if (new_val > 0x10FFFF) break;
        value = new_val;
        p++;
        consumed++;
        digits++;
      }
      if (digits > 0) {
        // Encode as UTF-8
        if (value < 0x80) {
          *out_buf = (char)value;
          *out_len = 1;
        } else if (value < 0x800) {
          out_buf[0] = (char)(0xC0 | (value >> 6));
          out_buf[1] = (char)(0x80 | (value & 0x3F));
          *out_len = 2;
        } else if (value < 0x10000) {
          out_buf[0] = (char)(0xE0 | (value >> 12));
          out_buf[1] = (char)(0x80 | ((value >> 6) & 0x3F));
          out_buf[2] = (char)(0x80 | (value & 0x3F));
          *out_len = 3;
        } else {
          out_buf[0] = (char)(0xF0 | (value >> 18));
          out_buf[1] = (char)(0x80 | ((value >> 12) & 0x3F));
          out_buf[2] = (char)(0x80 | ((value >> 6) & 0x3F));
          out_buf[3] = (char)(0x80 | (value & 0x3F));
          *out_len = 4;
        }
        return consumed;
      }
      *out_buf = 'U';
      *out_len = 1;
      return 1;
    }
    default:
      if (is_octal_digit(c)) {
        // Octal escape: \ooo (1-3 octal digits, max 0377)
        int value = c - '0';
        size_t consumed = 1;
        const char *p = pos + 1;
        int digits = 1;
        while (p < end && is_octal_digit(*p) && digits < 3) {
          int new_val = value * 8 + (*p - '0');
          if (new_val > 0377) break;
          value = new_val;
          p++;
          consumed++;
          digits++;
        }
        *out_buf = (char)value;
        *out_len = 1;
        return consumed;
      }
      // Unknown escape - just return the character literally
      *out_buf = c;
      *out_len = 1;
      return 1;
  }
}

/**
 * Append a string segment to the current word being built.
 * If word is nil, creates a new string. Otherwise concatenates.
 */
static TclObj append_to_word(const TclHostOps *ops, TclInterp interp,
                             TclObj word, const char *s, size_t len) {
  TclObj segment = ops->string.intern(interp, s, len);
  if (ops->list.is_nil(interp, word)) {
    return segment;
  }
  return ops->string.concat(interp, word, segment);
}

void tcl_parse_init(TclParseContext *ctx, const char *script, size_t len) {
  ctx->script = script;
  ctx->len = len;
  ctx->pos = 0;
}

/**
 * Skip whitespace, backslash-newline continuations, and comments.
 * Returns the new position in the script.
 */
static size_t skip_whitespace_and_comments(const char *script, size_t len, size_t pos) {
  const char *s = script + pos;
  const char *end = script + len;

  while (s < end) {
    // Skip whitespace
    if (is_whitespace(*s)) {
      s++;
      continue;
    }

    // Skip backslash-newline continuation
    if (*s == '\\' && s + 1 < end && s[1] == '\n') {
      s += 2;
      while (s < end && is_whitespace(*s)) {
        s++;
      }
      continue;
    }

    // Skip comments (# at start of command)
    if (*s == '#') {
      while (s < end && *s != '\n') {
        s++;
      }
      if (s < end && *s == '\n') {
        s++;
      }
      continue;
    }

    break;
  }

  return s - script;
}

/**
 * Parse a single word starting at the given position.
 * Updates pos to point past the parsed word.
 * Returns the parsed word, or sets error in interp and returns 0.
 */
static TclObj parse_word(const TclHostOps *ops, TclInterp interp,
                         const char *script, size_t len, size_t *pos,
                         TclParseStatus *status) {
  const char *p = script + *pos;
  const char *end = script + len;
  TclObj word = 0;
  const char *word_start = p;

  while (p < end && !is_word_terminator(*p)) {
    if (*p == '{') {
      // Braced string - no substitutions, content is literal
      int depth = 1;
      const char *brace_start = p;
      const char *content_start = p + 1;
      p++;
      while (p < end && depth > 0) {
        if (*p == '\\' && p + 1 < end) {
          // Backslash in braces: skip the next char to avoid miscounting braces
          p++;
          if (p < end) p++;
          continue;
        }
        if (*p == '{') {
          depth++;
        } else if (*p == '}') {
          depth--;
        }
        p++;
      }

      if (depth > 0) {
        // Unclosed braces
        TclObj result = ops->list.create(interp);
        TclObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        TclObj start_pos = ops->integer.create(interp, (int64_t)(brace_start - script));
        TclObj end_pos = ops->integer.create(interp, (int64_t)len);
        result = ops->list.push(interp, result, incomplete);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        ops->interp.set_result(interp, result);
        *status = TCL_PARSE_INCOMPLETE;
        return 0;
      }

      // Check for extra characters after close brace
      if (p < end && !is_word_terminator(*p)) {
        TclObj result = ops->list.create(interp);
        TclObj error_tag = ops->string.intern(interp, "ERROR", 5);
        TclObj start_pos = ops->integer.create(interp, (int64_t)(brace_start - script));
        TclObj end_pos = ops->integer.create(interp, (int64_t)len);
        TclObj msg = ops->string.intern(interp, "extra characters after close-brace", 34);
        result = ops->list.push(interp, result, error_tag);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        result = ops->list.push(interp, result, msg);
        ops->interp.set_result(interp, result);
        *status = TCL_PARSE_ERROR;
        return 0;
      }

      // Append braced content (literal, no substitution)
      size_t content_len = (p - 1) - content_start;
      word = append_to_word(ops, interp, word, content_start, content_len);

    } else if (*p == '"') {
      // Double-quoted string - process backslash escapes
      const char *quote_start = p;
      p++; // skip opening quote

      const char *seg_start = p;
      while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
          // Flush segment before backslash
          if (p > seg_start) {
            word = append_to_word(ops, interp, word, seg_start, p - seg_start);
          }
          p++; // skip backslash
          char escape_buf[4];
          size_t escape_len;
          size_t consumed = process_backslash(p, end, escape_buf, &escape_len);
          word = append_to_word(ops, interp, word, escape_buf, escape_len);
          p += consumed;
          seg_start = p;
        } else {
          p++;
        }
      }

      if (p >= end) {
        // Unclosed quotes
        TclObj result = ops->list.create(interp);
        TclObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        TclObj start_pos = ops->integer.create(interp, (int64_t)(quote_start - script));
        TclObj end_pos = ops->integer.create(interp, (int64_t)len);
        result = ops->list.push(interp, result, incomplete);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        ops->interp.set_result(interp, result);
        *status = TCL_PARSE_INCOMPLETE;
        return 0;
      }

      // Flush remaining segment
      if (p > seg_start) {
        word = append_to_word(ops, interp, word, seg_start, p - seg_start);
      }
      p++; // skip closing quote

      // Check for extra characters after close quote
      if (p < end && !is_word_terminator(*p)) {
        TclObj result = ops->list.create(interp);
        TclObj error_tag = ops->string.intern(interp, "ERROR", 5);
        TclObj start_pos = ops->integer.create(interp, (int64_t)(quote_start - script));
        TclObj end_pos = ops->integer.create(interp, (int64_t)len);
        TclObj msg = ops->string.intern(interp, "extra characters after close-quote", 34);
        result = ops->list.push(interp, result, error_tag);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        result = ops->list.push(interp, result, msg);
        ops->interp.set_result(interp, result);
        *status = TCL_PARSE_ERROR;
        return 0;
      }

    } else if (*p == '\\') {
      // Backslash in bare word
      p++; // skip backslash
      if (p < end) {
        if (*p == '\n') {
          // Backslash-newline in bare word acts as word terminator
          p++;
          while (p < end && is_whitespace(*p)) {
            p++;
          }
          break;
        }
        char escape_buf[4];
        size_t escape_len;
        size_t consumed = process_backslash(p, end, escape_buf, &escape_len);
        word = append_to_word(ops, interp, word, escape_buf, escape_len);
        p += consumed;
      }

    } else {
      // Regular character in bare word - collect a run of them
      const char *seg_start = p;
      while (p < end && !is_word_terminator(*p) &&
             *p != '{' && *p != '"' && *p != '\\') {
        p++;
      }
      if (p > seg_start) {
        word = append_to_word(ops, interp, word, seg_start, p - seg_start);
      }
    }
  }

  *pos = p - script;
  *status = TCL_PARSE_OK;

  // Handle empty word case (e.g., from "" or {})
  if (ops->list.is_nil(interp, word) && p > word_start) {
    return ops->string.intern(interp, "", 0);
  }

  return word;
}

TclParseStatus tcl_parse_command(const TclHostOps *ops, TclInterp interp,
                                  TclParseContext *ctx) {
  const char *script = ctx->script;
  size_t len = ctx->len;

  // Skip whitespace and comments between commands
  ctx->pos = skip_whitespace_and_comments(script, len, ctx->pos);

  // Check if we've reached the end
  if (ctx->pos >= len) {
    return TCL_PARSE_DONE;
  }

  const char *pos = script + ctx->pos;
  const char *end = script + len;

  // Skip command terminator if we're at one
  if (is_command_terminator(*pos) && *pos != '\0') {
    ctx->pos++;
    ctx->pos = skip_whitespace_and_comments(script, len, ctx->pos);
    if (ctx->pos >= len) {
      return TCL_PARSE_DONE;
    }
    pos = script + ctx->pos;
  }

  // Create a list to hold the words
  TclObj words = ops->list.create(interp);

  // Parse words until command terminator
  while (ctx->pos < len) {
    pos = script + ctx->pos;

    // Skip whitespace between words (but not command terminators)
    while (pos < end && is_whitespace(*pos)) {
      pos++;
      ctx->pos++;
    }

    // Also skip backslash-newline in whitespace context
    while (pos < end && *pos == '\\' && pos + 1 < end && pos[1] == '\n') {
      pos += 2;
      ctx->pos += 2;
      while (pos < end && is_whitespace(*pos)) {
        pos++;
        ctx->pos++;
      }
    }

    if (ctx->pos >= len) {
      break;
    }

    pos = script + ctx->pos;

    // Check for command terminator
    if (is_command_terminator(*pos)) {
      // Move past the terminator for next call
      if (*pos != '\0') {
        ctx->pos++;
      }
      break;
    }

    // Parse a word
    TclParseStatus status;
    TclObj word = parse_word(ops, interp, script, len, &ctx->pos, &status);
    if (status != TCL_PARSE_OK) {
      return status;
    }

    if (!ops->list.is_nil(interp, word)) {
      words = ops->list.push(interp, words, word);
    }
  }

  ops->interp.set_result(interp, words);
  return TCL_PARSE_OK;
}
