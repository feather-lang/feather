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

TclParseStatus tcl_parse(const TclHostOps *ops, TclInterp interp,
                         const char *script, size_t len) {
  // Create a list to hold the tokens (words)
  TclObj words = ops->list.create(interp);

  const char *pos = script;
  const char *end = script + len;

  while (pos < end) {
    // Skip whitespace between words (including backslash-newline)
    while (pos < end) {
      if (is_whitespace(*pos)) {
        pos++;
      } else if (*pos == '\\' && pos + 1 < end && pos[1] == '\n') {
        // Backslash-newline continuation in whitespace context
        pos += 2;
        while (pos < end && is_whitespace(*pos)) {
          pos++;
        }
      } else {
        break;
      }
    }

    if (pos >= end) {
      break;
    }

    // Check for command terminator (newline, semicolon)
    if (is_command_terminator(*pos)) {
      break;
    }

    // Parse a word - can be a mix of quoted/unquoted segments
    TclObj word = 0; // nil - will be set when first segment is added
    const char *word_start = pos;

    while (pos < end && !is_word_terminator(*pos)) {
      if (*pos == '{') {
        // Braced string - no substitutions, content is literal
        int depth = 1;
        const char *brace_start = pos;
        const char *content_start = pos + 1;
        pos++;
        while (pos < end && depth > 0) {
          if (*pos == '\\' && pos + 1 < end) {
            // Backslash in braces: only \newline is special, but we still
            // need to skip the next char to avoid miscounting braces
            pos++;
            if (pos < end) pos++;
            continue;
          }
          if (*pos == '{') {
            depth++;
          } else if (*pos == '}') {
            depth--;
          }
          pos++;
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
          return TCL_PARSE_INCOMPLETE;
        }

        // Check for extra characters after close brace
        if (pos < end && !is_word_terminator(*pos)) {
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
          return TCL_PARSE_ERROR;
        }

        // Append braced content (literal, no substitution)
        size_t content_len = (pos - 1) - content_start;
        word = append_to_word(ops, interp, word, content_start, content_len);

      } else if (*pos == '"') {
        // Double-quoted string - process backslash escapes
        const char *quote_start = pos;
        pos++; // skip opening quote

        const char *seg_start = pos;
        while (pos < end && *pos != '"') {
          if (*pos == '\\' && pos + 1 < end) {
            // Flush segment before backslash
            if (pos > seg_start) {
              word = append_to_word(ops, interp, word, seg_start, pos - seg_start);
            }
            pos++; // skip backslash
            char escape_buf[4];
            size_t escape_len;
            size_t consumed = process_backslash(pos, end, escape_buf, &escape_len);
            word = append_to_word(ops, interp, word, escape_buf, escape_len);
            pos += consumed;
            seg_start = pos;
          } else {
            pos++;
          }
        }

        if (pos >= end) {
          // Unclosed quotes
          TclObj result = ops->list.create(interp);
          TclObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
          TclObj start_pos = ops->integer.create(interp, (int64_t)(quote_start - script));
          TclObj end_pos = ops->integer.create(interp, (int64_t)len);
          result = ops->list.push(interp, result, incomplete);
          result = ops->list.push(interp, result, start_pos);
          result = ops->list.push(interp, result, end_pos);
          ops->interp.set_result(interp, result);
          return TCL_PARSE_INCOMPLETE;
        }

        // Flush remaining segment
        if (pos > seg_start) {
          word = append_to_word(ops, interp, word, seg_start, pos - seg_start);
        }
        pos++; // skip closing quote

        // Check for extra characters after close quote
        if (pos < end && !is_word_terminator(*pos)) {
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
          return TCL_PARSE_ERROR;
        }

      } else if (*pos == '\\') {
        // Backslash in bare word
        pos++; // skip backslash
        if (pos < end) {
          if (*pos == '\n') {
            // Backslash-newline in bare word acts as word terminator (becomes space/separator)
            // Skip newline and following whitespace
            pos++;
            while (pos < end && is_whitespace(*pos)) {
              pos++;
            }
            break; // End this word
          }
          char escape_buf[4];
          size_t escape_len;
          size_t consumed = process_backslash(pos, end, escape_buf, &escape_len);
          word = append_to_word(ops, interp, word, escape_buf, escape_len);
          pos += consumed;
        }

      } else {
        // Regular character in bare word - collect a run of them
        const char *seg_start = pos;
        while (pos < end && !is_word_terminator(*pos) &&
               *pos != '{' && *pos != '"' && *pos != '\\') {
          pos++;
        }
        if (pos > seg_start) {
          word = append_to_word(ops, interp, word, seg_start, pos - seg_start);
        }
      }
    }

    // Check if we built a word
    if (!ops->list.is_nil(interp, word)) {
      words = ops->list.push(interp, words, word);
    } else if (pos > word_start) {
      // Empty word (e.g., from "" or {})
      TclObj empty = ops->string.intern(interp, "", 0);
      words = ops->list.push(interp, words, empty);
    }
  }

  ops->interp.set_result(interp, words);
  return TCL_PARSE_OK;
}
