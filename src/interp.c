#include "feather.h"
#include "internal.h"

// Builtin command table entry
typedef struct {
  const char *name;
  FeatherBuiltinCmd cmd;
} BuiltinEntry;

// Table of all builtin commands
// All builtins are registered in the global namespace (::)
static const BuiltinEntry builtins[] = {
    {"::set", feather_builtin_set},
    {"::expr", feather_builtin_expr},
    {"::proc", feather_builtin_proc},
    {"::if", feather_builtin_if},
    {"::while", feather_builtin_while},
    {"::for", feather_builtin_for},
    {"::foreach", feather_builtin_foreach},
    {"::switch", feather_builtin_switch},
    {"::tailcall", feather_builtin_tailcall},
    {"::break", feather_builtin_break},
    {"::continue", feather_builtin_continue},
    {"::incr", feather_builtin_incr},
    {"::llength", feather_builtin_llength},
    {"::lindex", feather_builtin_lindex},
    {"::return", feather_builtin_return},
    {"::tcl::mathfunc::exp", feather_builtin_mathfunc_exp},
    {"::error", feather_builtin_error},
    {"::catch", feather_builtin_catch},
    {"::info", feather_builtin_info},
    {"::upvar", feather_builtin_upvar},
    {"::uplevel", feather_builtin_uplevel},
    {"::rename", feather_builtin_rename},
    {"::namespace", feather_builtin_namespace},
    {"::variable", feather_builtin_variable},
    {"::throw", feather_builtin_throw},
    {"::try", feather_builtin_try},
    {"::trace", feather_builtin_trace},
    // M15: List and string operations
    {"::list", feather_builtin_list},
    {"::lrange", feather_builtin_lrange},
    {"::lappend", feather_builtin_lappend},
    {"::lset", feather_builtin_lset},
    {"::lreplace", feather_builtin_lreplace},
    {"::lsort", feather_builtin_lsort},
    {"::lsearch", feather_builtin_lsearch},
    {"::string", feather_builtin_string},
    {"::split", feather_builtin_split},
    {"::join", feather_builtin_join},
    {"::concat", feather_builtin_concat},
    {"::append", feather_builtin_append},
    {"::unset", feather_builtin_unset},
    // M16: Dictionary support
    {"::dict", feather_builtin_dict},
    // String formatting
    {"::format", feather_builtin_format},
    {NULL, NULL} // sentinel
};

// Look up a builtin command by name
FeatherBuiltinCmd feather_lookup_builtin(const char *name, size_t len) {
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

void feather_interp_init(const FeatherHostOps *ops, FeatherInterp interp) {
  // Register all builtin commands in their respective namespaces
  for (const BuiltinEntry *entry = builtins; entry->name != NULL; entry++) {
    FeatherObj fullName = ops->string.intern(interp, entry->name, feather_strlen(entry->name));

    // Split the qualified name into namespace and simple name
    FeatherObj ns, simpleName;
    feather_split_command(ops, interp, fullName, &ns, &simpleName);

    // If no namespace part (shouldn't happen for our table), use global
    if (ops->list.is_nil(interp, ns)) {
      ns = ops->string.intern(interp, "::", 2);
    }

    // Create namespace if needed (for ::tcl::mathfunc)
    ops->ns.create(interp, ns);

    // Store command in namespace
    ops->ns.set_command(interp, ns, simpleName, TCL_CMD_BUILTIN, entry->cmd, 0, 0);
  }
}
