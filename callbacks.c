#include "feather.h"
#include "_cgo_export.h"

// Implementation of feather_host_* functions for native (Go) builds.
// These call through to Go functions exported via //export.
// For WASM builds, these would be provided as WASM imports instead.

// ============================================================================
// Bind Operations
// ============================================================================

FeatherResult feather_host_bind_unknown(FeatherInterp interp, FeatherObj cmd, FeatherObj args, FeatherObj *value) {
    return goBindUnknown(interp, cmd, args, value);
}

// ============================================================================
// String Operations
// ============================================================================

FeatherObj feather_host_string_intern(FeatherInterp interp, const char *s, size_t len) {
    return goStringIntern(interp, (char*)s, len);
}

FeatherObj feather_host_string_concat(FeatherInterp interp, FeatherObj a, FeatherObj b) {
    return goStringConcat(interp, a, b);
}

int feather_host_string_compare(FeatherInterp interp, FeatherObj a, FeatherObj b) {
    return goStringCompare(interp, a, b);
}

FeatherResult feather_host_string_regex_match(FeatherInterp interp, FeatherObj pattern, FeatherObj string, int nocase, int *result, FeatherObj *matches, FeatherObj *indices) {
    return goStringRegexMatch(interp, pattern, string, nocase, result, matches, indices);
}

int feather_host_string_byte_at(FeatherInterp interp, FeatherObj str, size_t index) {
    return goStringByteAt(interp, str, index);
}

size_t feather_host_string_byte_length(FeatherInterp interp, FeatherObj str) {
    return goStringByteLength(interp, str);
}

FeatherObj feather_host_string_slice(FeatherInterp interp, FeatherObj str, size_t start, size_t end) {
    return goStringSlice(interp, str, start, end);
}

int feather_host_string_equal(FeatherInterp interp, FeatherObj a, FeatherObj b) {
    return goStringEqual(interp, a, b);
}

int feather_host_string_match(FeatherInterp interp, FeatherObj pattern, FeatherObj str, int nocase) {
    return goStringMatch(interp, pattern, str, nocase);
}

FeatherObj feather_host_string_builder_new(FeatherInterp interp, size_t capacity) {
    return goStringBuilderNew(interp, capacity);
}

void feather_host_string_builder_append_byte(FeatherInterp interp, FeatherObj builder, int byte) {
    goStringBuilderAppendByte(interp, builder, byte);
}

void feather_host_string_builder_append_obj(FeatherInterp interp, FeatherObj builder, FeatherObj str) {
    goStringBuilderAppendObj(interp, builder, str);
}

FeatherObj feather_host_string_builder_finish(FeatherInterp interp, FeatherObj builder) {
    return goStringBuilderFinish(interp, builder);
}

// ============================================================================
// Rune Operations
// ============================================================================

size_t feather_host_rune_length(FeatherInterp interp, FeatherObj str) {
    return goRuneLength(interp, str);
}

FeatherObj feather_host_rune_at(FeatherInterp interp, FeatherObj str, size_t index) {
    return goRuneAt(interp, str, index);
}

FeatherObj feather_host_rune_range(FeatherInterp interp, FeatherObj str, int64_t first, int64_t last) {
    return goRuneRange(interp, str, first, last);
}

FeatherObj feather_host_rune_to_upper(FeatherInterp interp, FeatherObj str) {
    return goRuneToUpper(interp, str);
}

FeatherObj feather_host_rune_to_lower(FeatherInterp interp, FeatherObj str) {
    return goRuneToLower(interp, str);
}

FeatherObj feather_host_rune_fold(FeatherInterp interp, FeatherObj str) {
    return goRuneFold(interp, str);
}

int feather_host_rune_is_class(FeatherInterp interp, FeatherObj ch, FeatherCharClass charClass) {
    return goRuneIsClass(interp, ch, charClass);
}

// ============================================================================
// Interp Operations
// ============================================================================

FeatherResult feather_host_interp_set_result(FeatherInterp interp, FeatherObj result) {
    return goInterpSetResult(interp, result);
}

FeatherObj feather_host_interp_get_result(FeatherInterp interp) {
    return goInterpGetResult(interp);
}

FeatherResult feather_host_interp_reset_result(FeatherInterp interp, FeatherObj result) {
    return goInterpResetResult(interp, result);
}

FeatherResult feather_host_interp_set_return_options(FeatherInterp interp, FeatherObj options) {
    return goInterpSetReturnOptions(interp, options);
}

FeatherObj feather_host_interp_get_return_options(FeatherInterp interp, FeatherResult code) {
    return goInterpGetReturnOptions(interp, code);
}

FeatherObj feather_host_interp_get_script(FeatherInterp interp) {
    return goInterpGetScript(interp);
}

void feather_host_interp_set_script(FeatherInterp interp, FeatherObj path) {
    goInterpSetScript(interp, path);
}

// ============================================================================
// List Operations
// ============================================================================

FeatherObj feather_host_list_create(FeatherInterp interp) {
    return goListCreate(interp);
}

int feather_host_list_is_nil(FeatherInterp interp, FeatherObj obj) {
    return goListIsNil(interp, obj);
}

FeatherObj feather_host_list_from(FeatherInterp interp, FeatherObj obj) {
    return goListFrom(interp, obj);
}

FeatherObj feather_host_list_push(FeatherInterp interp, FeatherObj list, FeatherObj item) {
    return goListPush(interp, list, item);
}

FeatherObj feather_host_list_pop(FeatherInterp interp, FeatherObj list) {
    return goListPop(interp, list);
}

FeatherObj feather_host_list_unshift(FeatherInterp interp, FeatherObj list, FeatherObj item) {
    return goListUnshift(interp, list, item);
}

FeatherObj feather_host_list_shift(FeatherInterp interp, FeatherObj list) {
    return goListShift(interp, list);
}

size_t feather_host_list_length(FeatherInterp interp, FeatherObj list) {
    return goListLength(interp, list);
}

FeatherObj feather_host_list_at(FeatherInterp interp, FeatherObj list, size_t index) {
    return goListAt(interp, list, index);
}

FeatherObj feather_host_list_slice(FeatherInterp interp, FeatherObj list, size_t first, size_t last) {
    return goListSlice(interp, list, first, last);
}

FeatherResult feather_host_list_set_at(FeatherInterp interp, FeatherObj list, size_t index, FeatherObj value) {
    return goListSetAt(interp, list, index, value);
}

FeatherObj feather_host_list_splice(FeatherInterp interp, FeatherObj list, size_t first,
                                    size_t deleteCount, FeatherObj insertions) {
    return goListSplice(interp, list, first, deleteCount, insertions);
}

FeatherResult feather_host_list_sort(FeatherInterp interp, FeatherObj list,
                                     int (*cmp)(FeatherInterp interp, FeatherObj a, FeatherObj b, void *ctx),
                                     void *ctx) {
    return goListSort(interp, list, (void*)cmp, ctx);
}

// ============================================================================
// Dict Operations
// ============================================================================

FeatherObj feather_host_dict_create(FeatherInterp interp) {
    return goDictCreate(interp);
}

int feather_host_dict_is_dict(FeatherInterp interp, FeatherObj obj) {
    return goDictIsDict(interp, obj);
}

FeatherObj feather_host_dict_from(FeatherInterp interp, FeatherObj obj) {
    return goDictFrom(interp, obj);
}

FeatherObj feather_host_dict_get(FeatherInterp interp, FeatherObj dict, FeatherObj key) {
    return goDictGet(interp, dict, key);
}

FeatherObj feather_host_dict_set(FeatherInterp interp, FeatherObj dict, FeatherObj key, FeatherObj value) {
    return goDictSet(interp, dict, key, value);
}

int feather_host_dict_exists(FeatherInterp interp, FeatherObj dict, FeatherObj key) {
    return goDictExists(interp, dict, key);
}

FeatherObj feather_host_dict_remove(FeatherInterp interp, FeatherObj dict, FeatherObj key) {
    return goDictRemove(interp, dict, key);
}

size_t feather_host_dict_size(FeatherInterp interp, FeatherObj dict) {
    return goDictSize(interp, dict);
}

FeatherObj feather_host_dict_keys(FeatherInterp interp, FeatherObj dict) {
    return goDictKeys(interp, dict);
}

FeatherObj feather_host_dict_values(FeatherInterp interp, FeatherObj dict) {
    return goDictValues(interp, dict);
}

// ============================================================================
// Integer Operations
// ============================================================================

FeatherObj feather_host_integer_create(FeatherInterp interp, int64_t val) {
    return goIntCreate(interp, val);
}

FeatherResult feather_host_integer_get(FeatherInterp interp, FeatherObj obj, int64_t *out) {
    return goIntGet(interp, obj, out);
}

// ============================================================================
// Double Operations
// ============================================================================

FeatherObj feather_host_dbl_create(FeatherInterp interp, double val) {
    return goDoubleCreate(interp, val);
}

FeatherResult feather_host_dbl_get(FeatherInterp interp, FeatherObj obj, double *out) {
    return goDoubleGet(interp, obj, out);
}

FeatherDoubleClass feather_host_dbl_classify(double val) {
    return goDoubleClassify(val);
}

FeatherObj feather_host_dbl_format(FeatherInterp interp, double val, char specifier, int precision, int alternate) {
    return goDoubleFormat(interp, val, specifier, precision, alternate);
}

FeatherResult feather_host_dbl_math(FeatherInterp interp, FeatherMathOp op, double a, double b, double *out) {
    return goDoubleMath(interp, op, a, b, out);
}

// ============================================================================
// Frame Operations
// ============================================================================

FeatherResult feather_host_frame_push(FeatherInterp interp, FeatherObj cmd, FeatherObj args) {
    return goFramePush(interp, cmd, args);
}

FeatherResult feather_host_frame_pop(FeatherInterp interp) {
    return goFramePop(interp);
}

size_t feather_host_frame_level(FeatherInterp interp) {
    return goFrameLevel(interp);
}

FeatherResult feather_host_frame_set_active(FeatherInterp interp, size_t level) {
    return goFrameSetActive(interp, level);
}

size_t feather_host_frame_size(FeatherInterp interp) {
    return goFrameSize(interp);
}

FeatherResult feather_host_frame_info(FeatherInterp interp, size_t level, FeatherObj *cmd, FeatherObj *args, FeatherObj *ns) {
    return goFrameInfo(interp, level, cmd, args, ns);
}

FeatherResult feather_host_frame_set_namespace(FeatherInterp interp, FeatherObj ns) {
    return goFrameSetNamespace(interp, ns);
}

FeatherObj feather_host_frame_get_namespace(FeatherInterp interp) {
    return goFrameGetNamespace(interp);
}

FeatherResult feather_host_frame_push_locals(FeatherInterp interp, FeatherObj ns) {
    return goFramePushLocals(interp, ns);
}

FeatherResult feather_host_frame_pop_locals(FeatherInterp interp) {
    return goFramePopLocals(interp);
}

FeatherResult feather_host_frame_set_line(FeatherInterp interp, size_t line) {
    return goFrameSetLine(interp, line);
}

size_t feather_host_frame_get_line(FeatherInterp interp, size_t level) {
    return goFrameGetLine(interp, level);
}

FeatherResult feather_host_frame_set_lambda(FeatherInterp interp, FeatherObj lambda) {
    return goFrameSetLambda(interp, lambda);
}

FeatherObj feather_host_frame_get_lambda(FeatherInterp interp, size_t level) {
    return goFrameGetLambda(interp, level);
}

// ============================================================================
// Variable Operations
// ============================================================================

FeatherObj feather_host_var_get(FeatherInterp interp, FeatherObj name) {
    return goVarGet(interp, name);
}

void feather_host_var_set(FeatherInterp interp, FeatherObj name, FeatherObj value) {
    goVarSet(interp, name, value);
}

void feather_host_var_unset(FeatherInterp interp, FeatherObj name) {
    goVarUnset(interp, name);
}

FeatherResult feather_host_var_exists(FeatherInterp interp, FeatherObj name) {
    return goVarExists(interp, name);
}

void feather_host_var_link(FeatherInterp interp, FeatherObj local, size_t target_level, FeatherObj target) {
    goVarLink(interp, local, target_level, target);
}

void feather_host_var_link_ns(FeatherInterp interp, FeatherObj local, FeatherObj ns, FeatherObj name) {
    goVarLinkNs(interp, local, ns, name);
}

FeatherObj feather_host_var_names(FeatherInterp interp, FeatherObj ns) {
    return goVarNames(interp, ns);
}

int feather_host_var_is_link(FeatherInterp interp, FeatherObj name) {
    return goVarIsLink(interp, name);
}

FeatherObj feather_host_var_resolve_link(FeatherInterp interp, FeatherObj name) {
    return goVarResolveLink(interp, name);
}

// ============================================================================
// Namespace Operations
// ============================================================================

FeatherResult feather_host_ns_create(FeatherInterp interp, FeatherObj path) {
    return goNsCreate(interp, path);
}

FeatherResult feather_host_ns_delete(FeatherInterp interp, FeatherObj path) {
    return goNsDelete(interp, path);
}

int feather_host_ns_exists(FeatherInterp interp, FeatherObj path) {
    return goNsExists(interp, path);
}

FeatherObj feather_host_ns_current(FeatherInterp interp) {
    return goNsCurrent(interp);
}

FeatherResult feather_host_ns_parent(FeatherInterp interp, FeatherObj ns, FeatherObj *result) {
    return goNsParent(interp, ns, result);
}

FeatherObj feather_host_ns_children(FeatherInterp interp, FeatherObj ns) {
    return goNsChildren(interp, ns);
}

FeatherObj feather_host_ns_get_var(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsGetVar(interp, ns, name);
}

void feather_host_ns_set_var(FeatherInterp interp, FeatherObj ns, FeatherObj name, FeatherObj value) {
    goNsSetVar(interp, ns, name, value);
}

int feather_host_ns_var_exists(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsVarExists(interp, ns, name);
}

void feather_host_ns_unset_var(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    goNsUnsetVar(interp, ns, name);
}

FeatherCommandType feather_host_ns_get_command(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                                               FeatherBuiltinCmd *fn, FeatherObj *params, FeatherObj *body) {
    return goNsGetCommand(interp, ns, name, fn, params, body);
}

void feather_host_ns_set_command(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                                 FeatherCommandType kind, FeatherBuiltinCmd fn,
                                 FeatherObj params, FeatherObj body) {
    goNsSetCommand(interp, ns, name, kind, fn, params, body);
}

FeatherResult feather_host_ns_delete_command(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsDeleteCommand(interp, ns, name);
}

FeatherObj feather_host_ns_list_commands(FeatherInterp interp, FeatherObj ns) {
    return goNsListCommands(interp, ns);
}

FeatherObj feather_host_ns_get_exports(FeatherInterp interp, FeatherObj ns) {
    return goNsGetExports(interp, ns);
}

void feather_host_ns_set_exports(FeatherInterp interp, FeatherObj ns, FeatherObj patterns, int clear) {
    goNsSetExports(interp, ns, patterns, clear);
}

int feather_host_ns_is_exported(FeatherInterp interp, FeatherObj ns, FeatherObj name) {
    return goNsIsExported(interp, ns, name);
}

FeatherResult feather_host_ns_copy_command(FeatherInterp interp, FeatherObj srcNs, FeatherObj srcName,
                                           FeatherObj dstNs, FeatherObj dstName) {
    return goNsCopyCommand(interp, srcNs, srcName, dstNs, dstName);
}

// ============================================================================
// Foreign Operations
// ============================================================================

int feather_host_foreign_is_foreign(FeatherInterp interp, FeatherObj obj) {
    return goForeignIsForeign(interp, obj);
}

FeatherObj feather_host_foreign_type_name(FeatherInterp interp, FeatherObj obj) {
    return goForeignTypeName(interp, obj);
}

FeatherObj feather_host_foreign_string_rep(FeatherInterp interp, FeatherObj obj) {
    return goForeignStringRep(interp, obj);
}

FeatherObj feather_host_foreign_methods(FeatherInterp interp, FeatherObj obj) {
    return goForeignMethods(interp, obj);
}

FeatherResult feather_host_foreign_invoke(FeatherInterp interp, FeatherObj obj, FeatherObj method, FeatherObj args) {
    return goForeignInvoke(interp, obj, method, args);
}

void feather_host_foreign_destroy(FeatherInterp interp, FeatherObj obj) {
    goForeignDestroy(interp, obj);
}
