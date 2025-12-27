#include "index_parse.h"

// Helper to get byte at position with bounds checking
static int get_byte(const FeatherHostOps *ops, FeatherInterp interp,
                    FeatherObj obj, size_t pos) {
  return ops->string.byte_at(interp, obj, pos);
}

static int parse_int64_obj(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj obj, size_t len, size_t *pos, int64_t *out) {
  if (*pos >= len) return 0;
  int ch = get_byte(ops, interp, obj, *pos);
  int negative = 0;
  if (ch == '-') {
    negative = 1;
    (*pos)++;
    ch = get_byte(ops, interp, obj, *pos);
  } else if (ch == '+') {
    (*pos)++;
    ch = get_byte(ops, interp, obj, *pos);
  }
  if (*pos >= len || ch < '0' || ch > '9') return 0;
  int64_t val = 0;
  while (*pos < len) {
    ch = get_byte(ops, interp, obj, *pos);
    if (ch < '0' || ch > '9') break;
    val = val * 10 + (ch - '0');
    (*pos)++;
  }
  *out = negative ? -val : val;
  return 1;
}

FeatherResult feather_parse_index(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj indexObj, size_t listLen, int64_t *out) {
  size_t len = ops->string.byte_length(interp, indexObj);
  size_t pos = 0;
  int64_t base;
  int is_end = 0;

  // Check for "end"
  int c0 = get_byte(ops, interp, indexObj, 0);
  int c1 = get_byte(ops, interp, indexObj, 1);
  int c2 = get_byte(ops, interp, indexObj, 2);
  if (len >= 3 && c0 == 'e' && c1 == 'n' && c2 == 'd') {
    base = (int64_t)listLen - 1;
    pos = 3;
    is_end = 1;
  } else {
    if (!parse_int64_obj(ops, interp, indexObj, len, &pos, &base)) {
      goto bad_index;
    }
  }

  while (pos < len) {
    int ch = get_byte(ops, interp, indexObj, pos);
    if (ch == '+' || ch == '-') {
      int op = ch;
      pos++;
      int64_t offset;
      if (!parse_int64_obj(ops, interp, indexObj, len, &pos, &offset)) {
        goto bad_index;
      }
      if (op == '+') {
        base += offset;
      } else {
        base -= offset;
      }
    } else {
      goto bad_index;
    }
  }

  *out = base;
  (void)is_end;
  return TCL_OK;

bad_index:;
  FeatherObj msg = ops->string.intern(interp, "bad index \"", 11);
  msg = ops->string.concat(interp, msg, indexObj);
  FeatherObj suffix = ops->string.intern(interp,
    "\": must be integer?[+-]integer? or end?[+-]integer?", 51);
  msg = ops->string.concat(interp, msg, suffix);
  ops->interp.set_result(interp, msg);
  return TCL_ERROR;
}
