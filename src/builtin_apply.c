#include "feather.h"
#include "internal.h"
#include "charclass.h"

FeatherResult feather_builtin_apply(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args) {
  (void)cmd;

  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"apply lambdaExpr ?arg ...?\"", 52);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj argsCopy = ops->list.from(interp, args);
  FeatherObj lambdaExpr = ops->list.shift(interp, argsCopy);

  size_t lambdaLen = ops->list.length(interp, lambdaExpr);
  if (lambdaLen < 2 || lambdaLen > 3) {
    FeatherObj msg = ops->string.intern(interp, "can't interpret \"", 17);
    msg = ops->string.concat(interp, msg, lambdaExpr);
    msg = ops->string.concat(interp, msg,
        ops->string.intern(interp, "\" as a lambda expression", 24));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj lambdaCopy = ops->list.from(interp, lambdaExpr);
  FeatherObj params = ops->list.shift(interp, lambdaCopy);
  FeatherObj body = ops->list.shift(interp, lambdaCopy);
  FeatherObj ns = 0;
  if (lambdaLen == 3) {
    ns = ops->list.shift(interp, lambdaCopy);
  }

  size_t paramc = ops->list.length(interp, params);
  size_t provided_argc = argc - 1;

  int is_variadic = 0;

  // Track parameter types: 0=required, 1=optional, 2=args
  // We need to know which optionals become required due to TCL rule:
  // "optional params followed by required params become required"
  int param_types[64];
  int param_effectively_required[64];  // After applying the rule
  if (paramc > 64) paramc = 64;  // Safety limit

  FeatherObj paramsCopy = ops->list.from(interp, params);
  for (size_t i = 0; i < paramc; i++) {
    FeatherObj param = ops->list.at(interp, paramsCopy, i);

    size_t paramListLen = ops->list.length(interp, param);
    if (paramListLen == 2) {
      param_types[i] = 1;  // optional
    } else if (feather_obj_is_args_param(ops, interp, param)) {
      param_types[i] = 2;  // args
      is_variadic = 1;
    } else {
      param_types[i] = 0;  // required
    }
    param_effectively_required[i] = (param_types[i] == 0);  // Start with required only
  }

  // TCL rule: scan backwards from end, any optional before a non-args required
  // param becomes effectively required
  int seen_non_args_required = 0;
  for (size_t i = paramc; i > 0; i--) {
    size_t idx = i - 1;
    if (param_types[idx] == 0) {
      // Required param (not args)
      seen_non_args_required = 1;
      param_effectively_required[idx] = 1;
    } else if (param_types[idx] == 1) {
      // Optional - becomes required if followed by non-args required
      param_effectively_required[idx] = seen_non_args_required;
    }
    // args (type 2) doesn't affect seen_non_args_required and stays optional
  }

  // Count min/max args based on effective requirements
  size_t min_args = 0;
  size_t max_args = 0;
  for (size_t i = 0; i < paramc; i++) {
    if (param_types[i] == 2) {
      // args - unlimited max
      continue;
    }
    max_args++;
    if (param_effectively_required[i]) {
      min_args++;
    }
  }
  if (is_variadic) {
    max_args = (size_t)-1;
  }

  if (provided_argc < min_args || provided_argc > max_args) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"apply lambdaExpr", 41);

    paramsCopy = ops->list.from(interp, params);
    for (size_t i = 0; i < paramc; i++) {
      FeatherObj param = ops->list.shift(interp, paramsCopy);

      msg = ops->string.concat(interp, msg, ops->string.intern(interp, " ", 1));

      if (param_types[i] == 2) {
        // args parameter
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "?arg ...?", 9));
      } else if (param_types[i] == 1) {
        // Optional parameter - always shown with ?...? notation
        FeatherObj paramName = ops->list.at(interp, param, 0);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "?", 1));
        msg = ops->string.concat(interp, msg, paramName);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "?", 1));
      } else {
        // Required parameter
        msg = ops->string.concat(interp, msg, param);
      }
    }

    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Get line number from parent frame before pushing
  size_t parentLevel = ops->frame.level(interp);
  size_t parentLine = ops->frame.get_line(interp, parentLevel);

  FeatherObj applyName = ops->string.intern(interp, "apply", 5);
  if (ops->frame.push(interp, applyName, args) != TCL_OK) {
    return TCL_ERROR;
  }

  // Copy line number from parent and store the lambda expression
  ops->frame.set_line(interp, parentLine);
  ops->frame.set_lambda(interp, lambdaExpr);

  if (ns != 0) {
    FeatherObj absNs;
    // Check if ns starts with "::"
    int b0 = ops->string.byte_at(interp, ns, 0);
    int b1 = ops->string.byte_at(interp, ns, 1);
    if (b0 == ':' && b1 == ':') {
      absNs = ns;
    } else {
      absNs = ops->string.intern(interp, "::", 2);
      absNs = ops->string.concat(interp, absNs, ns);
    }
    ops->ns.create(interp, absNs);
    ops->frame.set_namespace(interp, absNs);
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));

  paramsCopy = ops->list.from(interp, params);
  size_t arg_index = 0;

  for (size_t i = 0; i < paramc; i++) {
    FeatherObj param = ops->list.shift(interp, paramsCopy);

    size_t paramListLen = ops->list.length(interp, param);

    if (feather_obj_is_args_param(ops, interp, param) && paramListLen == 1) {
      FeatherObj collectedArgs = ops->list.create(interp);
      while (arg_index < provided_argc) {
        FeatherObj arg = ops->list.shift(interp, argsCopy);
        collectedArgs = ops->list.push(interp, collectedArgs, arg);
        arg_index++;
      }
      ops->var.set(interp, param, collectedArgs);
    } else if (paramListLen == 2) {
      FeatherObj paramName = ops->list.at(interp, param, 0);
      if (arg_index < provided_argc) {
        FeatherObj arg = ops->list.shift(interp, argsCopy);
        ops->var.set(interp, paramName, arg);
        arg_index++;
      } else {
        FeatherObj defaultVal = ops->list.at(interp, param, 1);
        ops->var.set(interp, paramName, defaultVal);
      }
    } else {
      FeatherObj arg = ops->list.shift(interp, argsCopy);
      ops->var.set(interp, param, arg);
      arg_index++;
    }
  }

  FeatherResult result = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

  ops->frame.pop(interp);

  if (result == TCL_RETURN) {
    FeatherObj opts = ops->interp.get_return_options(interp, result);

    int code = TCL_OK;
    int level = 1;

    size_t optsLen = ops->list.length(interp, opts);
    FeatherObj optsCopy = ops->list.from(interp, opts);

    for (size_t i = 0; i + 1 < optsLen; i += 2) {
      FeatherObj key = ops->list.shift(interp, optsCopy);
      FeatherObj val = ops->list.shift(interp, optsCopy);

      if (feather_obj_eq_literal(ops, interp, key, "-code")) {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          code = (int)intVal;
        }
      } else if (feather_obj_eq_literal(ops, interp, key, "-level")) {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          level = (int)intVal;
        }
      }
    }

    level--;

    if (level <= 0) {
      return (FeatherResult)code;
    } else {
      FeatherObj newOpts = ops->list.create(interp);
      newOpts = ops->list.push(interp, newOpts,
                               ops->string.intern(interp, "-code", 5));
      newOpts = ops->list.push(interp, newOpts,
                               ops->integer.create(interp, code));
      newOpts = ops->list.push(interp, newOpts,
                               ops->string.intern(interp, "-level", 6));
      newOpts = ops->list.push(interp, newOpts,
                               ops->integer.create(interp, level));
      ops->interp.set_return_options(interp, newOpts);
      return TCL_RETURN;
    }
  }

  return result;
}
