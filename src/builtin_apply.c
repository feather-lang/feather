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
    size_t lambdaStrLen;
    const char *lambdaStr = ops->string.get(interp, lambdaExpr, &lambdaStrLen);
    msg = ops->string.concat(interp, msg, 
        ops->string.intern(interp, lambdaStr, lambdaStrLen));
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
  size_t required_params = 0;
  size_t optional_params = 0;

  FeatherObj paramsCopy = ops->list.from(interp, params);
  for (size_t i = 0; i < paramc; i++) {
    FeatherObj param = ops->list.at(interp, paramsCopy, i);
    size_t paramLen;
    const char *paramStr = ops->string.get(interp, param, &paramLen);

    size_t paramListLen = ops->list.length(interp, param);
    if (paramListLen == 2) {
      optional_params++;
    } else if (feather_is_args_param(paramStr, paramLen)) {
      is_variadic = 1;
    } else {
      required_params++;
    }
  }

  size_t min_args = required_params;
  size_t max_args = is_variadic ? (size_t)-1 : (required_params + optional_params);

  if (provided_argc < min_args || provided_argc > max_args) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"apply lambdaExpr", 41);

    paramsCopy = ops->list.from(interp, params);
    for (size_t i = 0; i < paramc; i++) {
      FeatherObj param = ops->list.shift(interp, paramsCopy);
      size_t paramLen;
      const char *paramStr = ops->string.get(interp, param, &paramLen);

      size_t paramListLen = ops->list.length(interp, param);

      msg = ops->string.concat(interp, msg, ops->string.intern(interp, " ", 1));

      if (feather_is_args_param(paramStr, paramLen) && paramListLen == 1) {
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, "?arg ...?", 9));
      } else if (paramListLen == 2) {
        FeatherObj paramName = ops->list.at(interp, param, 0);
        size_t nameLen;
        const char *nameStr = ops->string.get(interp, paramName, &nameLen);
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, nameStr, nameLen));
      } else {
        msg = ops->string.concat(interp, msg, ops->string.intern(interp, paramStr, paramLen));
      }
    }

    msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj applyName = ops->string.intern(interp, "apply", 5);
  if (ops->frame.push(interp, applyName, args) != TCL_OK) {
    return TCL_ERROR;
  }

  if (ns != 0) {
    size_t nsLen;
    const char *nsStr = ops->string.get(interp, ns, &nsLen);

    FeatherObj absNs;
    if (nsLen >= 2 && nsStr[0] == ':' && nsStr[1] == ':') {
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
    size_t paramLen;
    const char *paramStr = ops->string.get(interp, param, &paramLen);

    size_t paramListLen = ops->list.length(interp, param);

    if (feather_is_args_param(paramStr, paramLen) && paramListLen == 1) {
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

      size_t keyLen;
      const char *keyStr = ops->string.get(interp, key, &keyLen);

      if (keyLen == 5 && keyStr[0] == '-' && keyStr[1] == 'c' &&
          keyStr[2] == 'o' && keyStr[3] == 'd' && keyStr[4] == 'e') {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          code = (int)intVal;
        }
      } else if (keyLen == 6 && keyStr[0] == '-' && keyStr[1] == 'l' &&
                 keyStr[2] == 'e' && keyStr[3] == 'v' && keyStr[4] == 'e' &&
                 keyStr[5] == 'l') {
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
