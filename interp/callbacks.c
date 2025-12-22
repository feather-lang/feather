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

// Build the TclHostOps struct with all callbacks
TclHostOps make_host_ops(void) {
    TclHostOps ops;

    ops.frame.push = c_frame_push;
    ops.frame.pop = c_frame_pop;
    ops.frame.level = c_frame_level;
    ops.frame.set_active = c_frame_set_active;

    ops.var.get = c_var_get;
    ops.var.set = c_var_set;
    ops.var.unset = c_var_unset;
    ops.var.exists = c_var_exists;
    ops.var.link = c_var_link;

    ops.proc.define = c_proc_define;
    ops.proc.exists = c_proc_exists;
    ops.proc.params = c_proc_params;
    ops.proc.body = c_proc_body;

    ops.string.intern = c_string_intern;
    ops.string.get = c_string_get;
    ops.string.concat = c_string_concat;
    ops.string.compare = c_string_compare;

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

    ops.bind.unknown = c_bind_unknown;

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
