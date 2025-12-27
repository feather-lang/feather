#include "feather.h"
#include "_cgo_export.h"

// C wrapper functions that can be used as function pointers in FeatherHostOps
// These call through to the Go functions exported via //export

static FeatherResult c_bind_unknown(FeatherInterp interp, FeatherObj cmd, FeatherObj args, FeatherObj *value) {
    return goBindUnknown(interp, cmd, args, value);
}

static FeatherObj c_string_intern(FeatherInterp interp, const char *s, size_t len) {
    return goStringIntern(interp, (char*)s, len);
}

static const char* c_string_get(FeatherInterp interp, FeatherObj obj, size_t *len) {
    return goStringGet(interp, obj, len);
}

static FeatherObj c_string_concat(FeatherInterp interp, FeatherObj a, FeatherObj b) {
    return goStringConcat(interp, a, b);
}

static int c_string_compare(FeatherInterp interp, FeatherObj a, FeatherObj b) {
    return goStringCompare(interp, a, b);
}

static FeatherResult c_string_regex_match(FeatherInterp interp, FeatherObj pattern, FeatherObj string, int *result) {
    return goStringRegexMatch(interp, pattern, string, result);
}

// Rune operations (Unicode-aware)
static size_t c_rune_length(FeatherInterp interp, FeatherObj str) {
    return goRuneLength(interp, str);
}

static FeatherObj c_rune_at(FeatherInterp interp, FeatherObj str, size_t index) {
    return goRuneAt(interp, str, index);
}

static FeatherObj c_rune_range(FeatherInterp interp, FeatherObj str, int64_t first, int64_t last) {
    return goRuneRange(interp, str, first, last);
}

static FeatherObj c_rune_to_upper(FeatherInterp interp, FeatherObj str) {
    return goRuneToUpper(interp, str);
}

static FeatherObj c_rune_to_lower(FeatherInterp interp, FeatherObj str) {
    return goRuneToLower(interp, str);
}

static FeatherObj c_rune_fold(FeatherInterp interp, FeatherObj str) {
    return goRuneFold(interp, str);
}

static FeatherResult c_interp_set_result(FeatherInterp interp, FeatherObj result) {
    return goInterpSetResult(interp, result);
}

static FeatherObj c_interp_get_result(FeatherInterp interp) {
    return goInterpGetResult(interp);
}

static FeatherResult c_interp_reset_result(FeatherInterp interp, FeatherObj result) {
    return goInterpResetResult(interp, result);
}

static FeatherResult c_interp_set_return_options(FeatherInterp interp, FeatherObj options) {
    return goInterpSetReturnOptions(interp, options);
}

static FeatherObj c_interp_get_return_options(FeatherInterp interp, FeatherResult code) {
    return goInterpGetReturnOptions(interp, code);
}

static FeatherObj c_interp_get_script(FeatherInterp interp) {
    return goInterpGetScript(interp);
}

static void c_interp_set_script(FeatherInterp interp, FeatherObj path) {
    goInterpSetScript(interp, path);
}

static FeatherObj c_list_create(FeatherInterp interp) {
    return goListCreate(interp);
}

static int c_list_is_nil(FeatherInterp interp, FeatherObj obj) {
    return goListIsNil(interp, obj);
}

static FeatherObj c_list_from(FeatherInterp interp, FeatherObj obj) {
    return goListFrom(interp, obj);
}

static FeatherObj c_list_push(FeatherInterp interp, FeatherObj list, FeatherObj item) {
    return goListPush(interp, list, item);
}

static FeatherObj c_list_pop(FeatherInterp interp, FeatherObj list) {
    return goListPop(interp, list);
}

static FeatherObj c_list_unshift(FeatherInterp interp, FeatherObj list, FeatherObj item) {
    return goListUnshift(interp, list, item);
}

static FeatherObj c_list_shift(FeatherInterp interp, FeatherObj list) {
    return goListShift(interp, list);
}

static size_t c_list_length(FeatherInterp interp, FeatherObj list) {
    return goListLength(interp, list);
}

static FeatherObj c_list_at(FeatherInterp interp, FeatherObj list, size_t index) {
    return goListAt(interp, list, index);
}

static FeatherObj c_list_slice(FeatherInterp interp, FeatherObj list, size_t first, size_t last) {
    return goListSlice(interp, list, first, last);
}

static FeatherResult c_list_set_at(FeatherInterp interp, FeatherObj list, size_t index, FeatherObj value) {
    return goListSetAt(interp, list, index, value);
}

static FeatherObj c_list_splice(FeatherInterp interp, FeatherObj list, size_t first,
                            size_t deleteCount, FeatherObj insertions) {
    return goListSplice(interp, list, first, deleteCount, insertions);
}

static FeatherResult c_list_sort(FeatherInterp interp, FeatherObj list,
                             int (*cmp)(FeatherInterp interp, FeatherObj a, FeatherObj b, void *ctx),
                             void *ctx) {
    return goListSort(interp, list, (void*)cmp, ctx);
}

// Dict operations
static FeatherObj c_dict_create(FeatherInterp interp) {
    return goDictCreate(interp);
}

static int c_dict_is_dict(FeatherInterp interp, FeatherObj obj) {
    return goDictIsDict(interp, obj);
}

static FeatherObj c_dict_from(FeatherInterp interp, FeatherObj obj) {
    return goDictFrom(interp, obj);
}

static FeatherObj c_dict_get(FeatherInterp interp, FeatherObj dict, FeatherObj key) {
    return goDictGet(interp, dict, key);
}

static FeatherObj c_dict_set(FeatherInterp interp, FeatherObj dict, FeatherObj key, FeatherObj value) {
    return goDictSet(interp, dict, key, value);
}

static int c_dict_exists(FeatherInterp interp, FeatherObj dict, FeatherObj key) {
    return goDictExists(interp, dict, key);
}

static FeatherObj c_dict_remove(FeatherInterp interp, FeatherObj dict, FeatherObj key) {
    return goDictRemove(interp, dict, key);
}

static size_t c_dict_size(FeatherInterp interp, FeatherObj dict) {
    return goDictSize(interp, dict);
}

static FeatherObj c_dict_keys(FeatherInterp interp, FeatherObj dict) {
    return goDictKeys(interp, dict);
}

static FeatherObj c_dict_values(FeatherInterp interp, FeatherObj dict) {
    return goDictValues(interp, dict);
}

static FeatherObj c_int_create(FeatherInterp interp, int64_t val) {
    return goIntCreate(interp, val);
}

static FeatherResult c_int_get(FeatherInterp interp, FeatherObj obj, int64_t *out) {
    return goIntGet(interp, obj, out);
}

static FeatherObj c_dbl_create(FeatherInterp interp, double val) {
    return goDoubleCreate(interp, val);
}

static FeatherResult c_dbl_get(FeatherInterp interp, FeatherObj obj, double *out) {
    return goDoubleGet(interp, obj, out);
}

static FeatherDoubleClass c_dbl_classify(double val) {
    return goDoubleClassify(val);
}

static FeatherObj c_dbl_format(FeatherInterp interp, double val, char specifier, int precision) {
    return goDoubleFormat(interp, val, specifier, precision);
}

static FeatherResult c_dbl_math(FeatherInterp interp, FeatherMathOp op, double a, double b, double *out) {
    return goDoubleMath(interp, op, a, b, out);
}

static FeatherResult c_frame_push(FeatherInterp interp, FeatherObj cmd, FeatherObj args) {
    return goFramePush(interp, cmd, args);
}

static FeatherResult c_frame_pop(FeatherInterp interp) {
    return goFramePop(interp);
}

static size_t c_frame_level(FeatherInterp interp) {
    return goFrameLevel(interp);
}

static FeatherResult c_frame_set_active(FeatherInterp interp, size_t level) {
    return goFrameSetActive(interp, level);
}

static size_t c_frame_size(FeatherInterp interp) {
    return goFrameSize(interp);
}

static FeatherResult c_frame_info(FeatherInterp interp, size_t level, FeatherObj *cmd, FeatherObj *args, FeatherObj *ns) {
    return goFrameInfo(interp, level, cmd, args, ns);
}

static FeatherObj c_var_get(FeatherInterp interp, FeatherObj name) {
    return goVarGet(interp, name);
}

static void c_var_set(FeatherInterp interp, FeatherObj name, FeatherObj value) {
    goVarSet(interp, name, value);
}

static void c_var_unset(FeatherInterp interp, FeatherObj name) {
    goVarUnset(interp, name);
}

static FeatherResult c_var_exists(FeatherInterp interp, FeatherObj name) {
    return goVarExists(interp, name);
}

static void c_var_link(FeatherInterp interp, FeatherObj local, size_t target_level, FeatherObj target) {
    goVarLink(interp, local, target_level, target);
}

static void c_proc_define(FeatherInterp interp, FeatherObj name, FeatherObj params, FeatherObj body) {
    goProcDefine(interp, name, params, body);
}

static int c_proc_exists(FeatherInterp interp, FeatherObj name) {
    return goProcExists(interp, name);
}

static FeatherResult c_proc_params(FeatherInterp interp, FeatherObj name, FeatherObj *result) {
    return goProcParams(interp, name, result);
}

static FeatherResult c_proc_body(FeatherInterp interp, FeatherObj name, FeatherObj *result) {
    return goProcBody(interp, name, result);
}

static FeatherObj c_proc_names(FeatherInterp interp, FeatherObj namespace) {
    return goProcNames(interp, namespace);
}

static FeatherResult c_proc_resolve_namespace(FeatherInterp interp, FeatherObj path, FeatherObj *result) {
    return goProcResolveNamespace(interp, path, result);
}

static void c_proc_register_builtin(FeatherInterp interp, FeatherObj name, FeatherBuiltinCmd fn) {
    goProcRegisterBuiltin(interp, name, fn);
}

static FeatherCommandType c_proc_lookup(FeatherInterp interp, FeatherObj name, FeatherBuiltinCmd *fn) {
    return goProcLookup(interp, name, fn);
}

static FeatherResult c_proc_rename(FeatherInterp interp, FeatherObj oldName, FeatherObj newName) {
    return goProcRename(interp, oldName, newName);
}

// Namespace operations
static FeatherResult c_ns_create(FeatherInterp interp, FeatherObj path) {
    return goNsCreate(interp, path);
}

static FeatherResult c_ns_delete(FeatherInterp interp, FeatherObj path) {
    return goNsDelete(interp, path);
}

static int c_ns_exists(FeatherInterp interp, FeatherObj path) {
    return goNsExists(interp, path);
}

static FeatherObj c_ns_current(FeatherInterp interp) {
    return goNsCurrent(interp);
}

static FeatherResult c_ns_parent(FeatherInterp interp, FeatherObj ns, FeatherObj *result) {
    return goNsParent(interp, ns, result);
}

static FeatherObj c_ns_children(FeatherInterp interp, FeatherObj ns) {
    return goNsChildren(interp, ns);
}

static FeatherObj c_ns_get_var(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsGetVar(interp, ns, name);
}

static void c_ns_set_var(FeatherInterp interp, FeatherObj ns, FeatherObj name, FeatherObj value) {
    goNsSetVar(interp, ns, name, value);
}

static int c_ns_var_exists(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsVarExists(interp, ns, name);
}

static void c_ns_unset_var(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    goNsUnsetVar(interp, ns, name);
}

static FeatherCommandType c_ns_get_command(FeatherInterp interp, FeatherObj ns, FeatherObj name, FeatherBuiltinCmd *fn) {
    return goNsGetCommand(interp, ns, name, fn);
}

static void c_ns_set_command(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                             FeatherCommandType kind, FeatherBuiltinCmd fn,
                             FeatherObj params, FeatherObj body) {
    goNsSetCommand(interp, ns, name, kind, fn, params, body);
}

static FeatherResult c_ns_delete_command(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsDeleteCommand(interp, ns, name);
}

static FeatherObj c_ns_list_commands(FeatherInterp interp, FeatherObj ns) {
    return goNsListCommands(interp, ns);
}

static FeatherObj c_ns_get_exports(FeatherInterp interp, FeatherObj ns) {
    return goNsGetExports(interp, ns);
}

static void c_ns_set_exports(FeatherInterp interp, FeatherObj ns, FeatherObj patterns, int clear) {
    goNsSetExports(interp, ns, patterns, clear);
}

static int c_ns_is_exported(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsIsExported(interp, ns, name);
}

static FeatherResult c_ns_copy_command(FeatherInterp interp, FeatherObj srcNs, FeatherObj srcName,
                                   FeatherObj dstNs, FeatherObj dstName) {
    return goNsCopyCommand(interp, srcNs, srcName, dstNs, dstName);
}

// Frame namespace extensions
static FeatherResult c_frame_set_namespace(FeatherInterp interp, FeatherObj ns) {
    return goFrameSetNamespace(interp, ns);
}

static FeatherObj c_frame_get_namespace(FeatherInterp interp) {
    return goFrameGetNamespace(interp);
}

// Var namespace link
static void c_var_link_ns(FeatherInterp interp, FeatherObj local, FeatherObj ns, FeatherObj name) {
    goVarLinkNs(interp, local, ns, name);
}

static FeatherObj c_var_names(FeatherInterp interp, FeatherObj ns) {
    return goVarNames(interp, ns);
}

// Trace operations
static FeatherResult c_trace_add(FeatherInterp interp, FeatherObj kind, FeatherObj name, FeatherObj ops, FeatherObj script) {
    return goTraceAdd(interp, kind, name, ops, script);
}

static FeatherResult c_trace_remove(FeatherInterp interp, FeatherObj kind, FeatherObj name, FeatherObj ops, FeatherObj script) {
    return goTraceRemove(interp, kind, name, ops, script);
}

static FeatherObj c_trace_info(FeatherInterp interp, FeatherObj kind, FeatherObj name) {
    return goTraceInfo(interp, kind, name);
}

// Foreign object operations
static int c_foreign_is_foreign(FeatherInterp interp, FeatherObj obj) {
    return goForeignIsForeign(interp, obj);
}

static FeatherObj c_foreign_type_name(FeatherInterp interp, FeatherObj obj) {
    return goForeignTypeName(interp, obj);
}

static FeatherObj c_foreign_string_rep(FeatherInterp interp, FeatherObj obj) {
    return goForeignStringRep(interp, obj);
}

static FeatherObj c_foreign_methods(FeatherInterp interp, FeatherObj obj) {
    return goForeignMethods(interp, obj);
}

static FeatherResult c_foreign_invoke(FeatherInterp interp, FeatherObj obj, FeatherObj method, FeatherObj args) {
    return goForeignInvoke(interp, obj, method, args);
}

static void c_foreign_destroy(FeatherInterp interp, FeatherObj obj) {
    goForeignDestroy(interp, obj);
}

// Build the FeatherHostOps struct with all callbacks
FeatherHostOps make_host_ops(void) {
    FeatherHostOps ops;

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
    ops.list.slice = c_list_slice;
    ops.list.set_at = c_list_set_at;
    ops.list.splice = c_list_splice;
    ops.list.sort = c_list_sort;

    ops.dict.create = c_dict_create;
    ops.dict.is_dict = c_dict_is_dict;
    ops.dict.from = c_dict_from;
    ops.dict.get = c_dict_get;
    ops.dict.set = c_dict_set;
    ops.dict.exists = c_dict_exists;
    ops.dict.remove = c_dict_remove;
    ops.dict.size = c_dict_size;
    ops.dict.keys = c_dict_keys;
    ops.dict.values = c_dict_values;

    ops.integer.create = c_int_create;
    ops.integer.get = c_int_get;

    ops.dbl.create = c_dbl_create;
    ops.dbl.get = c_dbl_get;
    ops.dbl.classify = c_dbl_classify;
    ops.dbl.format = c_dbl_format;
    ops.dbl.math = c_dbl_math;

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

    ops.foreign.is_foreign = c_foreign_is_foreign;
    ops.foreign.type_name = c_foreign_type_name;
    ops.foreign.string_rep = c_foreign_string_rep;
    ops.foreign.methods = c_foreign_methods;
    ops.foreign.invoke = c_foreign_invoke;
    ops.foreign.destroy = c_foreign_destroy;

    return ops;
}

// Call the C interpreter with host ops
FeatherResult call_feather_eval_obj(FeatherInterp interp, FeatherObj script, FeatherEvalFlags flags) {
    FeatherHostOps ops = make_host_ops();
    // Evaluate the script object
    return feather_script_eval_obj(&ops, interp, script, flags);
}

// Call the C parser with host ops
FeatherParseStatus call_feather_parse(FeatherInterp interp, FeatherObj script) {
    FeatherHostOps ops = make_host_ops();
    size_t len;
    const char *str = ops.string.get(interp, script, &len);
    FeatherParseContext ctx;
    feather_parse_init(&ctx, str, len);
    FeatherParseStatus status = feather_parse_command(&ops, interp, &ctx);
    // Convert TCL_PARSE_DONE to TCL_PARSE_OK for backwards compatibility
    // (empty script should return OK with empty result)
    if (status == TCL_PARSE_DONE) {
        ops.interp.set_result(interp, ops.list.create(interp));
        return TCL_PARSE_OK;
    }
    return status;
}

// Initialize the C interpreter with host ops
void call_feather_interp_init(FeatherInterp interp) {
    FeatherHostOps ops = make_host_ops();
    feather_interp_init(&ops, interp);
}

// Call C's feather_list_parse to parse a string as a list
FeatherObj call_feather_list_parse(FeatherInterp interp, const char *s, size_t len) {
    FeatherHostOps ops = make_host_ops();
    return feather_list_parse(&ops, interp, s, len);
}
