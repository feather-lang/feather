#include "tclc.h"
#include "internal.h"

// Builtin command table entry
typedef struct {
  const char *name;
  TclBuiltinCmd cmd;
} BuiltinEntry;

// Table of all builtin commands
static const BuiltinEntry builtins[] = {
    {"set", tcl_builtin_set},
    {NULL, NULL} // sentinel
};

// Look up a builtin command by name
TclBuiltinCmd tcl_lookup_builtin(const char *name, size_t len) {
  for (const BuiltinEntry *entry = builtins; entry->name != NULL; entry++) {
    // Compare strings (need to check length since name might not be
    // null-terminated)
    const char *a = entry->name;
    const char *b = name;
    size_t i = 0;
    while (i < len && *a && *a == *b) {
      a++;
      b++;
      i++;
    }
    if (i == len && *a == '\0') {
      return entry->cmd;
    }
  }
  return NULL;
}

void tcl_interp_init(const TclHostOps *ops, TclInterp interp) {
  // Currently nothing to initialize - builtins are looked up statically
  (void)ops;
  (void)interp;
}
