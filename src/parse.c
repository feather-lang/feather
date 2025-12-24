#include "feather.h"

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

// Check if character is valid in a variable name (excluding ::)
static int is_varname_char_base(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

// Check if we're at a valid namespace separator (::)
// Returns 1 if at ::, 0 otherwise
static int is_namespace_sep(const char *p, const char *end) {
  return (p + 1 < end) && (p[0] == ':') && (p[1] == ':');
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
static FeatherObj append_to_word(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj word, const char *s, size_t len) {
  FeatherObj segment = ops->string.intern(interp, s, len);
  if (ops->list.is_nil(interp, word)) {
    return segment;
  }
  return ops->string.concat(interp, word, segment);
}

/**
 * Find the matching close bracket for command substitution.
 * pos points to the character after the opening '['.
 * Returns the position of the matching ']', or NULL if not found.
 */
static const char *find_matching_bracket(const char *pos, const char *end) {
  int depth = 1;

  while (pos < end && depth > 0) {
    char c = *pos;

    if (c == '\\' && pos + 1 < end) {
      // Skip escaped character
      pos += 2;
      continue;
    }

    if (c == '[') {
      depth++;
      pos++;
      continue;
    }

    if (c == ']') {
      depth--;
      if (depth == 0) {
        return pos;
      }
      pos++;
      continue;
    }

    if (c == '{') {
      // Skip braced content (no substitution, braces nest)
      int brace_depth = 1;
      pos++;
      while (pos < end && brace_depth > 0) {
        if (*pos == '\\' && pos + 1 < end) {
          pos += 2;
          continue;
        }
        if (*pos == '{') brace_depth++;
        else if (*pos == '}') brace_depth--;
        pos++;
      }
      continue;
    }

    if (c == '"') {
      // Skip quoted content (need to match quotes properly)
      pos++;
      while (pos < end && *pos != '"') {
        if (*pos == '\\' && pos + 1 < end) {
          pos += 2;
          continue;
        }
        pos++;
      }
      if (pos < end) pos++; // skip closing quote
      continue;
    }

    pos++;
  }

  return (depth == 0) ? pos : NULL;
}

/**
 * Parse and substitute a variable starting at pos (after the $).
 * Returns the number of characters consumed via consumed_out.
 * Appends the variable value to word via word_out.
 * Returns TCL_OK on success, TCL_ERROR if variable doesn't exist.
 */
static FeatherResult substitute_variable(const FeatherHostOps *ops, FeatherInterp interp,
                                     const char *pos, const char *end,
                                     FeatherObj word, FeatherObj *word_out,
                                     size_t *consumed_out) {
  if (pos >= end) {
    // Just a $ at end - treat as literal
    *word_out = append_to_word(ops, interp, word, "$", 1);
    *consumed_out = 0;
    return TCL_OK;
  }

  if (*pos == '{') {
    // ${name} form - scan until closing brace
    const char *name_start = pos + 1;
    const char *p = name_start;
    while (p < end && *p != '}') {
      p++;
    }
    if (p >= end) {
      // No closing brace - treat $ as literal
      *word_out = append_to_word(ops, interp, word, "$", 1);
      *consumed_out = 0;
      return TCL_OK;
    }
    // Found closing brace
    size_t name_len = p - name_start;

    // Resolve the variable name (handles qualified names)
    FeatherObj ns, localName;
    feather_resolve_variable(ops, interp, name_start, name_len, &ns, &localName);

    FeatherObj value;
    if (ops->list.is_nil(interp, ns)) {
      // Unqualified - frame-local lookup
      value = ops->var.get(interp, localName);
    } else {
      // Qualified - namespace lookup
      value = ops->ns.get_var(interp, ns, localName);
    }

    if (ops->list.is_nil(interp, value)) {
      // Variable not found - raise error
      FeatherObj msg1 = ops->string.intern(interp, "can't read \"", 12);
      FeatherObj msg2 = ops->string.intern(interp, name_start, name_len);
      FeatherObj msg3 = ops->string.intern(interp, "\": no such variable", 19);
      FeatherObj msg = ops->string.concat(interp, msg1, msg2);
      msg = ops->string.concat(interp, msg, msg3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    // Get string representation of value
    size_t val_len;
    const char *val_str = ops->string.get(interp, value, &val_len);
    *word_out = append_to_word(ops, interp, word, val_str, val_len);
    *consumed_out = (p - pos) + 1; // +1 for closing brace
    return TCL_OK;
  } else if (is_varname_char_base(*pos) || is_namespace_sep(pos, end)) {
    // $name form - scan valid variable name characters
    // Variable names can contain alphanumerics, underscores, and ::
    // But NOT a single : (that terminates the name)
    const char *name_start = pos;
    const char *p = pos;
    while (p < end) {
      if (is_varname_char_base(*p)) {
        p++;
      } else if (is_namespace_sep(p, end)) {
        p += 2; // Skip both colons
      } else {
        break;
      }
    }
    size_t name_len = p - name_start;

    // Resolve the variable name (handles qualified names)
    FeatherObj ns, localName;
    feather_resolve_variable(ops, interp, name_start, name_len, &ns, &localName);

    FeatherObj value;
    if (ops->list.is_nil(interp, ns)) {
      // Unqualified - frame-local lookup
      value = ops->var.get(interp, localName);
    } else {
      // Qualified - namespace lookup
      value = ops->ns.get_var(interp, ns, localName);
    }

    if (ops->list.is_nil(interp, value)) {
      // Variable not found - raise error
      FeatherObj msg1 = ops->string.intern(interp, "can't read \"", 12);
      FeatherObj msg2 = ops->string.intern(interp, name_start, name_len);
      FeatherObj msg3 = ops->string.intern(interp, "\": no such variable", 19);
      FeatherObj msg = ops->string.concat(interp, msg1, msg2);
      msg = ops->string.concat(interp, msg, msg3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    // Get string representation of value
    size_t val_len;
    const char *val_str = ops->string.get(interp, value, &val_len);
    *word_out = append_to_word(ops, interp, word, val_str, val_len);
    *consumed_out = name_len;
    return TCL_OK;
  } else {
    // Not a valid variable - treat $ as literal
    *word_out = append_to_word(ops, interp, word, "$", 1);
    *consumed_out = 0;
    return TCL_OK;
  }
}

/**
 * Parse and substitute a command starting at pos (after the [).
 * Returns the number of characters consumed (including the closing ]).
 * Appends the command result to word via word_out.
 * Returns -1 on error (sets status).
 */
static int substitute_command(const FeatherHostOps *ops, FeatherInterp interp,
                              const char *script, size_t script_len,
                              const char *pos, const char *end,
                              FeatherObj word, FeatherObj *word_out,
                              FeatherParseStatus *status) {
  const char *bracket_start = pos - 1; // points to '['
  const char *close = find_matching_bracket(pos, end);

  if (close == NULL) {
    // Unclosed bracket
    FeatherObj result = ops->list.create(interp);
    FeatherObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
    FeatherObj start_pos = ops->integer.create(interp, (int64_t)(bracket_start - script));
    FeatherObj end_pos = ops->integer.create(interp, (int64_t)script_len);
    result = ops->list.push(interp, result, incomplete);
    result = ops->list.push(interp, result, start_pos);
    result = ops->list.push(interp, result, end_pos);
    ops->interp.set_result(interp, result);
    *status = TCL_PARSE_INCOMPLETE;
    return -1;
  }

  // Extract and evaluate the script between brackets
  size_t cmd_len = close - pos;
  FeatherResult eval_result = feather_script_eval(ops, interp, pos, cmd_len, TCL_EVAL_LOCAL);

  if (eval_result != TCL_OK) {
    *status = TCL_PARSE_ERROR;
    return -1;
  }

  // Get the result and append to word
  FeatherObj cmd_result = ops->interp.get_result(interp);
  if (!ops->list.is_nil(interp, cmd_result)) {
    size_t result_len;
    const char *result_str = ops->string.get(interp, cmd_result, &result_len);
    *word_out = append_to_word(ops, interp, word, result_str, result_len);
  } else {
    *word_out = word;
  }

  return (close - pos) + 1; // +1 for closing bracket
}

void feather_parse_init(FeatherParseContext *ctx, const char *script, size_t len) {
  ctx->script = script;
  ctx->len = len;
  ctx->pos = 0;
}

/**
 * Skip whitespace in list context (spaces and tabs only, no comments).
 */
static size_t skip_list_whitespace(const char *s, size_t len, size_t pos) {
  while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n')) {
    pos++;
  }
  return pos;
}

/**
 * Parse a single element from a list string starting at pos.
 * List parsing differs from word parsing:
 * - No variable substitution
 * - No command substitution
 * - Braces and quotes delimit elements, backslash-newline becomes space
 *
 * Returns the parsed element, or nil if at end.
 * Updates pos to point past the parsed element.
 */
static FeatherObj parse_list_element(const FeatherHostOps *ops, FeatherInterp interp,
                                  const char *s, size_t len, size_t *pos) {
  // Skip leading whitespace
  *pos = skip_list_whitespace(s, len, *pos);

  if (*pos >= len) {
    return 0; // nil - no more elements
  }

  const char *p = s + *pos;
  const char *end = s + len;
  FeatherObj word = 0;

  if (*p == '{') {
    // Braced element - content is literal, braces nest
    int depth = 1;
    const char *content_start = p + 1;
    p++;
    while (p < end && depth > 0) {
      if (*p == '\\' && p + 1 < end) {
        p += 2;
        continue;
      }
      if (*p == '{') depth++;
      else if (*p == '}') depth--;
      p++;
    }
    if (depth == 0) {
      size_t content_len = (p - 1) - content_start;
      word = ops->string.intern(interp, content_start, content_len);
    }
    *pos = p - s;
    return word;

  } else if (*p == '"') {
    // Quoted element - content includes everything until closing quote
    // Backslash escapes are processed
    const char *content_start = p + 1;
    p++;
    // For simplicity, just find the closing quote and return content
    // A full implementation would process backslash escapes
    const char *seg_start = p;
    while (p < end && *p != '"') {
      if (*p == '\\' && p + 1 < end) {
        p += 2;
        continue;
      }
      p++;
    }
    size_t content_len = p - content_start;
    word = ops->string.intern(interp, content_start, content_len);
    if (p < end) p++; // skip closing quote
    *pos = p - s;
    return word;

  } else {
    // Bare word - terminated by whitespace
    const char *word_start = p;
    while (p < end && *p != ' ' && *p != '\t' && *p != '\n') {
      if (*p == '\\' && p + 1 < end) {
        p += 2;
        continue;
      }
      p++;
    }
    size_t word_len = p - word_start;
    if (word_len > 0) {
      word = ops->string.intern(interp, word_start, word_len);
    }
    *pos = p - s;
    return word;
  }
}

/**
 * Parse a string as a TCL list and return a list of elements.
 */
static FeatherObj parse_as_list(const FeatherHostOps *ops, FeatherInterp interp,
                            const char *s, size_t len) {
  FeatherObj result = ops->list.create(interp);
  size_t pos = 0;

  while (pos < len) {
    FeatherObj elem = parse_list_element(ops, interp, s, len, &pos);
    if (ops->list.is_nil(interp, elem)) {
      break;
    }
    result = ops->list.push(interp, result, elem);
  }

  return result;
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
static FeatherObj parse_word(const FeatherHostOps *ops, FeatherInterp interp,
                         const char *script, size_t len, size_t *pos,
                         FeatherParseStatus *status) {
  const char *p = script + *pos;
  const char *end = script + len;
  FeatherObj word = 0;
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
        FeatherObj result = ops->list.create(interp);
        FeatherObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)(brace_start - script));
        FeatherObj end_pos = ops->integer.create(interp, (int64_t)len);
        result = ops->list.push(interp, result, incomplete);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        ops->interp.set_result(interp, result);
        *status = TCL_PARSE_INCOMPLETE;
        return 0;
      }

      // Check for extra characters after close brace
      if (p < end && !is_word_terminator(*p)) {
        FeatherObj result = ops->list.create(interp);
        FeatherObj error_tag = ops->string.intern(interp, "ERROR", 5);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)(brace_start - script));
        FeatherObj end_pos = ops->integer.create(interp, (int64_t)len);
        FeatherObj msg = ops->string.intern(interp, "extra characters after close-brace", 34);
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
      // Double-quoted string - process backslash escapes and variable substitution
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
        } else if (*p == '$') {
          // Flush segment before $
          if (p > seg_start) {
            word = append_to_word(ops, interp, word, seg_start, p - seg_start);
          }
          p++; // skip $
          size_t consumed;
          if (substitute_variable(ops, interp, p, end, word, &word, &consumed) != TCL_OK) {
            *status = TCL_PARSE_ERROR;
            return 0;
          }
          p += consumed;
          seg_start = p;
        } else if (*p == '[') {
          // Flush segment before [
          if (p > seg_start) {
            word = append_to_word(ops, interp, word, seg_start, p - seg_start);
          }
          p++; // skip [
          int consumed = substitute_command(ops, interp, script, len, p, end, word, &word, status);
          if (consumed < 0) {
            return 0;
          }
          p += consumed;
          seg_start = p;
        } else {
          p++;
        }
      }

      if (p >= end) {
        // Unclosed quotes
        FeatherObj result = ops->list.create(interp);
        FeatherObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)(quote_start - script));
        FeatherObj end_pos = ops->integer.create(interp, (int64_t)len);
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
        FeatherObj result = ops->list.create(interp);
        FeatherObj error_tag = ops->string.intern(interp, "ERROR", 5);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)(quote_start - script));
        FeatherObj end_pos = ops->integer.create(interp, (int64_t)len);
        FeatherObj msg = ops->string.intern(interp, "extra characters after close-quote", 34);
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

    } else if (*p == '$') {
      // Variable substitution in bare word
      p++; // skip $
      size_t consumed;
      if (substitute_variable(ops, interp, p, end, word, &word, &consumed) != TCL_OK) {
        *status = TCL_PARSE_ERROR;
        return 0;
      }
      p += consumed;

    } else if (*p == '[') {
      // Command substitution in bare word
      p++; // skip [
      int consumed = substitute_command(ops, interp, script, len, p, end, word, &word, status);
      if (consumed < 0) {
        return 0;
      }
      p += consumed;

    } else {
      // Regular character in bare word - collect a run of them
      const char *seg_start = p;
      while (p < end && !is_word_terminator(*p) &&
             *p != '{' && *p != '"' && *p != '\\' && *p != '$' && *p != '[') {
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

/**
 * feather_subst performs substitutions on a string.
 *
 * This is a standalone substitution function that can be used by:
 * - The expression parser for quoted strings
 * - The subst builtin command
 * - Any code needing TCL-style substitution
 */
FeatherResult feather_subst(const FeatherHostOps *ops, FeatherInterp interp,
                    const char *str, size_t len, int flags) {
  const char *p = str;
  const char *end = str + len;
  FeatherObj result = 0;  // nil initially
  const char *seg_start = p;

  while (p < end) {
    if (*p == '\\' && (flags & TCL_SUBST_BACKSLASHES)) {
      // Flush segment before backslash
      if (p > seg_start) {
        result = append_to_word(ops, interp, result, seg_start, p - seg_start);
      }
      p++;  // skip backslash
      if (p < end) {
        char escape_buf[4];
        size_t escape_len;
        size_t consumed = process_backslash(p, end, escape_buf, &escape_len);
        result = append_to_word(ops, interp, result, escape_buf, escape_len);
        p += consumed;
      }
      seg_start = p;

    } else if (*p == '$' && (flags & TCL_SUBST_VARIABLES)) {
      // Flush segment before $
      if (p > seg_start) {
        result = append_to_word(ops, interp, result, seg_start, p - seg_start);
      }
      p++;  // skip $
      size_t consumed;
      if (substitute_variable(ops, interp, p, end, result, &result, &consumed) != TCL_OK) {
        return TCL_ERROR;
      }
      p += consumed;
      seg_start = p;

    } else if (*p == '[' && (flags & TCL_SUBST_COMMANDS)) {
      // Flush segment before [
      if (p > seg_start) {
        result = append_to_word(ops, interp, result, seg_start, p - seg_start);
      }
      p++;  // skip [

      // Find matching close bracket
      const char *close = find_matching_bracket(p, end);
      if (close == NULL) {
        // Unclosed bracket - error
        FeatherObj msg = ops->string.intern(interp, "missing close-bracket", 21);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Evaluate the command between brackets
      size_t cmd_len = close - p;
      FeatherResult eval_result = feather_script_eval(ops, interp, p, cmd_len, TCL_EVAL_LOCAL);
      if (eval_result != TCL_OK) {
        return TCL_ERROR;
      }

      // Append command result
      FeatherObj cmd_result = ops->interp.get_result(interp);
      if (!ops->list.is_nil(interp, cmd_result)) {
        size_t result_len;
        const char *result_str = ops->string.get(interp, cmd_result, &result_len);
        result = append_to_word(ops, interp, result, result_str, result_len);
      }

      p = close + 1;  // skip past ]
      seg_start = p;

    } else {
      p++;
    }
  }

  // Flush remaining segment
  if (p > seg_start) {
    result = append_to_word(ops, interp, result, seg_start, p - seg_start);
  }

  // Handle empty result
  if (ops->list.is_nil(interp, result)) {
    result = ops->string.intern(interp, "", 0);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

FeatherParseStatus feather_parse_command(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherParseContext *ctx) {
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
  FeatherObj words = ops->list.create(interp);

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

    // Check for argument expansion {*}
    // Rule [5]: {*} followed by non-whitespace triggers expansion
    int is_expansion = 0;
    if (pos + 3 <= end &&
        pos[0] == '{' && pos[1] == '*' && pos[2] == '}' &&
        pos + 3 < end && !is_word_terminator(pos[3])) {
      is_expansion = 1;
      ctx->pos += 3; // skip {*}
    }

    // Parse a word
    FeatherParseStatus status;
    FeatherObj word = parse_word(ops, interp, script, len, &ctx->pos, &status);
    if (status != TCL_PARSE_OK) {
      return status;
    }

    if (!ops->list.is_nil(interp, word)) {
      if (is_expansion) {
        // Parse word as a list and add each element
        size_t word_len;
        const char *word_str = ops->string.get(interp, word, &word_len);
        FeatherObj list = parse_as_list(ops, interp, word_str, word_len);

        // Add each list element to words
        size_t list_len = ops->list.length(interp, list);
        for (size_t i = 0; i < list_len; i++) {
          FeatherObj elem = ops->list.shift(interp, list);
          if (!ops->list.is_nil(interp, elem)) {
            words = ops->list.push(interp, words, elem);
          }
        }
      } else {
        words = ops->list.push(interp, words, word);
      }
    }
  }

  ops->interp.set_result(interp, words);
  return TCL_PARSE_OK;
}
