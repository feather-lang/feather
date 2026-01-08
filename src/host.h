#ifndef FEATHER_HOST_H
#define FEATHER_HOST_H

/**
 * Host Interface for feather WASM builds.
 *
 * WASM hosts must provide implementations for all feather_host_* functions
 * as WASM imports in the "env" namespace.
 *
 * Native hosts can either:
 * - Provide a custom FeatherHostOps struct to feather_script_eval(), or
 * - Link implementations of feather_host_* and pass NULL for ops
 *
 * The naming convention maps struct fields to function names:
 *   ops->frame.push      -> feather_host_frame_push
 *   ops->string.intern   -> feather_host_string_intern
 *   ops->list.at         -> feather_host_list_at
 */

#include "feather.h"

/* ============================================================================
 * Frame Operations (8 functions)
 * ============================================================================ */

extern FeatherResult feather_host_frame_push(FeatherInterp interp, FeatherObj cmd, FeatherObj args);
extern FeatherResult feather_host_frame_pop(FeatherInterp interp);
extern size_t feather_host_frame_level(FeatherInterp interp);
extern FeatherResult feather_host_frame_set_active(FeatherInterp interp, size_t level);
extern size_t feather_host_frame_size(FeatherInterp interp);
extern FeatherResult feather_host_frame_info(FeatherInterp interp, size_t level,
                                             FeatherObj *cmd, FeatherObj *args, FeatherObj *ns);
extern FeatherResult feather_host_frame_set_namespace(FeatherInterp interp, FeatherObj ns);
extern FeatherObj feather_host_frame_get_namespace(FeatherInterp interp);
extern FeatherResult feather_host_frame_push_locals(FeatherInterp interp, FeatherObj ns);
extern FeatherResult feather_host_frame_pop_locals(FeatherInterp interp);
extern FeatherResult feather_host_frame_set_line(FeatherInterp interp, size_t line);
extern size_t feather_host_frame_get_line(FeatherInterp interp, size_t level);
extern FeatherResult feather_host_frame_set_lambda(FeatherInterp interp, FeatherObj lambda);
extern FeatherObj feather_host_frame_get_lambda(FeatherInterp interp, size_t level);

/* ============================================================================
 * Variable Operations (7 functions)
 * ============================================================================ */

extern FeatherObj feather_host_var_get(FeatherInterp interp, FeatherObj name);
extern void feather_host_var_set(FeatherInterp interp, FeatherObj name, FeatherObj value);
extern void feather_host_var_unset(FeatherInterp interp, FeatherObj name);
extern FeatherResult feather_host_var_exists(FeatherInterp interp, FeatherObj name);
extern void feather_host_var_link(FeatherInterp interp, FeatherObj local, size_t target_level,
                                  FeatherObj target);
extern void feather_host_var_link_ns(FeatherInterp interp, FeatherObj local, FeatherObj ns,
                                     FeatherObj name);
extern FeatherObj feather_host_var_names(FeatherInterp interp, FeatherObj ns);
extern int feather_host_var_is_link(FeatherInterp interp, FeatherObj name);
extern FeatherObj feather_host_var_resolve_link(FeatherInterp interp, FeatherObj name);

/* ============================================================================
 * Namespace Operations (18 functions)
 * ============================================================================ */

extern FeatherResult feather_host_ns_create(FeatherInterp interp, FeatherObj path);
extern FeatherResult feather_host_ns_delete(FeatherInterp interp, FeatherObj path);
extern int feather_host_ns_exists(FeatherInterp interp, FeatherObj path);
extern FeatherObj feather_host_ns_current(FeatherInterp interp);
extern FeatherResult feather_host_ns_parent(FeatherInterp interp, FeatherObj ns,
                                            FeatherObj *result);
extern FeatherObj feather_host_ns_children(FeatherInterp interp, FeatherObj ns);
extern FeatherObj feather_host_ns_get_var(FeatherInterp interp, FeatherObj ns, FeatherObj name);
extern void feather_host_ns_set_var(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                                    FeatherObj value);
extern int feather_host_ns_var_exists(FeatherInterp interp, FeatherObj ns, FeatherObj name);
extern void feather_host_ns_unset_var(FeatherInterp interp, FeatherObj ns, FeatherObj name);
extern FeatherCommandType feather_host_ns_get_command(FeatherInterp interp, FeatherObj ns,
                                                      FeatherObj name, FeatherBuiltinCmd *fn,
                                                      FeatherObj *params, FeatherObj *body);
extern void feather_host_ns_set_command(FeatherInterp interp, FeatherObj ns, FeatherObj name,
                                        FeatherCommandType kind, FeatherBuiltinCmd fn,
                                        FeatherObj params, FeatherObj body);
extern FeatherResult feather_host_ns_delete_command(FeatherInterp interp, FeatherObj ns,
                                                    FeatherObj name);
extern FeatherObj feather_host_ns_list_commands(FeatherInterp interp, FeatherObj ns);
extern FeatherObj feather_host_ns_get_exports(FeatherInterp interp, FeatherObj ns);
extern void feather_host_ns_set_exports(FeatherInterp interp, FeatherObj ns, FeatherObj patterns,
                                        int clear);
extern int feather_host_ns_is_exported(FeatherInterp interp, FeatherObj ns, FeatherObj name);
extern FeatherResult feather_host_ns_copy_command(FeatherInterp interp, FeatherObj srcNs,
                                                  FeatherObj srcName, FeatherObj dstNs,
                                                  FeatherObj dstName);

/* ============================================================================
 * String Operations (13 functions)
 * ============================================================================ */

extern int feather_host_string_byte_at(FeatherInterp interp, FeatherObj str, size_t index);
extern size_t feather_host_string_byte_length(FeatherInterp interp, FeatherObj str);
extern FeatherObj feather_host_string_slice(FeatherInterp interp, FeatherObj str, size_t start,
                                            size_t end);
extern FeatherObj feather_host_string_concat(FeatherInterp interp, FeatherObj a, FeatherObj b);
extern int feather_host_string_compare(FeatherInterp interp, FeatherObj a, FeatherObj b);
extern int feather_host_string_equal(FeatherInterp interp, FeatherObj a, FeatherObj b);
extern int feather_host_string_match(FeatherInterp interp, FeatherObj pattern, FeatherObj str,
                                     int nocase);
extern FeatherResult feather_host_string_regex_match(FeatherInterp interp, FeatherObj pattern,
                                                     FeatherObj string, int nocase, int *result,
                                                     FeatherObj *matches, FeatherObj *indices);
extern FeatherObj feather_host_string_builder_new(FeatherInterp interp, size_t capacity);
extern void feather_host_string_builder_append_byte(FeatherInterp interp, FeatherObj builder,
                                                    int byte);
extern void feather_host_string_builder_append_obj(FeatherInterp interp, FeatherObj builder,
                                                   FeatherObj str);
extern FeatherObj feather_host_string_builder_finish(FeatherInterp interp, FeatherObj builder);
extern FeatherObj feather_host_string_intern(FeatherInterp interp, const char *s, size_t len);

/* ============================================================================
 * Rune Operations (7 functions)
 * ============================================================================ */

extern size_t feather_host_rune_length(FeatherInterp interp, FeatherObj str);
extern FeatherObj feather_host_rune_at(FeatherInterp interp, FeatherObj str, size_t index);
extern FeatherObj feather_host_rune_range(FeatherInterp interp, FeatherObj str, int64_t first,
                                          int64_t last);
extern FeatherObj feather_host_rune_to_upper(FeatherInterp interp, FeatherObj str);
extern FeatherObj feather_host_rune_to_lower(FeatherInterp interp, FeatherObj str);
extern FeatherObj feather_host_rune_fold(FeatherInterp interp, FeatherObj str);
extern int feather_host_rune_is_class(FeatherInterp interp, FeatherObj ch,
                                      FeatherCharClass charClass);

/* ============================================================================
 * List Operations (13 functions)
 * ============================================================================ */

extern int feather_host_list_is_nil(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_list_create(FeatherInterp interp);
extern FeatherObj feather_host_list_from(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_list_push(FeatherInterp interp, FeatherObj list, FeatherObj item);
extern FeatherObj feather_host_list_pop(FeatherInterp interp, FeatherObj list);
extern FeatherObj feather_host_list_unshift(FeatherInterp interp, FeatherObj list, FeatherObj item);
extern FeatherObj feather_host_list_shift(FeatherInterp interp, FeatherObj list);
extern size_t feather_host_list_length(FeatherInterp interp, FeatherObj list);
extern FeatherObj feather_host_list_at(FeatherInterp interp, FeatherObj list, size_t index);
extern FeatherObj feather_host_list_slice(FeatherInterp interp, FeatherObj list, size_t first,
                                          size_t last);
extern FeatherResult feather_host_list_set_at(FeatherInterp interp, FeatherObj list, size_t index,
                                              FeatherObj value);
extern FeatherObj feather_host_list_splice(FeatherInterp interp, FeatherObj list, size_t first,
                                           size_t deleteCount, FeatherObj insertions);
extern FeatherResult feather_host_list_sort(FeatherInterp interp, FeatherObj list,
                                            int (*cmp)(FeatherInterp interp, FeatherObj a,
                                                       FeatherObj b, void *ctx),
                                            void *ctx);

/* ============================================================================
 * Dict Operations (10 functions)
 * ============================================================================ */

extern FeatherObj feather_host_dict_create(FeatherInterp interp);
extern int feather_host_dict_is_dict(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_dict_from(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_dict_get(FeatherInterp interp, FeatherObj dict, FeatherObj key);
extern FeatherObj feather_host_dict_set(FeatherInterp interp, FeatherObj dict, FeatherObj key,
                                        FeatherObj value);
extern int feather_host_dict_exists(FeatherInterp interp, FeatherObj dict, FeatherObj key);
extern FeatherObj feather_host_dict_remove(FeatherInterp interp, FeatherObj dict, FeatherObj key);
extern size_t feather_host_dict_size(FeatherInterp interp, FeatherObj dict);
extern FeatherObj feather_host_dict_keys(FeatherInterp interp, FeatherObj dict);
extern FeatherObj feather_host_dict_values(FeatherInterp interp, FeatherObj dict);

/* ============================================================================
 * Integer Operations (2 functions)
 * ============================================================================ */

extern FeatherObj feather_host_integer_create(FeatherInterp interp, int64_t val);
extern FeatherResult feather_host_integer_get(FeatherInterp interp, FeatherObj obj, int64_t *out);

/* ============================================================================
 * Double Operations (5 functions)
 * ============================================================================ */

extern FeatherObj feather_host_dbl_create(FeatherInterp interp, double val);
extern FeatherResult feather_host_dbl_get(FeatherInterp interp, FeatherObj obj, double *out);
extern FeatherDoubleClass feather_host_dbl_classify(double val);
extern FeatherObj feather_host_dbl_format(FeatherInterp interp, double val, char specifier,
                                          int precision, int alternate);
extern FeatherResult feather_host_dbl_math(FeatherInterp interp, FeatherMathOp op, double a,
                                           double b, double *out);

/* ============================================================================
 * Interp Operations (7 functions)
 * ============================================================================ */

extern FeatherResult feather_host_interp_set_result(FeatherInterp interp, FeatherObj result);
extern FeatherObj feather_host_interp_get_result(FeatherInterp interp);
extern FeatherResult feather_host_interp_reset_result(FeatherInterp interp, FeatherObj result);
extern FeatherResult feather_host_interp_set_return_options(FeatherInterp interp,
                                                            FeatherObj options);
extern FeatherObj feather_host_interp_get_return_options(FeatherInterp interp, FeatherResult code);
extern FeatherObj feather_host_interp_get_script(FeatherInterp interp);
extern void feather_host_interp_set_script(FeatherInterp interp, FeatherObj path);

/* ============================================================================
 * Bind Operations (1 function)
 * ============================================================================ */

extern FeatherResult feather_host_bind_unknown(FeatherInterp interp, FeatherObj cmd,
                                               FeatherObj args, FeatherObj *value);

/* ============================================================================
 * Trace Operations (5 functions)
 * ============================================================================ */

extern FeatherResult feather_host_trace_add(FeatherInterp interp, FeatherObj kind, FeatherObj name,
                                            FeatherObj ops, FeatherObj script);
extern FeatherResult feather_host_trace_remove(FeatherInterp interp, FeatherObj kind,
                                               FeatherObj name, FeatherObj ops, FeatherObj script);
extern FeatherObj feather_host_trace_info(FeatherInterp interp, FeatherObj kind, FeatherObj name);
extern void feather_host_trace_fire_enter(FeatherInterp interp, FeatherObj cmdName, FeatherObj cmdList);
extern void feather_host_trace_fire_leave(FeatherInterp interp, FeatherObj cmdName, FeatherObj cmdList,
                                          FeatherResult code, FeatherObj result);

/* ============================================================================
 * Foreign Operations (6 functions)
 * ============================================================================ */

extern int feather_host_foreign_is_foreign(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_foreign_type_name(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_foreign_string_rep(FeatherInterp interp, FeatherObj obj);
extern FeatherObj feather_host_foreign_methods(FeatherInterp interp, FeatherObj obj);
extern FeatherResult feather_host_foreign_invoke(FeatherInterp interp, FeatherObj obj,
                                                 FeatherObj method, FeatherObj args);
extern void feather_host_foreign_destroy(FeatherInterp interp, FeatherObj obj);

/* ============================================================================
 * Helper function
 * ============================================================================ */

/**
 * feather_get_ops returns the provided ops if non-NULL, otherwise returns
 * the default ops struct populated with feather_host_* function pointers.
 *
 * This allows public API functions to accept NULL for ops parameter,
 * falling back to the default host functions.
 */
extern const FeatherHostOps *feather_get_ops(const FeatherHostOps *ops);

#endif /* FEATHER_HOST_H */
