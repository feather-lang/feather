#include "feather.h"
#include "host.h"
#include "charclass.h"
#include "parse_helpers.h"

// Character classification helpers using int (for byte_at compatibility)
static int parse_is_whitespace(int c) {
  return c == ' ' || c == '\t';
}

static int is_command_terminator(int c) {
  return c == '\n' || c == '\r' || c == '\0' || c == ';' || c < 0;
}

static int is_word_terminator(int c) {
  return parse_is_whitespace(c) || is_command_terminator(c);
}

static int parse_is_hex_digit(int c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Check if character is valid in a variable name (excluding ::)
static int is_varname_char_base(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

// Check if we're at a namespace separator (::) using object-based access
static int is_namespace_sep_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj script, size_t pos, size_t len) {
  if (pos + 1 >= len) return 0;
  int c1 = ops->string.byte_at(interp, script, pos);
  int c2 = ops->string.byte_at(interp, script, pos + 1);
  return c1 == ':' && c2 == ':';
}

static int parse_hex_value(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  return 0;
}

/**
 * Process a backslash escape sequence using object-based byte access.
 * pos points to the character after the backslash.
 * Returns the number of characters consumed from input.
 * Writes the resulting character(s) to out_buf (must have space for 4 bytes).
 * Returns the number of bytes written via out_len.
 */
static size_t process_backslash_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj script, size_t pos, size_t len,
                                     char *out_buf, size_t *out_len) {
  if (pos >= len) {
    *out_len = 0;
    return 0;
  }

  int c = ops->string.byte_at(interp, script, pos);

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
      size_t p = pos + 1;
      while (p < len && parse_is_whitespace(ops->string.byte_at(interp, script, p))) {
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
      size_t p = pos + 1;
      int digits = 0;
      while (p < len && digits < 2) {
        int ch = ops->string.byte_at(interp, script, p);
        if (!parse_is_hex_digit(ch)) break;
        value = value * 16 + parse_hex_value(ch);
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
      size_t p = pos + 1;
      int digits = 0;
      while (p < len && digits < 4) {
        int ch = ops->string.byte_at(interp, script, p);
        if (!parse_is_hex_digit(ch)) break;
        value = value * 16 + parse_hex_value(ch);
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
      size_t p = pos + 1;
      int digits = 0;
      while (p < len && digits < 8) {
        int ch = ops->string.byte_at(interp, script, p);
        if (!parse_is_hex_digit(ch)) break;
        unsigned int new_val = value * 16 + parse_hex_value(ch);
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
      if (feather_is_octal_digit(c)) {
        // Octal escape: \ooo (1-3 octal digits, max 0377)
        int value = c - '0';
        size_t consumed = 1;
        size_t p = pos + 1;
        int digits = 1;
        while (p < len && digits < 3) {
          int ch = ops->string.byte_at(interp, script, p);
          if (!feather_is_octal_digit(ch)) break;
          int new_val = value * 8 + (ch - '0');
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
      *out_buf = (char)c;
      *out_len = 1;
      return 1;
  }
}

/**
 * Append a string segment to the current word being built.
 * Uses ops->string.slice to extract the segment.
 */
static FeatherObj append_slice_to_word(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj word, FeatherObj script,
                                        size_t start, size_t end) {
  if (start >= end) return word;
  FeatherObj segment = ops->string.slice(interp, script, start, end);
  if (ops->list.is_nil(interp, word)) {
    return segment;
  }
  return ops->string.concat(interp, word, segment);
}

/**
 * Append a literal string to the word.
 */
static FeatherObj append_literal_to_word(const FeatherHostOps *ops, FeatherInterp interp,
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
 * Returns the position of the matching ']', or len if not found.
 */
static size_t find_matching_bracket_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                         FeatherObj script, size_t pos, size_t len) {
  int depth = 1;

  while (pos < len && depth > 0) {
    int c = ops->string.byte_at(interp, script, pos);

    if (c == '\\' && pos + 1 < len) {
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
      while (pos < len && brace_depth > 0) {
        int ch = ops->string.byte_at(interp, script, pos);
        if (ch == '\\' && pos + 1 < len) {
          pos += 2;
          continue;
        }
        if (ch == '{') brace_depth++;
        else if (ch == '}') brace_depth--;
        pos++;
      }
      continue;
    }

    if (c == '"') {
      // Skip quoted content
      pos++;
      while (pos < len) {
        int ch = ops->string.byte_at(interp, script, pos);
        if (ch == '"') break;
        if (ch == '\\' && pos + 1 < len) {
          pos += 2;
          continue;
        }
        pos++;
      }
      if (pos < len) pos++; // skip closing quote
      continue;
    }

    pos++;
  }

  return (depth == 0) ? pos : len;
}

/**
 * Parse and substitute a variable starting at pos (after the $).
 * Uses object-based byte access.
 */
static FeatherResult substitute_variable_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj script, size_t len,
                                              size_t pos, FeatherObj word,
                                              FeatherObj *word_out, size_t *consumed_out) {
  if (pos >= len) {
    // Just a $ at end - treat as literal
    *word_out = append_literal_to_word(ops, interp, word, "$", 1);
    *consumed_out = 0;
    return TCL_OK;
  }

  int c = ops->string.byte_at(interp, script, pos);

  if (c == '{') {
    // ${name} form - scan until closing brace
    size_t name_start = pos + 1;
    size_t p = name_start;
    while (p < len && ops->string.byte_at(interp, script, p) != '}') {
      p++;
    }
    if (p >= len) {
      // No closing brace - treat $ as literal
      *word_out = append_literal_to_word(ops, interp, word, "$", 1);
      *consumed_out = 0;
      return TCL_OK;
    }
    // Found closing brace
    FeatherObj varName = ops->string.slice(interp, script, name_start, p);

    // Resolve the variable name (handles qualified names)
    FeatherObj ns, localName;
    feather_obj_resolve_variable(ops, interp, varName, &ns, &localName);

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
      FeatherObj msg3 = ops->string.intern(interp, "\": no such variable", 19);
      FeatherObj msg = ops->string.concat(interp, msg1, varName);
      msg = ops->string.concat(interp, msg, msg3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    // If word is empty, preserve object identity (avoid shimmering)
    if (ops->list.is_nil(interp, word)) {
      *word_out = value;
    } else {
      *word_out = ops->string.concat(interp, word, value);
    }
    *consumed_out = (p - pos) + 1; // +1 for closing brace
    return TCL_OK;
  } else if (is_varname_char_base(c) || is_namespace_sep_obj(ops, interp, script, pos, len)) {
    // $name form - scan valid variable name characters
    size_t name_start = pos;
    size_t p = pos;
    while (p < len) {
      int ch = ops->string.byte_at(interp, script, p);
      if (is_varname_char_base(ch)) {
        p++;
      } else if (is_namespace_sep_obj(ops, interp, script, p, len)) {
        p += 2; // Skip both colons
      } else {
        break;
      }
    }
    FeatherObj varName = ops->string.slice(interp, script, name_start, p);

    // Resolve the variable name (handles qualified names)
    FeatherObj ns, localName;
    feather_obj_resolve_variable(ops, interp, varName, &ns, &localName);

    FeatherObj value;
    if (ops->list.is_nil(interp, ns)) {
      value = ops->var.get(interp, localName);
    } else {
      value = ops->ns.get_var(interp, ns, localName);
    }

    if (ops->list.is_nil(interp, value)) {
      FeatherObj msg1 = ops->string.intern(interp, "can't read \"", 12);
      FeatherObj msg3 = ops->string.intern(interp, "\": no such variable", 19);
      FeatherObj msg = ops->string.concat(interp, msg1, varName);
      msg = ops->string.concat(interp, msg, msg3);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    if (ops->list.is_nil(interp, word)) {
      *word_out = value;
    } else {
      *word_out = ops->string.concat(interp, word, value);
    }
    *consumed_out = p - name_start;
    return TCL_OK;
  } else {
    // Not a valid variable - treat $ as literal
    *word_out = append_literal_to_word(ops, interp, word, "$", 1);
    *consumed_out = 0;
    return TCL_OK;
  }
}

/**
 * Parse and substitute a command starting at pos (after the [).
 * Returns the number of characters consumed (including the closing ]).
 * Returns (size_t)-1 on error.
 */
static size_t substitute_command_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                      FeatherObj script, size_t scriptLen,
                                      size_t pos, FeatherObj word,
                                      FeatherObj *word_out, FeatherParseStatus *status) {
  size_t bracket_start = pos - 1; // points to '['
  size_t close = find_matching_bracket_obj(ops, interp, script, pos, scriptLen);

  if (close >= scriptLen) {
    // Unclosed bracket
    FeatherObj result = ops->list.create(interp);
    FeatherObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
    FeatherObj start_pos = ops->integer.create(interp, (int64_t)bracket_start);
    FeatherObj end_pos = ops->integer.create(interp, (int64_t)scriptLen);
    result = ops->list.push(interp, result, incomplete);
    result = ops->list.push(interp, result, start_pos);
    result = ops->list.push(interp, result, end_pos);
    ops->interp.set_result(interp, result);
    *status = TCL_PARSE_INCOMPLETE;
    return (size_t)-1;
  }

  // Extract and evaluate the script between brackets
  FeatherObj cmdScript = ops->string.slice(interp, script, pos, close);
  FeatherResult eval_result = feather_script_eval_obj(ops, interp, cmdScript, TCL_EVAL_LOCAL);

  if (eval_result != TCL_OK) {
    *status = TCL_PARSE_ERROR;
    return (size_t)-1;
  }

  // Get the result and append to word
  FeatherObj cmd_result = ops->interp.get_result(interp);
  if (!ops->list.is_nil(interp, cmd_result)) {
    if (ops->list.is_nil(interp, word)) {
      *word_out = cmd_result;
    } else {
      *word_out = ops->string.concat(interp, word, cmd_result);
    }
  } else {
    *word_out = word;
  }

  return (close - pos) + 1; // +1 for closing bracket
}

// ============================================================================
// Object-based parser context
// ============================================================================

void feather_parse_init_obj(FeatherParseContextObj *ctx, FeatherObj script, size_t len) {
  ctx->script = script;
  ctx->len = len;
  ctx->pos = 0;
}

/**
 * Skip whitespace in list context (spaces, tabs, newlines).
 */
static size_t skip_list_whitespace_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                        FeatherObj s, size_t len, size_t pos) {
  while (pos < len) {
    int c = ops->string.byte_at(interp, s, pos);
    if (c != ' ' && c != '\t' && c != '\n') break;
    pos++;
  }
  return pos;
}

/**
 * Parse a single element from a list string using object-based access.
 */
static FeatherResult parse_list_element_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj s, size_t len, size_t *pos,
                                             FeatherObj *elem_out) {
  // Skip leading whitespace
  *pos = skip_list_whitespace_obj(ops, interp, s, len, *pos);

  if (*pos >= len) {
    *elem_out = 0; // nil - no more elements
    return TCL_OK;
  }

  int c = ops->string.byte_at(interp, s, *pos);
  FeatherObj word = 0;

  if (c == '{') {
    // Braced element - content is literal, braces nest
    int depth = 1;
    size_t content_start = *pos + 1;
    (*pos)++;
    while (*pos < len && depth > 0) {
      int ch = ops->string.byte_at(interp, s, *pos);
      if (ch == '\\' && *pos + 1 < len) {
        (*pos) += 2;
        continue;
      }
      if (ch == '{') depth++;
      else if (ch == '}') depth--;
      (*pos)++;
    }

    if (depth > 0) {
      FeatherObj msg = ops->string.intern(interp, "unmatched open brace in list", 28);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Content is from content_start to pos-1 (before closing brace)
    *elem_out = ops->string.slice(interp, s, content_start, *pos - 1);
    return TCL_OK;
  } else if (c == '"') {
    // Quoted element - process backslash escapes
    size_t seg_start = *pos + 1;
    (*pos)++;
    while (*pos < len) {
      int ch = ops->string.byte_at(interp, s, *pos);
      if (ch == '"') break;
      if (ch == '\\' && *pos + 1 < len) {
        // Flush segment before backslash
        if (*pos > seg_start) {
          word = append_slice_to_word(ops, interp, word, s, seg_start, *pos);
        }
        (*pos)++;
        char escape_buf[4];
        size_t escape_len;
        size_t consumed = process_backslash_obj(ops, interp, s, *pos, len, escape_buf, &escape_len);
        word = append_literal_to_word(ops, interp, word, escape_buf, escape_len);
        *pos += consumed;
        seg_start = *pos;
      } else {
        (*pos)++;
      }
    }

    if (*pos >= len) {
      FeatherObj msg = ops->string.intern(interp, "unmatched open quote in list", 28);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    // Flush remaining segment
    if (*pos > seg_start) {
      word = append_slice_to_word(ops, interp, word, s, seg_start, *pos);
    }
    (*pos)++; // skip closing quote

    if (ops->list.is_nil(interp, word)) {
      word = ops->string.intern(interp, "", 0);
    }
    *elem_out = word;
    return TCL_OK;
  } else {
    // Bare word - scan until whitespace, process backslashes
    size_t seg_start = *pos;
    while (*pos < len) {
      int ch = ops->string.byte_at(interp, s, *pos);
      if (ch == ' ' || ch == '\t' || ch == '\n') break;
      if (ch == '\\' && *pos + 1 < len) {
        // Flush segment before backslash
        if (*pos > seg_start) {
          word = append_slice_to_word(ops, interp, word, s, seg_start, *pos);
        }
        (*pos)++;
        char escape_buf[4];
        size_t escape_len;
        size_t consumed = process_backslash_obj(ops, interp, s, *pos, len, escape_buf, &escape_len);
        word = append_literal_to_word(ops, interp, word, escape_buf, escape_len);
        *pos += consumed;
        seg_start = *pos;
      } else {
        (*pos)++;
      }
    }

    // Flush remaining segment
    if (*pos > seg_start) {
      word = append_slice_to_word(ops, interp, word, s, seg_start, *pos);
    }

    if (ops->list.is_nil(interp, word)) {
      word = ops->string.intern(interp, "", 0);
    }
    *elem_out = word;
    return TCL_OK;
  }
}

/**
 * Parse a string as a TCL list using object-based access.
 */
FeatherObj feather_list_parse_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                   FeatherObj s) {
  ops = feather_get_ops(ops);
  size_t len = ops->string.byte_length(interp, s);
  FeatherObj result = ops->list.create(interp);
  size_t pos = 0;

  while (pos < len) {
    FeatherObj elem;
    FeatherResult status = parse_list_element_obj(ops, interp, s, len, &pos, &elem);
    if (status != TCL_OK) {
      return 0;  // error already set in interp result
    }
    if (ops->list.is_nil(interp, elem)) {
      break;
    }
    result = ops->list.push(interp, result, elem);
  }

  return result;
}

/**
 * Skip whitespace, backslash-newline continuations, and comments.
 */
static size_t skip_whitespace_and_comments_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                                FeatherObj script, size_t len, size_t pos) {
  while (pos < len) {
    int c = ops->string.byte_at(interp, script, pos);

    // Skip whitespace
    if (parse_is_whitespace(c)) {
      pos++;
      continue;
    }

    // Skip backslash-newline continuation
    if (c == '\\' && pos + 1 < len) {
      int c2 = ops->string.byte_at(interp, script, pos + 1);
      if (c2 == '\n') {
        pos += 2;
        while (pos < len && parse_is_whitespace(ops->string.byte_at(interp, script, pos))) {
          pos++;
        }
        continue;
      }
    }

    // Skip comments (# at start of command)
    if (c == '#') {
      while (pos < len && ops->string.byte_at(interp, script, pos) != '\n') {
        pos++;
      }
      if (pos < len) {
        pos++; // skip newline
      }
      continue;
    }

    break;
  }

  return pos;
}

/**
 * Parse a single word using object-based access.
 */
static FeatherObj parse_word_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj script, size_t len, size_t *pos,
                                  FeatherParseStatus *status) {
  size_t p = *pos;
  FeatherObj word = 0;
  size_t word_start = p;

  while (p < len && !is_word_terminator(ops->string.byte_at(interp, script, p))) {
    int c = ops->string.byte_at(interp, script, p);

    if (c == '{') {
      // Braced string - no substitutions, content is literal
      int depth = 1;
      size_t brace_start = p;
      size_t content_start = p + 1;
      p++;
      while (p < len && depth > 0) {
        int ch = ops->string.byte_at(interp, script, p);
        if (ch == '\\' && p + 1 < len) {
          p++;
          if (p < len) p++;
          continue;
        }
        if (ch == '{') depth++;
        else if (ch == '}') depth--;
        p++;
      }

      if (depth > 0) {
        // Unclosed braces
        FeatherObj result = ops->list.create(interp);
        FeatherObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)brace_start);
        FeatherObj end_pos = ops->integer.create(interp, (int64_t)len);
        result = ops->list.push(interp, result, incomplete);
        result = ops->list.push(interp, result, start_pos);
        result = ops->list.push(interp, result, end_pos);
        ops->interp.set_result(interp, result);
        *status = TCL_PARSE_INCOMPLETE;
        return 0;
      }

      // Check for extra characters after close brace
      if (p < len && !is_word_terminator(ops->string.byte_at(interp, script, p))) {
        FeatherObj result = ops->list.create(interp);
        FeatherObj error_tag = ops->string.intern(interp, "ERROR", 5);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)brace_start);
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
      word = append_slice_to_word(ops, interp, word, script, content_start, p - 1);

    } else if (c == '"') {
      // Double-quoted string
      size_t quote_start = p;
      p++; // skip opening quote

      size_t seg_start = p;
      while (p < len && ops->string.byte_at(interp, script, p) != '"') {
        int ch = ops->string.byte_at(interp, script, p);
        if (ch == '\\' && p + 1 < len) {
          // Flush segment before backslash
          if (p > seg_start) {
            word = append_slice_to_word(ops, interp, word, script, seg_start, p);
          }
          p++; // skip backslash
          char escape_buf[4];
          size_t escape_len;
          size_t consumed = process_backslash_obj(ops, interp, script, p, len, escape_buf, &escape_len);
          word = append_literal_to_word(ops, interp, word, escape_buf, escape_len);
          p += consumed;
          seg_start = p;
        } else if (ch == '$') {
          // Flush segment before $
          if (p > seg_start) {
            word = append_slice_to_word(ops, interp, word, script, seg_start, p);
          }
          p++; // skip $
          size_t consumed;
          if (substitute_variable_obj(ops, interp, script, len, p, word, &word, &consumed) != TCL_OK) {
            *status = TCL_PARSE_ERROR;
            return 0;
          }
          p += consumed;
          seg_start = p;
        } else if (ch == '[') {
          // Flush segment before [
          if (p > seg_start) {
            word = append_slice_to_word(ops, interp, word, script, seg_start, p);
          }
          p++; // skip [
          size_t consumed = substitute_command_obj(ops, interp, script, len, p, word, &word, status);
          if (consumed == (size_t)-1) {
            return 0;
          }
          p += consumed;
          seg_start = p;
        } else {
          p++;
        }
      }

      if (p >= len) {
        // Unclosed quotes
        FeatherObj result = ops->list.create(interp);
        FeatherObj incomplete = ops->string.intern(interp, "INCOMPLETE", 10);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)quote_start);
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
        word = append_slice_to_word(ops, interp, word, script, seg_start, p);
      }
      p++; // skip closing quote

      // Check for extra characters after close quote
      if (p < len && !is_word_terminator(ops->string.byte_at(interp, script, p))) {
        FeatherObj result = ops->list.create(interp);
        FeatherObj error_tag = ops->string.intern(interp, "ERROR", 5);
        FeatherObj start_pos = ops->integer.create(interp, (int64_t)quote_start);
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

    } else if (c == '\\') {
      // Backslash in bare word
      p++; // skip backslash
      if (p < len) {
        int ch = ops->string.byte_at(interp, script, p);
        if (ch == '\n') {
          // Backslash-newline in bare word acts as word terminator
          p++;
          while (p < len && parse_is_whitespace(ops->string.byte_at(interp, script, p))) {
            p++;
          }
          break;
        }
        char escape_buf[4];
        size_t escape_len;
        size_t consumed = process_backslash_obj(ops, interp, script, p, len, escape_buf, &escape_len);
        word = append_literal_to_word(ops, interp, word, escape_buf, escape_len);
        p += consumed;
      }

    } else if (c == '$') {
      // Variable substitution in bare word
      p++; // skip $
      size_t consumed;
      if (substitute_variable_obj(ops, interp, script, len, p, word, &word, &consumed) != TCL_OK) {
        *status = TCL_PARSE_ERROR;
        return 0;
      }
      p += consumed;

    } else if (c == '[') {
      // Command substitution in bare word
      p++; // skip [
      size_t consumed = substitute_command_obj(ops, interp, script, len, p, word, &word, status);
      if (consumed == (size_t)-1) {
        return 0;
      }
      p += consumed;

    } else {
      // Regular character in bare word - collect a run of them
      size_t seg_start = p;
      while (p < len) {
        int ch = ops->string.byte_at(interp, script, p);
        if (is_word_terminator(ch) || ch == '{' || ch == '"' || ch == '\\' || ch == '$' || ch == '[') {
          break;
        }
        p++;
      }
      if (p > seg_start) {
        word = append_slice_to_word(ops, interp, word, script, seg_start, p);
      }
    }
  }

  *pos = p;
  *status = TCL_PARSE_OK;

  // Handle empty word case (e.g., from "" or {})
  if (ops->list.is_nil(interp, word) && p > word_start) {
    return ops->string.intern(interp, "", 0);
  }

  return word;
}

/**
 * feather_subst_obj performs substitutions on a string using object-based access.
 */
FeatherResult feather_subst_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj str, int flags) {
  ops = feather_get_ops(ops);
  size_t len = ops->string.byte_length(interp, str);
  size_t p = 0;
  FeatherObj result = 0;
  size_t seg_start = 0;

  while (p < len) {
    int c = ops->string.byte_at(interp, str, p);

    if (c == '\\' && (flags & TCL_SUBST_BACKSLASHES)) {
      // Flush segment before backslash
      if (p > seg_start) {
        result = append_slice_to_word(ops, interp, result, str, seg_start, p);
      }
      p++;  // skip backslash
      if (p < len) {
        char escape_buf[4];
        size_t escape_len;
        size_t consumed = process_backslash_obj(ops, interp, str, p, len, escape_buf, &escape_len);
        result = append_literal_to_word(ops, interp, result, escape_buf, escape_len);
        p += consumed;
      }
      seg_start = p;

    } else if (c == '$' && (flags & TCL_SUBST_VARIABLES)) {
      // Flush segment before $
      if (p > seg_start) {
        result = append_slice_to_word(ops, interp, result, str, seg_start, p);
      }
      p++;  // skip $
      size_t consumed;
      if (substitute_variable_obj(ops, interp, str, len, p, result, &result, &consumed) != TCL_OK) {
        return TCL_ERROR;
      }
      p += consumed;
      seg_start = p;

    } else if (c == '[' && (flags & TCL_SUBST_COMMANDS)) {
      // Flush segment before [
      if (p > seg_start) {
        result = append_slice_to_word(ops, interp, result, str, seg_start, p);
      }
      p++;  // skip [

      // Find matching close bracket
      size_t close = find_matching_bracket_obj(ops, interp, str, p, len);
      if (close >= len) {
        // Unclosed bracket - error
        FeatherObj msg = ops->string.intern(interp, "missing close-bracket", 21);
        ops->interp.set_result(interp, msg);
        return TCL_ERROR;
      }

      // Evaluate the command between brackets
      FeatherObj cmdScript = ops->string.slice(interp, str, p, close);
      FeatherResult eval_result = feather_script_eval_obj(ops, interp, cmdScript, TCL_EVAL_LOCAL);
      if (eval_result != TCL_OK) {
        return TCL_ERROR;
      }

      // Append command result
      FeatherObj cmd_result = ops->interp.get_result(interp);
      if (!ops->list.is_nil(interp, cmd_result)) {
        if (ops->list.is_nil(interp, result)) {
          result = cmd_result;
        } else {
          result = ops->string.concat(interp, result, cmd_result);
        }
      }

      p = close + 1;  // skip past ]
      seg_start = p;

    } else {
      p++;
    }
  }

  // Flush remaining segment
  if (p > seg_start) {
    result = append_slice_to_word(ops, interp, result, str, seg_start, p);
  }

  // Handle empty result
  if (ops->list.is_nil(interp, result)) {
    result = ops->string.intern(interp, "", 0);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

/**
 * Object-based parse_command implementation.
 */
FeatherParseStatus feather_parse_command_obj(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherParseContextObj *ctx) {
  ops = feather_get_ops(ops);
  FeatherObj script = ctx->script;
  size_t len = ctx->len;

  // Skip whitespace and comments between commands
  ctx->pos = skip_whitespace_and_comments_obj(ops, interp, script, len, ctx->pos);

  // Check if we've reached the end
  if (ctx->pos >= len) {
    return TCL_PARSE_DONE;
  }

  int c = ops->string.byte_at(interp, script, ctx->pos);

  // Skip command terminator if we're at one
  if (is_command_terminator(c) && c != '\0' && c >= 0) {
    ctx->pos++;
    ctx->pos = skip_whitespace_and_comments_obj(ops, interp, script, len, ctx->pos);
    if (ctx->pos >= len) {
      return TCL_PARSE_DONE;
    }
  }

  // Create a list to hold the words
  FeatherObj words = ops->list.create(interp);

  // Parse words until command terminator
  while (ctx->pos < len) {
    // Skip whitespace between words (but not command terminators)
    while (ctx->pos < len && parse_is_whitespace(ops->string.byte_at(interp, script, ctx->pos))) {
      ctx->pos++;
    }

    // Also skip backslash-newline in whitespace context
    while (ctx->pos < len) {
      int ch = ops->string.byte_at(interp, script, ctx->pos);
      if (ch != '\\') break;
      if (ctx->pos + 1 >= len) break;
      int ch2 = ops->string.byte_at(interp, script, ctx->pos + 1);
      if (ch2 != '\n') break;
      ctx->pos += 2;
      while (ctx->pos < len && parse_is_whitespace(ops->string.byte_at(interp, script, ctx->pos))) {
        ctx->pos++;
      }
    }

    if (ctx->pos >= len) {
      break;
    }

    c = ops->string.byte_at(interp, script, ctx->pos);

    // Check for command terminator
    if (is_command_terminator(c)) {
      // Move past the terminator for next call
      if (c != '\0' && c >= 0) {
        ctx->pos++;
      }
      break;
    }

    // Check for argument expansion {*}
    int is_expansion = 0;
    if (ctx->pos + 3 <= len) {
      int c1 = ops->string.byte_at(interp, script, ctx->pos);
      int c2 = ops->string.byte_at(interp, script, ctx->pos + 1);
      int c3 = ops->string.byte_at(interp, script, ctx->pos + 2);
      if (c1 == '{' && c2 == '*' && c3 == '}' && ctx->pos + 3 < len) {
        int c4 = ops->string.byte_at(interp, script, ctx->pos + 3);
        if (!is_word_terminator(c4)) {
          is_expansion = 1;
          ctx->pos += 3; // skip {*}
        }
      }
    }

    // Parse a word
    FeatherParseStatus status;
    FeatherObj word = parse_word_obj(ops, interp, script, len, &ctx->pos, &status);
    if (status != TCL_PARSE_OK) {
      return status;
    }

    if (!ops->list.is_nil(interp, word)) {
      if (is_expansion) {
        // Parse word as a list using list.from()
        FeatherObj list = ops->list.from(interp, word);

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

// ============================================================================
// Compatibility layer - char* based API (for backward compatibility)
// ============================================================================

void feather_parse_init(FeatherParseContext *ctx, const char *script, size_t len) {
  ctx->script = script;
  ctx->len = len;
  ctx->pos = 0;
}

FeatherObj feather_list_parse(const FeatherHostOps *ops, FeatherInterp interp,
                               const char *s, size_t len) {
  ops = feather_get_ops(ops);
  FeatherObj str = ops->string.intern(interp, s, len);
  return feather_list_parse_obj(ops, interp, str);
}

FeatherResult feather_subst(const FeatherHostOps *ops, FeatherInterp interp,
                             const char *str, size_t len, int flags) {
  ops = feather_get_ops(ops);
  FeatherObj strObj = ops->string.intern(interp, str, len);
  return feather_subst_obj(ops, interp, strObj, flags);
}

FeatherParseStatus feather_parse_command(const FeatherHostOps *ops, FeatherInterp interp,
                                          FeatherParseContext *ctx) {
  ops = feather_get_ops(ops);
  // Convert char* context to object-based context
  FeatherObj script = ops->string.intern(interp, ctx->script, ctx->len);
  FeatherParseContextObj objCtx;
  feather_parse_init_obj(&objCtx, script, ctx->len);
  objCtx.pos = ctx->pos;

  FeatherParseStatus status = feather_parse_command_obj(ops, interp, &objCtx);

  // Update char* context position
  ctx->pos = objCtx.pos;
  return status;
}
