#include "namespace_util.h"

FeatherObj feather_get_display_name(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj name) {
  size_t len = ops->string.byte_length(interp, name);

  // Check if starts with "::"
  if (len > 2 &&
      ops->string.byte_at(interp, name, 0) == ':' &&
      ops->string.byte_at(interp, name, 1) == ':') {
    // Check for another :: after the prefix
    for (size_t i = 2; i + 1 < len; i++) {
      if (ops->string.byte_at(interp, name, i) == ':' &&
          ops->string.byte_at(interp, name, i + 1) == ':') {
        return name;  // Has more qualifiers, return as-is
      }
    }
    // Just "::foo" pattern - return without leading ::
    return ops->string.slice(interp, name, 2, len);
  }
  return name;
}
