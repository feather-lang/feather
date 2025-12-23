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
    {"expr", tcl_builtin_expr},
    {"proc", tcl_builtin_proc},
    {"if", tcl_builtin_if},
    {"while", tcl_builtin_while},
    {"break", tcl_builtin_break},
    {"continue", tcl_builtin_continue},
    {"incr", tcl_builtin_incr},
    {"llength", tcl_builtin_llength},
    {"lindex", tcl_builtin_lindex},
    {"run", tcl_builtin_run},
    {"return", tcl_builtin_return},
    {"tcl::mathfunc::exp", tcl_builtin_mathfunc_exp},
    {"error", tcl_builtin_error},
    {"catch", tcl_builtin_catch},
    {"info", tcl_builtin_info},
    {"upvar", tcl_builtin_upvar},
    {"uplevel", tcl_builtin_uplevel},
    {"rename", tcl_builtin_rename},
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
  // Register all builtin commands with the host, including their function pointers
  for (const BuiltinEntry *entry = builtins; entry->name != NULL; entry++) {
    TclObj name = ops->string.intern(interp, entry->name, tcl_strlen(entry->name));
    ops->proc.register_builtin(interp, name, entry->cmd);
  }
}
