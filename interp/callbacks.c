#include "tclc.h"
#include "_cgo_export.h"

// C wrapper functions that can be used as function pointers in TclHostOps
// These call through to the Go functions exported via //export

static TclResult c_bind_unknown(TclInterp interp, TclObj cmd, TclObj args, TclObj *value) {
    return goBindUnknown(interp, cmd, args, value);
}

static TclObj c_string_intern(TclInterp interp, const char *s, size_t len) {
    return goStringIntern(interp, (char*)s, len);
}

static const char* c_string_get(TclInterp interp, TclObj obj, size_t *len) {
    return goStringGet(interp, obj, len);
}

static TclObj c_string_concat(TclInterp interp, TclObj a, TclObj b) {
    return goStringConcat(interp, a, b);
}

static int c_string_compare(TclInterp interp, TclObj a, TclObj b) {
    return goStringCompare(interp, a, b);
}

static TclResult c_string_regex_match(TclInterp interp, TclObj pattern, TclObj string, int *result) {
    return goStringRegexMatch(interp, pattern, string, result);
}

// Rune operations (Unicode-aware)
static size_t c_rune_length(TclInterp interp, TclObj str) {
    return goRuneLength(interp, str);
}

static TclObj c_rune_at(TclInterp interp, TclObj str, size_t index) {
    return goRuneAt(interp, str, index);
}

static TclObj c_rune_range(TclInterp interp, TclObj str, int64_t first, int64_t last) {
    return goRuneRange(interp, str, first, last);
}

static TclObj c_rune_to_upper(TclInterp interp, TclObj str) {
    return goRuneToUpper(interp, str);
}

static TclObj c_rune_to_lower(TclInterp interp, TclObj str) {
    return goRuneToLower(interp, str);
}

static TclObj c_rune_fold(TclInterp interp, TclObj str) {
    return goRuneFold(interp, str);
}

static TclResult c_interp_set_result(TclInterp interp, TclObj result) {
    return goInterpSetResult(interp, result);
}

static TclObj c_interp_get_result(TclInterp interp) {
    return goInterpGetResult(interp);
}

static TclResult c_interp_reset_result(TclInterp interp, TclObj result) {
    return goInterpResetResult(interp, result);
}

static TclResult c_interp_set_return_options(TclInterp interp, TclObj options) {
    return goInterpSetReturnOptions(interp, options);
}

static TclObj c_interp_get_return_options(TclInterp interp, TclResult code) {
    return goInterpGetReturnOptions(interp, code);
}

static TclObj c_interp_get_script(TclInterp interp) {
    return goInterpGetScript(interp);
}

static void c_interp_set_script(TclInterp interp, TclObj path) {
    goInterpSetScript(interp, path);
}

static TclObj c_list_create(TclInterp interp) {
    return goListCreate(interp);
}

static int c_list_is_nil(TclInterp interp, TclObj obj) {
    return goListIsNil(interp, obj);
}

static TclObj c_list_from(TclInterp interp, TclObj obj) {
    return goListFrom(interp, obj);
}

static TclObj c_list_push(TclInterp interp, TclObj list, TclObj item) {
    return goListPush(interp, list, item);
}

static TclObj c_list_pop(TclInterp interp, TclObj list) {
    return goListPop(interp, list);
}

static TclObj c_list_unshift(TclInterp interp, TclObj list, TclObj item) {
    return goListUnshift(interp, list, item);
}

static TclObj c_list_shift(TclInterp interp, TclObj list) {
    return goListShift(interp, list);
}

static size_t c_list_length(TclInterp interp, TclObj list) {
    return goListLength(interp, list);
}

static TclObj c_list_at(TclInterp interp, TclObj list, size_t index) {
    return goListAt(interp, list, index);
}

static TclObj c_int_create(TclInterp interp, int64_t val) {
    return goIntCreate(interp, val);
}

static TclResult c_int_get(TclInterp interp, TclObj obj, int64_t *out) {
    return goIntGet(interp, obj, out);
}

static TclObj c_dbl_create(TclInterp interp, double val) {
    return goDoubleCreate(interp, val);
}

static TclResult c_dbl_get(TclInterp interp, TclObj obj, double *out) {
    return goDoubleGet(interp, obj, out);
}

static TclResult c_frame_push(TclInterp interp, TclObj cmd, TclObj args) {
    return goFramePush(interp, cmd, args);
}

static TclResult c_frame_pop(TclInterp interp) {
    return goFramePop(interp);
}

static size_t c_frame_level(TclInterp interp) {
    return goFrameLevel(interp);
}

static TclResult c_frame_set_active(TclInterp interp, size_t level) {
    return goFrameSetActive(interp, level);
}

static size_t c_frame_size(TclInterp interp) {
    return goFrameSize(interp);
}

static TclResult c_frame_info(TclInterp interp, size_t level, TclObj *cmd, TclObj *args, TclObj *ns) {
    return goFrameInfo(interp, level, cmd, args, ns);
}

static TclObj c_var_get(TclInterp interp, TclObj name) {
    return goVarGet(interp, name);
}

static void c_var_set(TclInterp interp, TclObj name, TclObj value) {
    goVarSet(interp, name, value);
}

static void c_var_unset(TclInterp interp, TclObj name) {
    goVarUnset(interp, name);
}

static TclResult c_var_exists(TclInterp interp, TclObj name) {
    return goVarExists(interp, name);
}

static void c_var_link(TclInterp interp, TclObj local, size_t target_level, TclObj target) {
    goVarLink(interp, local, target_level, target);
}

static void c_proc_define(TclInterp interp, TclObj name, TclObj params, TclObj body) {
    goProcDefine(interp, name, params, body);
}

static int c_proc_exists(TclInterp interp, TclObj name) {
    return goProcExists(interp, name);
}

static TclResult c_proc_params(TclInterp interp, TclObj name, TclObj *result) {
    return goProcParams(interp, name, result);
}

static TclResult c_proc_body(TclInterp interp, TclObj name, TclObj *result) {
    return goProcBody(interp, name, result);
}

static TclObj c_proc_names(TclInterp interp, TclObj namespace) {
    return goProcNames(interp, namespace);
}

static TclResult c_proc_resolve_namespace(TclInterp interp, TclObj path, TclObj *result) {
    return goProcResolveNamespace(interp, path, result);
}

static void c_proc_register_builtin(TclInterp interp, TclObj name, TclBuiltinCmd fn) {
    goProcRegisterBuiltin(interp, name, fn);
}

static TclCommandType c_proc_lookup(TclInterp interp, TclObj name, TclBuiltinCmd *fn) {
    return goProcLookup(interp, name, fn);
}

static TclResult c_proc_rename(TclInterp interp, TclObj oldName, TclObj newName) {
    return goProcRename(interp, oldName, newName);
}

// Namespace operations
static TclResult c_ns_create(TclInterp interp, TclObj path) {
    return goNsCreate(interp, path);
}

static TclResult c_ns_delete(TclInterp interp, TclObj path) {
    return goNsDelete(interp, path);
}

static int c_ns_exists(TclInterp interp, TclObj path) {
    return goNsExists(interp, path);
}

static TclObj c_ns_current(TclInterp interp) {
    return goNsCurrent(interp);
}

static TclResult c_ns_parent(TclInterp interp, TclObj ns, TclObj *result) {
    return goNsParent(interp, ns, result);
}

static TclObj c_ns_children(TclInterp interp, TclObj ns) {
    return goNsChildren(interp, ns);
}

static TclObj c_ns_get_var(TclInterp interp, TclObj ns, TclObj name) {
    return goNsGetVar(interp, ns, name);
}

static void c_ns_set_var(TclInterp interp, TclObj ns, TclObj name, TclObj value) {
    goNsSetVar(interp, ns, name, value);
}

static int c_ns_var_exists(TclInterp interp, TclObj ns, TclObj name) {
    return goNsVarExists(interp, ns, name);
}

static void c_ns_unset_var(TclInterp interp, TclObj ns, TclObj name) {
    goNsUnsetVar(interp, ns, name);
}

static TclCommandType c_ns_get_command(TclInterp interp, TclObj ns, TclObj name, TclBuiltinCmd *fn) {
    return goNsGetCommand(interp, ns, name, fn);
}

static void c_ns_set_command(TclInterp interp, TclObj ns, TclObj name,
                             TclCommandType kind, TclBuiltinCmd fn,
                             TclObj params, TclObj body) {
    goNsSetCommand(interp, ns, name, kind, fn, params, body);
}

static TclResult c_ns_delete_command(TclInterp interp, TclObj ns, TclObj name) {
    return goNsDeleteCommand(interp, ns, name);
}

static TclObj c_ns_list_commands(TclInterp interp, TclObj ns) {
    return goNsListCommands(interp, ns);
}

static TclObj c_ns_get_exports(TclInterp interp, TclObj ns) {
    return goNsGetExports(interp, ns);
}

static void c_ns_set_exports(TclInterp interp, TclObj ns, TclObj patterns, int clear) {
    goNsSetExports(interp, ns, patterns, clear);
}

static int c_ns_is_exported(TclInterp interp, TclObj ns, TclObj name) {
    return goNsIsExported(interp, ns, name);
}

static TclResult c_ns_copy_command(TclInterp interp, TclObj srcNs, TclObj srcName,
                                   TclObj dstNs, TclObj dstName) {
    return goNsCopyCommand(interp, srcNs, srcName, dstNs, dstName);
}

// Frame namespace extensions
static TclResult c_frame_set_namespace(TclInterp interp, TclObj ns) {
    return goFrameSetNamespace(interp, ns);
}

static TclObj c_frame_get_namespace(TclInterp interp) {
    return goFrameGetNamespace(interp);
}

// Var namespace link
static void c_var_link_ns(TclInterp interp, TclObj local, TclObj ns, TclObj name) {
    goVarLinkNs(interp, local, ns, name);
}

static TclObj c_var_names(TclInterp interp, TclObj ns) {
    return goVarNames(interp, ns);
}

// Trace operations
static TclResult c_trace_add(TclInterp interp, TclObj kind, TclObj name, TclObj ops, TclObj script) {
    return goTraceAdd(interp, kind, name, ops, script);
}

static TclResult c_trace_remove(TclInterp interp, TclObj kind, TclObj name, TclObj ops, TclObj script) {
    return goTraceRemove(interp, kind, name, ops, script);
}

static TclObj c_trace_info(TclInterp interp, TclObj kind, TclObj name) {
    return goTraceInfo(interp, kind, name);
}

// Build the TclHostOps struct with all callbacks
TclHostOps make_host_ops(void) {
    TclHostOps ops;

    ops.frame.push = c_frame_push;
    ops.frame.pop = c_frame_pop;
    ops.frame.level = c_frame_level;
    ops.frame.set_active = c_frame_set_active;
    ops.frame.size = c_frame_size;
    ops.frame.info = c_frame_info;
    ops.frame.set_namespace = c_frame_set_namespace;
    ops.frame.get_namespace = c_frame_get_namespace;

    ops.var.get = c_var_get;
    ops.var.set = c_var_set;
    ops.var.unset = c_var_unset;
    ops.var.exists = c_var_exists;
    ops.var.link = c_var_link;
    ops.var.link_ns = c_var_link_ns;
    ops.var.names = c_var_names;

    ops.proc.define = c_proc_define;
    ops.proc.exists = c_proc_exists;
    ops.proc.params = c_proc_params;
    ops.proc.body = c_proc_body;
    ops.proc.names = c_proc_names;
    ops.proc.resolve_namespace = c_proc_resolve_namespace;
    ops.proc.register_builtin = c_proc_register_builtin;
    ops.proc.lookup = c_proc_lookup;
    ops.proc.rename = c_proc_rename;

    ops.ns.create = c_ns_create;
    ops.ns.delete = c_ns_delete;
    ops.ns.exists = c_ns_exists;
    ops.ns.current = c_ns_current;
    ops.ns.parent = c_ns_parent;
    ops.ns.children = c_ns_children;
    ops.ns.get_var = c_ns_get_var;
    ops.ns.set_var = c_ns_set_var;
    ops.ns.var_exists = c_ns_var_exists;
    ops.ns.unset_var = c_ns_unset_var;
    ops.ns.get_command = c_ns_get_command;
    ops.ns.set_command = c_ns_set_command;
    ops.ns.delete_command = c_ns_delete_command;
    ops.ns.list_commands = c_ns_list_commands;
    ops.ns.get_exports = c_ns_get_exports;
    ops.ns.set_exports = c_ns_set_exports;
    ops.ns.is_exported = c_ns_is_exported;
    ops.ns.copy_command = c_ns_copy_command;

    ops.string.intern = c_string_intern;
    ops.string.get = c_string_get;
    ops.string.concat = c_string_concat;
    ops.string.compare = c_string_compare;
    ops.string.regex_match = c_string_regex_match;

    ops.rune.length = c_rune_length;
    ops.rune.at = c_rune_at;
    ops.rune.range = c_rune_range;
    ops.rune.to_upper = c_rune_to_upper;
    ops.rune.to_lower = c_rune_to_lower;
    ops.rune.fold = c_rune_fold;

    ops.list.is_nil = c_list_is_nil;
    ops.list.create = c_list_create;
    ops.list.from = c_list_from;
    ops.list.push = c_list_push;
    ops.list.pop = c_list_pop;
    ops.list.unshift = c_list_unshift;
    ops.list.shift = c_list_shift;
    ops.list.length = c_list_length;
    ops.list.at = c_list_at;

    ops.integer.create = c_int_create;
    ops.integer.get = c_int_get;

    ops.dbl.create = c_dbl_create;
    ops.dbl.get = c_dbl_get;

    ops.interp.set_result = c_interp_set_result;
    ops.interp.get_result = c_interp_get_result;
    ops.interp.reset_result = c_interp_reset_result;
    ops.interp.set_return_options = c_interp_set_return_options;
    ops.interp.get_return_options = c_interp_get_return_options;
    ops.interp.get_script = c_interp_get_script;
    ops.interp.set_script = c_interp_set_script;

    ops.bind.unknown = c_bind_unknown;

    ops.trace.add = c_trace_add;
    ops.trace.remove = c_trace_remove;
    ops.trace.info = c_trace_info;

    return ops;
}

// Call the C interpreter with host ops
TclResult call_tcl_eval_obj(TclInterp interp, TclObj script, TclEvalFlags flags) {
    TclHostOps ops = make_host_ops();
    // Evaluate the script object
    return tcl_script_eval_obj(&ops, interp, script, flags);
}

// Call the C parser with host ops
TclParseStatus call_tcl_parse(TclInterp interp, TclObj script) {
    TclHostOps ops = make_host_ops();
    size_t len;
    const char *str = ops.string.get(interp, script, &len);
    TclParseContext ctx;
    tcl_parse_init(&ctx, str, len);
    TclParseStatus status = tcl_parse_command(&ops, interp, &ctx);
    // Convert TCL_PARSE_DONE to TCL_PARSE_OK for backwards compatibility
    // (empty script should return OK with empty result)
    if (status == TCL_PARSE_DONE) {
        ops.interp.set_result(interp, ops.list.create(interp));
        return TCL_PARSE_OK;
    }
    return status;
}

// Initialize the C interpreter with host ops
void call_tcl_interp_init(TclInterp interp) {
    TclHostOps ops = make_host_ops();
    tcl_interp_init(&ops, interp);
}
