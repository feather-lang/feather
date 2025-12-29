#include "feather.h"
#include "host.h"
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
    {"::lmap", feather_builtin_lmap},
    {"::lassign", feather_builtin_lassign},
    {"::linsert", feather_builtin_linsert},
    {"::switch", feather_builtin_switch},
    {"::tailcall", feather_builtin_tailcall},
    {"::break", feather_builtin_break},
    {"::continue", feather_builtin_continue},
    {"::incr", feather_builtin_incr},
    {"::llength", feather_builtin_llength},
    {"::lindex", feather_builtin_lindex},
    {"::return", feather_builtin_return},
    {"::tcl::mathfunc::sqrt", feather_builtin_mathfunc_sqrt},
    {"::tcl::mathfunc::exp", feather_builtin_mathfunc_exp},
    {"::tcl::mathfunc::log", feather_builtin_mathfunc_log},
    {"::tcl::mathfunc::log10", feather_builtin_mathfunc_log10},
    {"::tcl::mathfunc::sin", feather_builtin_mathfunc_sin},
    {"::tcl::mathfunc::cos", feather_builtin_mathfunc_cos},
    {"::tcl::mathfunc::tan", feather_builtin_mathfunc_tan},
    {"::tcl::mathfunc::asin", feather_builtin_mathfunc_asin},
    {"::tcl::mathfunc::acos", feather_builtin_mathfunc_acos},
    {"::tcl::mathfunc::atan", feather_builtin_mathfunc_atan},
    {"::tcl::mathfunc::sinh", feather_builtin_mathfunc_sinh},
    {"::tcl::mathfunc::cosh", feather_builtin_mathfunc_cosh},
    {"::tcl::mathfunc::tanh", feather_builtin_mathfunc_tanh},
    {"::tcl::mathfunc::floor", feather_builtin_mathfunc_floor},
    {"::tcl::mathfunc::ceil", feather_builtin_mathfunc_ceil},
    {"::tcl::mathfunc::round", feather_builtin_mathfunc_round},
    {"::tcl::mathfunc::abs", feather_builtin_mathfunc_abs},
    {"::tcl::mathfunc::pow", feather_builtin_mathfunc_pow},
    {"::tcl::mathfunc::atan2", feather_builtin_mathfunc_atan2},
    {"::tcl::mathfunc::fmod", feather_builtin_mathfunc_fmod},
    {"::tcl::mathfunc::hypot", feather_builtin_mathfunc_hypot},
    {"::tcl::mathfunc::double", feather_builtin_mathfunc_double},
    {"::tcl::mathfunc::int", feather_builtin_mathfunc_int},
    {"::tcl::mathfunc::wide", feather_builtin_mathfunc_wide},
    {"::tcl::mathfunc::isnan", feather_builtin_mathfunc_isnan},
    {"::tcl::mathfunc::isinf", feather_builtin_mathfunc_isinf},
    {"::error", feather_builtin_error},
    {"::catch", feather_builtin_catch},
    {"::info", feather_builtin_info},
    {"::upvar", feather_builtin_upvar},
    {"::uplevel", feather_builtin_uplevel},
    {"::rename", feather_builtin_rename},
    {"::namespace", feather_builtin_namespace},
    {"::variable", feather_builtin_variable},
    {"::global", feather_builtin_global},
    {"::apply", feather_builtin_apply},
    {"::throw", feather_builtin_throw},
    {"::try", feather_builtin_try},
    {"::trace", feather_builtin_trace},
    // M15: List and string operations
    {"::list", feather_builtin_list},
    {"::lrange", feather_builtin_lrange},
    {"::lappend", feather_builtin_lappend},
    {"::lset", feather_builtin_lset},
    {"::lreplace", feather_builtin_lreplace},
    {"::lreverse", feather_builtin_lreverse},
    {"::lrepeat", feather_builtin_lrepeat},
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
    {"::scan", feather_builtin_scan},
    {"::subst", feather_builtin_subst},
    {"::eval", feather_builtin_eval},
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
  ops = feather_get_ops(ops);
  // Register all builtin commands in their respective namespaces
  for (const BuiltinEntry *entry = builtins; entry->name != NULL; entry++) {
    FeatherObj fullName = ops->string.intern(interp, entry->name, feather_strlen(entry->name));

    // Split the qualified name into namespace and simple name
    FeatherObj ns, simpleName;
    feather_obj_split_command(ops, interp, fullName, &ns, &simpleName);

    // If no namespace part (shouldn't happen for our table), use global
    if (ops->list.is_nil(interp, ns)) {
      ns = ops->string.intern(interp, "::", 2);
    }

    // Create namespace if needed (for ::tcl::mathfunc)
    ops->ns.create(interp, ns);

    // Store command in namespace
    ops->ns.set_command(interp, ns, simpleName, TCL_CMD_BUILTIN, entry->cmd, 0, 0);
  }

  // Create ::tcl::trace namespace and initialize trace storage dicts
  FeatherObj traceNs = ops->string.intern(interp, "::tcl::trace", 12);
  ops->ns.create(interp, traceNs);

  FeatherObj emptyDict = ops->dict.create(interp);
  FeatherObj varName = ops->string.intern(interp, "variable", 8);
  FeatherObj cmdName = ops->string.intern(interp, "command", 7);
  FeatherObj execName = ops->string.intern(interp, "execution", 9);

  ops->ns.set_var(interp, traceNs, varName, emptyDict);
  ops->ns.set_var(interp, traceNs, cmdName, emptyDict);
  ops->ns.set_var(interp, traceNs, execName, emptyDict);
}

FeatherObj feather_trace_get_dict(const FeatherHostOps *ops, FeatherInterp interp,
                                  const char *kind) {
  ops = feather_get_ops(ops);
  FeatherObj traceNs = ops->string.intern(interp, "::tcl::trace", 12);
  FeatherObj kindName = ops->string.intern(interp, kind, feather_strlen(kind));
  return ops->ns.get_var(interp, traceNs, kindName);
}

void feather_trace_set_dict(const FeatherHostOps *ops, FeatherInterp interp,
                            const char *kind, FeatherObj dict) {
  ops = feather_get_ops(ops);
  FeatherObj traceNs = ops->string.intern(interp, "::tcl::trace", 12);
  FeatherObj kindName = ops->string.intern(interp, kind, feather_strlen(kind));
  ops->ns.set_var(interp, traceNs, kindName, dict);
}
