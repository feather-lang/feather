#include "namespace_util.h"

FeatherObj feather_get_display_name(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj name) {
  size_t len;
  const char *str = ops->string.get(interp, name, &len);

  if (len > 2 && str[0] == ':' && str[1] == ':') {
    for (size_t i = 2; i + 1 < len; i++) {
      if (str[i] == ':' && str[i + 1] == ':') {
        return name;
      }
    }
    return ops->string.intern(interp, str + 2, len - 2);
  }
  return name;
}
