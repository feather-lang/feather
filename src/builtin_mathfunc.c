#include "feather.h"
#include "internal.h"

/* Helper: Get one double argument from args list */
static FeatherResult get_one_double(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj args, const char *funcname, double *out) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"", 25);
    FeatherObj fn = ops->string.intern(interp, funcname, feather_strlen(funcname));
    FeatherObj suffix = ops->string.intern(interp, " value\"", 7);
    msg = ops->string.concat(interp, msg, fn);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj arg = ops->list.at(interp, args, 0);
  return ops->dbl.get(interp, arg, out);
}

/* Helper: Get one double argument with TCL-style error messages for math functions */
static FeatherResult get_one_double_mathfunc(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj args, const char *funcname, double *out) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "not enough arguments for math function \"", 40);
    FeatherObj fn = ops->string.intern(interp, funcname, feather_strlen(funcname));
    FeatherObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, fn);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  if (argc > 1) {
    FeatherObj msg = ops->string.intern(interp, "too many arguments for math function \"", 38);
    FeatherObj fn = ops->string.intern(interp, funcname, feather_strlen(funcname));
    FeatherObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, fn);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj arg = ops->list.at(interp, args, 0);
  return ops->dbl.get(interp, arg, out);
}

/* Helper: Get two doubles with TCL-style error messages for math functions */
static FeatherResult get_two_doubles_mathfunc(const FeatherHostOps *ops, FeatherInterp interp,
                                               FeatherObj args, const char *funcname,
                                               double *a, double *b) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp, "not enough arguments for math function \"", 40);
    FeatherObj fn = ops->string.intern(interp, funcname, feather_strlen(funcname));
    FeatherObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, fn);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  if (argc > 2) {
    FeatherObj msg = ops->string.intern(interp, "too many arguments for math function \"", 38);
    FeatherObj fn = ops->string.intern(interp, funcname, feather_strlen(funcname));
    FeatherObj suffix = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, fn);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  if (ops->dbl.get(interp, ops->list.at(interp, args, 0), a) != TCL_OK) {
    return TCL_ERROR;
  }
  return ops->dbl.get(interp, ops->list.at(interp, args, 1), b);
}

/* Helper: Get two double arguments from args list */
static FeatherResult get_two_doubles(const FeatherHostOps *ops, FeatherInterp interp,
                                     FeatherObj args, const char *funcname,
                                     double *a, double *b) {
  size_t argc = ops->list.length(interp, args);
  if (argc != 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"", 25);
    FeatherObj fn = ops->string.intern(interp, funcname, feather_strlen(funcname));
    FeatherObj suffix = ops->string.intern(interp, " x y\"", 5);
    msg = ops->string.concat(interp, msg, fn);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  if (ops->dbl.get(interp, ops->list.at(interp, args, 0), a) != TCL_OK) {
    return TCL_ERROR;
  }
  return ops->dbl.get(interp, ops->list.at(interp, args, 1), b);
}

/* Helper: Call unary math op and set result */
static FeatherResult unary_math(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj args, const char *funcname, FeatherMathOp op) {
  double arg, result;
  if (get_one_double(ops, interp, args, funcname, &arg) != TCL_OK) {
    return TCL_ERROR;
  }
  if (ops->dbl.math(interp, op, arg, 0, &result) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, ops->dbl.create(interp, result));
  return TCL_OK;
}

/* Helper: Call binary math op and set result */
static FeatherResult binary_math(const FeatherHostOps *ops, FeatherInterp interp,
                                 FeatherObj args, const char *funcname, FeatherMathOp op) {
  double a, b, result;
  if (get_two_doubles(ops, interp, args, funcname, &a, &b) != TCL_OK) {
    return TCL_ERROR;
  }
  if (ops->dbl.math(interp, op, a, b, &result) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, ops->dbl.create(interp, result));
  return TCL_OK;
}

/* Unary math functions */

FeatherResult feather_builtin_mathfunc_sqrt(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::sqrt", FEATHER_MATH_SQRT);
}

FeatherResult feather_builtin_mathfunc_exp(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::exp", FEATHER_MATH_EXP);
}

FeatherResult feather_builtin_mathfunc_log(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::log", FEATHER_MATH_LOG);
}

FeatherResult feather_builtin_mathfunc_log10(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::log10", FEATHER_MATH_LOG10);
}

FeatherResult feather_builtin_mathfunc_sin(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::sin", FEATHER_MATH_SIN);
}

FeatherResult feather_builtin_mathfunc_cos(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::cos", FEATHER_MATH_COS);
}

FeatherResult feather_builtin_mathfunc_tan(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::tan", FEATHER_MATH_TAN);
}

FeatherResult feather_builtin_mathfunc_asin(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::asin", FEATHER_MATH_ASIN);
}

FeatherResult feather_builtin_mathfunc_acos(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::acos", FEATHER_MATH_ACOS);
}

FeatherResult feather_builtin_mathfunc_atan(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::atan", FEATHER_MATH_ATAN);
}

FeatherResult feather_builtin_mathfunc_sinh(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::sinh", FEATHER_MATH_SINH);
}

FeatherResult feather_builtin_mathfunc_cosh(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::cosh", FEATHER_MATH_COSH);
}

FeatherResult feather_builtin_mathfunc_tanh(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::tanh", FEATHER_MATH_TANH);
}

FeatherResult feather_builtin_mathfunc_floor(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::floor", FEATHER_MATH_FLOOR);
}

FeatherResult feather_builtin_mathfunc_ceil(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return unary_math(ops, interp, args, "tcl::mathfunc::ceil", FEATHER_MATH_CEIL);
}

FeatherResult feather_builtin_mathfunc_round(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  double arg, result;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::round", &arg) != TCL_OK) {
    return TCL_ERROR;
  }
  if (ops->dbl.math(interp, FEATHER_MATH_ROUND, arg, 0, &result) != TCL_OK) {
    return TCL_ERROR;
  }
  /* TCL round() always returns an integer */
  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)result));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_abs(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  /* TCL abs() returns int for int input, double for double input */
  /* Check if arg is an integer first */
  size_t argc = ops->list.length(interp, args);
  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"tcl::mathfunc::abs value\"", 50);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj arg = ops->list.at(interp, args, 0);
  int64_t ival;
  if (ops->integer.get(interp, arg, &ival) == TCL_OK) {
    /* Integer argument - return integer result */
    if (ival < 0) ival = -ival;
    ops->interp.set_result(interp, ops->integer.create(interp, ival));
    return TCL_OK;
  }
  /* Fall back to double */
  double dval, result;
  if (ops->dbl.get(interp, arg, &dval) != TCL_OK) {
    return TCL_ERROR;
  }
  if (ops->dbl.math(interp, FEATHER_MATH_ABS, dval, 0, &result) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, ops->dbl.create(interp, result));
  return TCL_OK;
}

/* Binary math functions */

FeatherResult feather_builtin_mathfunc_pow(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  return binary_math(ops, interp, args, "tcl::mathfunc::pow", FEATHER_MATH_POW);
}

FeatherResult feather_builtin_mathfunc_atan2(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  return binary_math(ops, interp, args, "tcl::mathfunc::atan2", FEATHER_MATH_ATAN2);
}

FeatherResult feather_builtin_mathfunc_fmod(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  return binary_math(ops, interp, args, "tcl::mathfunc::fmod", FEATHER_MATH_FMOD);
}

FeatherResult feather_builtin_mathfunc_hypot(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  return binary_math(ops, interp, args, "tcl::mathfunc::hypot", FEATHER_MATH_HYPOT);
}

/* Type conversion functions */

FeatherResult feather_builtin_mathfunc_double(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::double", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, ops->dbl.create(interp, val));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_int(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::int", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)val));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_wide(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::wide", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)val));
  return TCL_OK;
}

/* Classification functions */

FeatherResult feather_builtin_mathfunc_isnan(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::isnan", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  FeatherDoubleClass cls = ops->dbl.classify(val);
  int result = (cls == FEATHER_DBL_NAN) ? 1 : 0;
  ops->interp.set_result(interp, ops->integer.create(interp, result));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_isinf(const FeatherHostOps *ops, FeatherInterp interp,
                                             FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::isinf", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  FeatherDoubleClass cls = ops->dbl.classify(val);
  int result = (cls == FEATHER_DBL_INF || cls == FEATHER_DBL_NEG_INF) ? 1 : 0;
  ops->interp.set_result(interp, ops->integer.create(interp, result));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_isfinite(const FeatherHostOps *ops, FeatherInterp interp,
                                                FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double_mathfunc(ops, interp, args, "isfinite", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  FeatherDoubleClass cls = ops->dbl.classify(val);
  /* Finite means not NaN and not infinite */
  int result = (cls != FEATHER_DBL_NAN && cls != FEATHER_DBL_INF && cls != FEATHER_DBL_NEG_INF) ? 1 : 0;
  ops->interp.set_result(interp, ops->integer.create(interp, result));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_isnormal(const FeatherHostOps *ops, FeatherInterp interp,
                                                FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double_mathfunc(ops, interp, args, "isnormal", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  FeatherDoubleClass cls = ops->dbl.classify(val);
  /* Normal means not zero, not subnormal, not infinite, not NaN */
  int result = (cls == FEATHER_DBL_NORMAL) ? 1 : 0;
  ops->interp.set_result(interp, ops->integer.create(interp, result));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_issubnormal(const FeatherHostOps *ops, FeatherInterp interp,
                                                   FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double_mathfunc(ops, interp, args, "issubnormal", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  FeatherDoubleClass cls = ops->dbl.classify(val);
  int result = (cls == FEATHER_DBL_SUBNORMAL) ? 1 : 0;
  ops->interp.set_result(interp, ops->integer.create(interp, result));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_isunordered(const FeatherHostOps *ops, FeatherInterp interp,
                                                   FeatherObj cmd, FeatherObj args) {
  double a, b;
  if (get_two_doubles_mathfunc(ops, interp, args, "isunordered", &a, &b) != TCL_OK) {
    return TCL_ERROR;
  }
  FeatherDoubleClass cls_a = ops->dbl.classify(a);
  FeatherDoubleClass cls_b = ops->dbl.classify(b);
  /* Unordered means either is NaN */
  int result = (cls_a == FEATHER_DBL_NAN || cls_b == FEATHER_DBL_NAN) ? 1 : 0;
  ops->interp.set_result(interp, ops->integer.create(interp, result));
  return TCL_OK;
}

/* Helper: case-insensitive string comparison for boolean parsing */
static int str_equals_ci(const FeatherHostOps *ops, FeatherInterp interp,
                         FeatherObj obj, const char *target, size_t target_len) {
  size_t len = ops->string.byte_length(interp, obj);
  if (len != target_len) return 0;
  for (size_t i = 0; i < len; i++) {
    int c = ops->string.byte_at(interp, obj, i);
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c != target[i]) return 0;
  }
  return 1;
}

FeatherResult feather_builtin_mathfunc_bool(const FeatherHostOps *ops, FeatherInterp interp,
                                            FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "not enough arguments for math function \"bool\"", 45);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  if (argc > 1) {
    FeatherObj msg = ops->string.intern(interp, "too many arguments for math function \"bool\"", 43);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj arg = ops->list.at(interp, args, 0);

  /* Try integer first */
  int64_t ival;
  if (ops->integer.get(interp, arg, &ival) == TCL_OK) {
    ops->interp.set_result(interp, ops->integer.create(interp, ival ? 1 : 0));
    return TCL_OK;
  }

  /* Try double */
  double dval;
  if (ops->dbl.get(interp, arg, &dval) == TCL_OK) {
    ops->interp.set_result(interp, ops->integer.create(interp, dval != 0.0 ? 1 : 0));
    return TCL_OK;
  }

  /* Try boolean string literals (case-insensitive) */
  if (str_equals_ci(ops, interp, arg, "true", 4) ||
      str_equals_ci(ops, interp, arg, "yes", 3) ||
      str_equals_ci(ops, interp, arg, "on", 2)) {
    ops->interp.set_result(interp, ops->integer.create(interp, 1));
    return TCL_OK;
  }
  if (str_equals_ci(ops, interp, arg, "false", 5) ||
      str_equals_ci(ops, interp, arg, "no", 2) ||
      str_equals_ci(ops, interp, arg, "off", 3)) {
    ops->interp.set_result(interp, ops->integer.create(interp, 0));
    return TCL_OK;
  }

  /* Not a valid boolean */
  feather_error_expected(ops, interp, "boolean value", arg);
  return TCL_ERROR;
}

FeatherResult feather_builtin_mathfunc_entier(const FeatherHostOps *ops, FeatherInterp interp,
                                              FeatherObj cmd, FeatherObj args) {
  double val;
  if (get_one_double(ops, interp, args, "tcl::mathfunc::entier", &val) != TCL_OK) {
    return TCL_ERROR;
  }
  /* entier truncates toward zero, same as int for our 64-bit implementation */
  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)val));
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_max(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "not enough arguments for math function \"max\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  /* Track whether we need floating-point result */
  int use_double = 0;
  double max_dbl = 0;
  int64_t max_int = 0;
  int first = 1;

  for (size_t i = 0; i < argc; i++) {
    FeatherObj arg = ops->list.at(interp, args, i);
    int64_t ival;
    double dval;

    /* Try integer first */
    if (!use_double && ops->integer.get(interp, arg, &ival) == TCL_OK) {
      if (first || ival > max_int) {
        max_int = ival;
      }
    } else if (ops->dbl.get(interp, arg, &dval) == TCL_OK) {
      /* Switch to double mode if needed */
      if (!use_double) {
        use_double = 1;
        if (!first) {
          max_dbl = (double)max_int;
        }
      }
      if (first || dval > max_dbl) {
        max_dbl = dval;
      }
    } else {
      /* Conversion failed */
      FeatherObj msg = ops->string.intern(interp, "expected floating-point number", 30);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    first = 0;
  }

  if (use_double) {
    ops->interp.set_result(interp, ops->dbl.create(interp, max_dbl));
  } else {
    ops->interp.set_result(interp, ops->integer.create(interp, max_int));
  }
  return TCL_OK;
}

FeatherResult feather_builtin_mathfunc_min(const FeatherHostOps *ops, FeatherInterp interp,
                                           FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp, "not enough arguments for math function \"min\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  /* Track whether we need floating-point result */
  int use_double = 0;
  double min_dbl = 0;
  int64_t min_int = 0;
  int first = 1;

  for (size_t i = 0; i < argc; i++) {
    FeatherObj arg = ops->list.at(interp, args, i);
    int64_t ival;
    double dval;

    /* Try integer first */
    if (!use_double && ops->integer.get(interp, arg, &ival) == TCL_OK) {
      if (first || ival < min_int) {
        min_int = ival;
      }
    } else if (ops->dbl.get(interp, arg, &dval) == TCL_OK) {
      /* Switch to double mode if needed */
      if (!use_double) {
        use_double = 1;
        if (!first) {
          min_dbl = (double)min_int;
        }
      }
      if (first || dval < min_dbl) {
        min_dbl = dval;
      }
    } else {
      /* Conversion failed */
      FeatherObj msg = ops->string.intern(interp, "expected floating-point number", 30);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    first = 0;
  }

  if (use_double) {
    ops->interp.set_result(interp, ops->dbl.create(interp, min_dbl));
  } else {
    ops->interp.set_result(interp, ops->integer.create(interp, min_int));
  }
  return TCL_OK;
}

/* Usage registration for all mathfunc commands */
void feather_register_mathfunc_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec, e;

  /* sin - Trigonometric sine */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Trigonometric sine function",
    "Returns the sine of arg, where arg is in radians.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Angle in radians");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::sin 0",
    "Returns 0.0 (sine of 0 radians)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::sin", spec);

  /* cos - Trigonometric cosine */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Trigonometric cosine function",
    "Returns the cosine of arg, where arg is in radians.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Angle in radians");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::cos 0",
    "Returns 1.0 (cosine of 0 radians)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::cos", spec);

  /* tan - Trigonometric tangent */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Trigonometric tangent function",
    "Returns the tangent of arg, where arg is in radians.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Angle in radians");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::tan 0",
    "Returns 0.0 (tangent of 0 radians)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::tan", spec);

  /* asin - Arc sine */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Arc sine function",
    "Returns the arc sine of arg in radians. The argument must be in the range [-1, 1].");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value in range [-1, 1]");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::asin 0.5",
    "Returns approximately 0.5236 radians (30 degrees)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::asin", spec);

  /* acos - Arc cosine */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Arc cosine function",
    "Returns the arc cosine of arg in radians. The argument must be in the range [-1, 1].");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value in range [-1, 1]");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::acos 0.5",
    "Returns approximately 1.0472 radians (60 degrees)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::acos", spec);

  /* atan - Arc tangent */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Arc tangent function",
    "Returns the arc tangent of arg in radians.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::atan 1",
    "Returns approximately 0.7854 radians (45 degrees)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::atan", spec);

  /* atan2 - Two-argument arc tangent */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Two-argument arc tangent function",
    "Returns the arc tangent of y/x in radians, using the signs of both arguments "
    "to determine the quadrant of the result. This is useful for converting Cartesian "
    "coordinates to polar coordinates.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  e = feather_usage_help(ops, interp, e, "Y coordinate");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<x>");
  e = feather_usage_help(ops, interp, e, "X coordinate");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::atan2 1 1",
    "Returns approximately 0.7854 radians (45 degrees)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::atan2", spec);

  /* sinh - Hyperbolic sine */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Hyperbolic sine function",
    "Returns the hyperbolic sine of arg.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::sinh 0",
    "Returns 0.0 (hyperbolic sine of 0)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::sinh", spec);

  /* cosh - Hyperbolic cosine */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Hyperbolic cosine function",
    "Returns the hyperbolic cosine of arg.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::cosh 0",
    "Returns 1.0 (hyperbolic cosine of 0)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::cosh", spec);

  /* tanh - Hyperbolic tangent */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Hyperbolic tangent function",
    "Returns the hyperbolic tangent of arg.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::tanh 0",
    "Returns 0.0 (hyperbolic tangent of 0)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::tanh", spec);

  /* exp - Exponential */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Exponential function",
    "Returns e raised to the power of arg, where e is the base of natural logarithms "
    "(approximately 2.71828).");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Exponent value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::exp 1",
    "Returns approximately 2.71828 (e to the power of 1)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::exp", spec);

  /* log - Natural logarithm */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Natural logarithm function",
    "Returns the natural logarithm (base e) of arg. The argument must be positive.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Positive numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::log 2.71828",
    "Returns approximately 1.0 (natural log of e)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::log", spec);

  /* log10 - Base-10 logarithm */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Base-10 logarithm function",
    "Returns the base-10 logarithm of arg. The argument must be positive.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Positive numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::log10 100",
    "Returns 2.0 (10 to the power of 2 is 100)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::log10", spec);

  /* sqrt - Square root */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Square root function",
    "Returns the square root of arg. The argument must be non-negative.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Non-negative numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::sqrt 16",
    "Returns 4.0 (square root of 16)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::sqrt", spec);

  /* pow - Power function */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Power function",
    "Returns x raised to the power of y.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<x>");
  e = feather_usage_help(ops, interp, e, "Base value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  e = feather_usage_help(ops, interp, e, "Exponent value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::pow 2 8",
    "Returns 256.0 (2 to the power of 8)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::pow", spec);

  /* floor - Floor function */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Floor function",
    "Returns the largest integer value not greater than arg (rounds down).");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::floor 3.7",
    "Returns 3.0 (largest integer not greater than 3.7)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::floor", spec);

  /* ceil - Ceiling function */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Ceiling function",
    "Returns the smallest integer value not less than arg (rounds up).");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::ceil 3.2",
    "Returns 4.0 (smallest integer not less than 3.2)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::ceil", spec);

  /* round - Round to nearest integer */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Round to nearest integer",
    "Returns the integer value closest to arg. If arg is halfway between two integers, "
    "rounds to the even integer. Returns an integer type (not a floating-point number).");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::round 3.7",
    "Returns 4 (nearest integer to 3.7)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::round", spec);

  /* abs - Absolute value */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Absolute value function",
    "Returns the absolute value of arg. Preserves the input type: integer input returns "
    "integer, floating-point input returns floating-point.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Numeric value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::abs -5",
    "Returns 5 (absolute value of -5)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::abs", spec);

  /* fmod - Floating-point remainder */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Floating-point remainder function",
    "Returns the floating-point remainder of x divided by y. The result has the same "
    "sign as x.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<x>");
  e = feather_usage_help(ops, interp, e, "Dividend value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  e = feather_usage_help(ops, interp, e, "Divisor value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::fmod 7.5 2.0",
    "Returns 1.5 (remainder of 7.5 divided by 2.0)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::fmod", spec);

  /* hypot - Hypotenuse calculation */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Hypotenuse calculation",
    "Returns the square root of (x*x + y*y), computed in a way that avoids overflow. "
    "This is the length of the hypotenuse of a right triangle with sides of length x and y.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<x>");
  e = feather_usage_help(ops, interp, e, "First side length");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  e = feather_usage_help(ops, interp, e, "Second side length");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::hypot 3 4",
    "Returns 5.0 (hypotenuse of right triangle with sides 3 and 4)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::hypot", spec);

  /* double - Convert to floating-point */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Convert to floating-point",
    "Converts arg to a floating-point number and returns it.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to convert");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::double 42",
    "Returns 42.0 (floating-point representation)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::double", spec);

  /* int - Convert to integer */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Convert to integer",
    "Converts arg to an integer by truncating toward zero and returns it. "
    "In Feather, this produces a 64-bit integer.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to convert");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::int 3.7",
    "Returns 3 (truncated toward zero)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::int", spec);

  /* wide - Convert to 64-bit integer */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Convert to 64-bit integer",
    "Converts arg to a 64-bit integer by truncating toward zero and returns it. "
    "In Feather, this is equivalent to int() as all integers are 64-bit.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to convert");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::wide 3.7",
    "Returns 3 (truncated toward zero)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::wide", spec);

  /* isnan - Test if Not-a-Number */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Test if Not-a-Number",
    "Returns 1 if arg is a floating-point NaN (Not-a-Number) value, 0 otherwise.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::isnan [expr {0.0/0.0}]",
    "Returns 1 (result of 0/0 is NaN)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::isnan", spec);

  /* isinf - Test if infinite */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Test if infinite",
    "Returns 1 if arg is a floating-point infinity (positive or negative), 0 otherwise.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::isinf [expr {1.0/0.0}]",
    "Returns 1 (result of 1/0 is infinity)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::isinf", spec);

  /* isfinite - Test if finite */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Test if finite",
    "Returns 1 if arg is a finite floating-point number (not NaN and not infinite), "
    "0 otherwise. Finite numbers include zero, subnormal, and normal numbers.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::isfinite 3.14",
    "Returns 1 (3.14 is a finite number)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::isfinite", spec);

  /* isnormal - Test if normal */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Test if normal",
    "Returns 1 if arg is a normal floating-point number (not zero, not subnormal, "
    "not infinite, and not NaN), 0 otherwise.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::isnormal 3.14",
    "Returns 1 (3.14 is a normal number)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::isnormal", spec);

  /* issubnormal - Test if subnormal */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Test if subnormal",
    "Returns 1 if arg is a subnormal (denormalized) floating-point number, 0 otherwise. "
    "Subnormal numbers are very small numbers close to zero that have reduced precision.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::issubnormal 1e-320",
    "Returns 1 (very small number is subnormal)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::issubnormal", spec);

  /* isunordered - Test if either is NaN */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Test if either value is NaN",
    "Returns 1 if either x or y is NaN (Not-a-Number), 0 otherwise. This is useful "
    "for checking if a comparison between two values would be unordered.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<x>");
  e = feather_usage_help(ops, interp, e, "First value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  e = feather_usage_help(ops, interp, e, "Second value to test");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::isunordered 3.14 [expr {0.0/0.0}]",
    "Returns 1 (second argument is NaN)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::isunordered", spec);

  /* bool - Convert to boolean */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Convert to boolean",
    "Converts arg to a boolean value (0 or 1). Accepts numeric values (0 is false, "
    "non-zero is true) and boolean string literals: \"true\", \"false\", \"yes\", \"no\", "
    "\"on\", \"off\" (case-insensitive).");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to convert");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::bool yes",
    "Returns 1 (\"yes\" converts to true)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::bool", spec);

  /* entier - Convert to integer (same as int in Feather) */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Convert to integer",
    "Converts arg to an integer by truncating toward zero. In standard TCL, this "
    "provides arbitrary-precision integer conversion, but Feather uses 64-bit integers, "
    "so this is equivalent to int().");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "Value to convert");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::entier 3.7",
    "Returns 3 (truncated toward zero)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::entier", spec);

  /* max - Return maximum value */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Return maximum value",
    "Returns the maximum of one or more numeric arguments. Preserves integer type "
    "if all arguments are integers, otherwise returns floating-point.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "First value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e, "Additional values to compare");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::max 5 12 8",
    "Returns 12 (maximum of 5, 12, and 8)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::max", spec);

  /* min - Return minimum value */
  spec = feather_usage_spec(ops, interp);
  e = feather_usage_about(ops, interp,
    "Return minimum value",
    "Returns the minimum of one or more numeric arguments. Preserves integer type "
    "if all arguments are integers, otherwise returns floating-point.");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "<arg>");
  e = feather_usage_help(ops, interp, e, "First value");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_arg(ops, interp, "?arg?...");
  e = feather_usage_help(ops, interp, e, "Additional values to compare");
  spec = feather_usage_add(ops, interp, spec, e);
  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::min 5 12 8",
    "Returns 5 (minimum of 5, 12, and 8)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);
  feather_usage_register(ops, interp, "tcl::mathfunc::min", spec);
}
