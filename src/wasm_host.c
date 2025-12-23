/**
 * WASM host operations wrapper.
 * 
 * This file provides the bridge between the tclc interpreter and the
 * WASM host (Go code via wazero). Instead of function pointers passed
 * in a TclHostOps struct, the host operations are WASM imports.
 * 
 * This file is only compiled for the WASM target.
 */

#ifdef __wasm__

#include "tclc.h"

// Declare WASM imports for all TclHostOps callbacks.
// These are implemented in Go and provided via wazero's host module.

// String operations
__attribute__((import_module("env"), import_name("string_intern")))
extern TclObj wasm_string_intern(TclInterp interp, const char *s, size_t len);

__attribute__((import_module("env"), import_name("string_get")))
extern const char* wasm_string_get(TclInterp interp, TclObj obj, size_t *len);

__attribute__((import_module("env"), import_name("string_concat")))
extern TclObj wasm_string_concat(TclInterp interp, TclObj a, TclObj b);

__attribute__((import_module("env"), import_name("string_compare")))
extern int wasm_string_compare(TclInterp interp, TclObj a, TclObj b);

// Integer operations
__attribute__((import_module("env"), import_name("integer_create")))
extern TclObj wasm_integer_create(TclInterp interp, int64_t val);

__attribute__((import_module("env"), import_name("integer_get")))
extern TclResult wasm_integer_get(TclInterp interp, TclObj obj, int64_t *out);

// Double operations
__attribute__((import_module("env"), import_name("dbl_create")))
extern TclObj wasm_dbl_create(TclInterp interp, double val);

__attribute__((import_module("env"), import_name("dbl_get")))
extern TclResult wasm_dbl_get(TclInterp interp, TclObj obj, double *out);

// List operations
__attribute__((import_module("env"), import_name("list_is_nil")))
extern int wasm_list_is_nil(TclInterp interp, TclObj obj);

__attribute__((import_module("env"), import_name("list_create")))
extern TclObj wasm_list_create(TclInterp interp);

__attribute__((import_module("env"), import_name("list_from")))
extern TclObj wasm_list_from(TclInterp interp, TclObj obj);

__attribute__((import_module("env"), import_name("list_push")))
extern TclObj wasm_list_push(TclInterp interp, TclObj list, TclObj item);

__attribute__((import_module("env"), import_name("list_pop")))
extern TclObj wasm_list_pop(TclInterp interp, TclObj list);

__attribute__((import_module("env"), import_name("list_unshift")))
extern TclObj wasm_list_unshift(TclInterp interp, TclObj list, TclObj item);

__attribute__((import_module("env"), import_name("list_shift")))
extern TclObj wasm_list_shift(TclInterp interp, TclObj list);

__attribute__((import_module("env"), import_name("list_length")))
extern size_t wasm_list_length(TclInterp interp, TclObj list);

__attribute__((import_module("env"), import_name("list_at")))
extern TclObj wasm_list_at(TclInterp interp, TclObj list, size_t index);

// Frame operations
__attribute__((import_module("env"), import_name("frame_push")))
extern TclResult wasm_frame_push(TclInterp interp, TclObj cmd, TclObj args);

__attribute__((import_module("env"), import_name("frame_pop")))
extern TclResult wasm_frame_pop(TclInterp interp);

__attribute__((import_module("env"), import_name("frame_level")))
extern size_t wasm_frame_level(TclInterp interp);

__attribute__((import_module("env"), import_name("frame_set_active")))
extern TclResult wasm_frame_set_active(TclInterp interp, size_t level);

__attribute__((import_module("env"), import_name("frame_size")))
extern size_t wasm_frame_size(TclInterp interp);

__attribute__((import_module("env"), import_name("frame_info")))
extern TclResult wasm_frame_info(TclInterp interp, size_t level, TclObj *cmd, TclObj *args);

// Variable operations
__attribute__((import_module("env"), import_name("var_get")))
extern TclObj wasm_var_get(TclInterp interp, TclObj name);

__attribute__((import_module("env"), import_name("var_set")))
extern void wasm_var_set(TclInterp interp, TclObj name, TclObj value);

__attribute__((import_module("env"), import_name("var_unset")))
extern void wasm_var_unset(TclInterp interp, TclObj name);

__attribute__((import_module("env"), import_name("var_exists")))
extern TclResult wasm_var_exists(TclInterp interp, TclObj name);

__attribute__((import_module("env"), import_name("var_link")))
extern void wasm_var_link(TclInterp interp, TclObj local, size_t target_level, TclObj target);

// Procedure operations
__attribute__((import_module("env"), import_name("proc_define")))
extern void wasm_proc_define(TclInterp interp, TclObj name, TclObj params, TclObj body);

__attribute__((import_module("env"), import_name("proc_exists")))
extern int wasm_proc_exists(TclInterp interp, TclObj name);

__attribute__((import_module("env"), import_name("proc_params")))
extern TclResult wasm_proc_params(TclInterp interp, TclObj name, TclObj *result);

__attribute__((import_module("env"), import_name("proc_body")))
extern TclResult wasm_proc_body(TclInterp interp, TclObj name, TclObj *result);

__attribute__((import_module("env"), import_name("proc_names")))
extern TclObj wasm_proc_names(TclInterp interp, TclObj namespace);

__attribute__((import_module("env"), import_name("proc_resolve_namespace")))
extern TclResult wasm_proc_resolve_namespace(TclInterp interp, TclObj path, TclObj *result);

__attribute__((import_module("env"), import_name("proc_register_command")))
extern void wasm_proc_register_command(TclInterp interp, TclObj name);

__attribute__((import_module("env"), import_name("proc_lookup")))
extern TclCommandType wasm_proc_lookup(TclInterp interp, TclObj name, TclObj *canonical_name);

__attribute__((import_module("env"), import_name("proc_rename")))
extern TclResult wasm_proc_rename(TclInterp interp, TclObj oldName, TclObj newName);

// Interpreter operations
__attribute__((import_module("env"), import_name("interp_set_result")))
extern TclResult wasm_interp_set_result(TclInterp interp, TclObj result);

__attribute__((import_module("env"), import_name("interp_get_result")))
extern TclObj wasm_interp_get_result(TclInterp interp);

__attribute__((import_module("env"), import_name("interp_reset_result")))
extern TclResult wasm_interp_reset_result(TclInterp interp, TclObj result);

__attribute__((import_module("env"), import_name("interp_set_return_options")))
extern TclResult wasm_interp_set_return_options(TclInterp interp, TclObj options);

__attribute__((import_module("env"), import_name("interp_get_return_options")))
extern TclObj wasm_interp_get_return_options(TclInterp interp, TclResult code);

// Bind operations
__attribute__((import_module("env"), import_name("bind_unknown")))
extern TclResult wasm_bind_unknown(TclInterp interp, TclObj cmd, TclObj args, TclObj *value);


// Static wrapper functions that match TclHostOps signatures

static TclObj c_string_intern(TclInterp interp, const char *s, size_t len) {
    return wasm_string_intern(interp, s, len);
}

static const char* c_string_get(TclInterp interp, TclObj obj, size_t *len) {
    return wasm_string_get(interp, obj, len);
}

static TclObj c_string_concat(TclInterp interp, TclObj a, TclObj b) {
    return wasm_string_concat(interp, a, b);
}

static int c_string_compare(TclInterp interp, TclObj a, TclObj b) {
    return wasm_string_compare(interp, a, b);
}

static TclObj c_integer_create(TclInterp interp, int64_t val) {
    return wasm_integer_create(interp, val);
}

static TclResult c_integer_get(TclInterp interp, TclObj obj, int64_t *out) {
    return wasm_integer_get(interp, obj, out);
}

static TclObj c_dbl_create(TclInterp interp, double val) {
    return wasm_dbl_create(interp, val);
}

static TclResult c_dbl_get(TclInterp interp, TclObj obj, double *out) {
    return wasm_dbl_get(interp, obj, out);
}

static int c_list_is_nil(TclInterp interp, TclObj obj) {
    return wasm_list_is_nil(interp, obj);
}

static TclObj c_list_create(TclInterp interp) {
    return wasm_list_create(interp);
}

static TclObj c_list_from(TclInterp interp, TclObj obj) {
    return wasm_list_from(interp, obj);
}

static TclObj c_list_push(TclInterp interp, TclObj list, TclObj item) {
    return wasm_list_push(interp, list, item);
}

static TclObj c_list_pop(TclInterp interp, TclObj list) {
    return wasm_list_pop(interp, list);
}

static TclObj c_list_unshift(TclInterp interp, TclObj list, TclObj item) {
    return wasm_list_unshift(interp, list, item);
}

static TclObj c_list_shift(TclInterp interp, TclObj list) {
    return wasm_list_shift(interp, list);
}

static size_t c_list_length(TclInterp interp, TclObj list) {
    return wasm_list_length(interp, list);
}

static TclObj c_list_at(TclInterp interp, TclObj list, size_t index) {
    return wasm_list_at(interp, list, index);
}

static TclResult c_frame_push(TclInterp interp, TclObj cmd, TclObj args) {
    return wasm_frame_push(interp, cmd, args);
}

static TclResult c_frame_pop(TclInterp interp) {
    return wasm_frame_pop(interp);
}

static size_t c_frame_level(TclInterp interp) {
    return wasm_frame_level(interp);
}

static TclResult c_frame_set_active(TclInterp interp, size_t level) {
    return wasm_frame_set_active(interp, level);
}

static size_t c_frame_size(TclInterp interp) {
    return wasm_frame_size(interp);
}

static TclResult c_frame_info(TclInterp interp, size_t level, TclObj *cmd, TclObj *args) {
    return wasm_frame_info(interp, level, cmd, args);
}

static TclObj c_var_get(TclInterp interp, TclObj name) {
    return wasm_var_get(interp, name);
}

static void c_var_set(TclInterp interp, TclObj name, TclObj value) {
    wasm_var_set(interp, name, value);
}

static void c_var_unset(TclInterp interp, TclObj name) {
    wasm_var_unset(interp, name);
}

static TclResult c_var_exists(TclInterp interp, TclObj name) {
    return wasm_var_exists(interp, name);
}

static void c_var_link(TclInterp interp, TclObj local, size_t target_level, TclObj target) {
    wasm_var_link(interp, local, target_level, target);
}

static void c_proc_define(TclInterp interp, TclObj name, TclObj params, TclObj body) {
    wasm_proc_define(interp, name, params, body);
}

static int c_proc_exists(TclInterp interp, TclObj name) {
    return wasm_proc_exists(interp, name);
}

static TclResult c_proc_params(TclInterp interp, TclObj name, TclObj *result) {
    return wasm_proc_params(interp, name, result);
}

static TclResult c_proc_body(TclInterp interp, TclObj name, TclObj *result) {
    return wasm_proc_body(interp, name, result);
}

static TclObj c_proc_names(TclInterp interp, TclObj namespace) {
    return wasm_proc_names(interp, namespace);
}

static TclResult c_proc_resolve_namespace(TclInterp interp, TclObj path, TclObj *result) {
    return wasm_proc_resolve_namespace(interp, path, result);
}

static void c_proc_register_command(TclInterp interp, TclObj name) {
    wasm_proc_register_command(interp, name);
}

static TclCommandType c_proc_lookup(TclInterp interp, TclObj name, TclObj *canonical_name) {
    return wasm_proc_lookup(interp, name, canonical_name);
}

static TclResult c_proc_rename(TclInterp interp, TclObj oldName, TclObj newName) {
    return wasm_proc_rename(interp, oldName, newName);
}

static TclResult c_interp_set_result(TclInterp interp, TclObj result) {
    return wasm_interp_set_result(interp, result);
}

static TclObj c_interp_get_result(TclInterp interp) {
    return wasm_interp_get_result(interp);
}

static TclResult c_interp_reset_result(TclInterp interp, TclObj result) {
    return wasm_interp_reset_result(interp, result);
}

static TclResult c_interp_set_return_options(TclInterp interp, TclObj options) {
    return wasm_interp_set_return_options(interp, options);
}

static TclObj c_interp_get_return_options(TclInterp interp, TclResult code) {
    return wasm_interp_get_return_options(interp, code);
}

static TclResult c_bind_unknown(TclInterp interp, TclObj cmd, TclObj args, TclObj *value) {
    return wasm_bind_unknown(interp, cmd, args, value);
}


// Global TclHostOps struct populated with WASM imports
static TclHostOps wasm_host_ops;
static int wasm_host_ops_initialized = 0;

static void init_wasm_host_ops(void) {
    if (wasm_host_ops_initialized) return;
    
    wasm_host_ops.string.intern = c_string_intern;
    wasm_host_ops.string.get = c_string_get;
    wasm_host_ops.string.concat = c_string_concat;
    wasm_host_ops.string.compare = c_string_compare;
    
    wasm_host_ops.integer.create = c_integer_create;
    wasm_host_ops.integer.get = c_integer_get;
    
    wasm_host_ops.dbl.create = c_dbl_create;
    wasm_host_ops.dbl.get = c_dbl_get;
    
    wasm_host_ops.list.is_nil = c_list_is_nil;
    wasm_host_ops.list.create = c_list_create;
    wasm_host_ops.list.from = c_list_from;
    wasm_host_ops.list.push = c_list_push;
    wasm_host_ops.list.pop = c_list_pop;
    wasm_host_ops.list.unshift = c_list_unshift;
    wasm_host_ops.list.shift = c_list_shift;
    wasm_host_ops.list.length = c_list_length;
    wasm_host_ops.list.at = c_list_at;
    
    wasm_host_ops.frame.push = c_frame_push;
    wasm_host_ops.frame.pop = c_frame_pop;
    wasm_host_ops.frame.level = c_frame_level;
    wasm_host_ops.frame.set_active = c_frame_set_active;
    wasm_host_ops.frame.size = c_frame_size;
    wasm_host_ops.frame.info = c_frame_info;
    
    wasm_host_ops.var.get = c_var_get;
    wasm_host_ops.var.set = c_var_set;
    wasm_host_ops.var.unset = c_var_unset;
    wasm_host_ops.var.exists = c_var_exists;
    wasm_host_ops.var.link = c_var_link;
    
    wasm_host_ops.proc.define = c_proc_define;
    wasm_host_ops.proc.exists = c_proc_exists;
    wasm_host_ops.proc.params = c_proc_params;
    wasm_host_ops.proc.body = c_proc_body;
    wasm_host_ops.proc.names = c_proc_names;
    wasm_host_ops.proc.resolve_namespace = c_proc_resolve_namespace;
    wasm_host_ops.proc.register_command = c_proc_register_command;
    wasm_host_ops.proc.lookup = c_proc_lookup;
    wasm_host_ops.proc.rename = c_proc_rename;
    
    wasm_host_ops.interp.set_result = c_interp_set_result;
    wasm_host_ops.interp.get_result = c_interp_get_result;
    wasm_host_ops.interp.reset_result = c_interp_reset_result;
    wasm_host_ops.interp.set_return_options = c_interp_set_return_options;
    wasm_host_ops.interp.get_return_options = c_interp_get_return_options;
    
    wasm_host_ops.bind.unknown = c_bind_unknown;
    
    wasm_host_ops_initialized = 1;
}

// Get the global WASM host ops
const TclHostOps* wasm_get_host_ops(void) {
    init_wasm_host_ops();
    return &wasm_host_ops;
}

// WASM-exported wrapper functions that use the global host ops

__attribute__((visibility("default")))
void wasm_interp_init(TclInterp interp) {
    init_wasm_host_ops();
    tcl_interp_init(&wasm_host_ops, interp);
}

__attribute__((visibility("default")))
TclResult wasm_script_eval(TclInterp interp, const char *source, size_t len, TclEvalFlags flags) {
    init_wasm_host_ops();
    return tcl_script_eval(&wasm_host_ops, interp, source, len, flags);
}

__attribute__((visibility("default")))
TclResult wasm_script_eval_obj(TclInterp interp, TclObj script, TclEvalFlags flags) {
    init_wasm_host_ops();
    return tcl_script_eval_obj(&wasm_host_ops, interp, script, flags);
}

__attribute__((visibility("default")))
TclResult wasm_command_exec(TclInterp interp, TclObj command, TclEvalFlags flags) {
    init_wasm_host_ops();
    return tcl_command_exec(&wasm_host_ops, interp, command, flags);
}

#endif // __wasm__
