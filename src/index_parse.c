#include "index_parse.h"

static int parse_int64(const char *str, size_t len, size_t *pos, int64_t *out) {
  if (*pos >= len) return 0;
  int negative = 0;
  if (str[*pos] == '-') {
    negative = 1;
    (*pos)++;
  } else if (str[*pos] == '+') {
    (*pos)++;
  }
  if (*pos >= len || str[*pos] < '0' || str[*pos] > '9') return 0;
  int64_t val = 0;
  while (*pos < len && str[*pos] >= '0' && str[*pos] <= '9') {
    val = val * 10 + (str[*pos] - '0');
    (*pos)++;
  }
  *out = negative ? -val : val;
  return 1;
}

FeatherResult feather_parse_index(const FeatherHostOps *ops, FeatherInterp interp,
                                  FeatherObj indexObj, size_t listLen, int64_t *out) {
  size_t len;
  const char *str = ops->string.get(interp, indexObj, &len);
  size_t pos = 0;
  int64_t base;
  int is_end = 0;

  if (len >= 3 && str[0] == 'e' && str[1] == 'n' && str[2] == 'd') {
    base = (int64_t)listLen - 1;
    pos = 3;
    is_end = 1;
  } else {
    if (!parse_int64(str, len, &pos, &base)) {
      goto bad_index;
    }
  }

  while (pos < len) {
    if (str[pos] == '+' || str[pos] == '-') {
      char op = str[pos];
      pos++;
      int64_t offset;
      if (!parse_int64(str, len, &pos, &offset)) {
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
