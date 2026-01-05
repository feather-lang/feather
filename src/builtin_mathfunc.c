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

/* Usage registration for all mathfunc commands - structured as subcommands */
void feather_register_mathfunc_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Mathematical functions for Tcl expressions",
    "The expr command handles mathematical functions of the form sin($x) or "
    "atan2($y,$x) by converting them to calls of the form "
    "[tcl::mathfunc::sin [expr {$x}]] or [tcl::mathfunc::atan2 [expr {$y}] "
    "[expr {$x}]]. These functions are available both within expr and by "
    "invoking the given commands directly.\n\n"
    "All functions work with floating-point numbers unless otherwise noted. "
    "Type conversion functions (int, wide, double, entier) and comparison "
    "functions (max, min) preserve integer types when appropriate.\n\n"
    "Note: Feather does not implement rand(), srand(), or isqrt() as these "
    "require features outside Feather's scope (random number generation and "
    "arbitrary precision integers).");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- abs --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "abs", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the absolute value of arg. Arg may be either integer or "
    "floating-point, and the result is returned in the same form.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- acos --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "acos", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the arc cosine of arg, in the range [0,pi] radians. Arg should "
    "be in the range [-1,1].");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- asin --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "asin", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the arc sine of arg, in the range [-pi/2,pi/2] radians. Arg "
    "should be in the range [-1,1].");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- atan --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "atan", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the arc tangent of arg, in the range [-pi/2,pi/2] radians.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- atan2 --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<y>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<x>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "atan2", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the arc tangent of y/x, in the range [-pi,pi] radians. x and y "
    "cannot both be 0. If x is greater than 0, this is equivalent to "
    "\"atan [expr {y/x}]\".");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- bool --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "bool", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Accepts any numeric value, or any string acceptable to string is boolean, "
    "and returns the corresponding boolean value 0 or 1. Non-zero numbers are "
    "true. Other numbers are false. Non-numeric strings produce boolean value "
    "in agreement with string is true and string is false.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- ceil --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "ceil", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the smallest integral floating-point value (i.e. with a zero "
    "fractional part) not less than arg. The argument may be any numeric value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- cos --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "cos", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the cosine of arg, measured in radians.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- cosh --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "cosh", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the hyperbolic cosine of arg. If the result would cause an "
    "overflow, an error is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- double --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "double", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "The argument may be any numeric value. If arg is a floating-point value, "
    "returns arg, otherwise converts arg to floating-point and returns the "
    "converted value. May return Inf or -Inf when the argument is a numeric "
    "value that exceeds the floating-point range.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- entier --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "entier", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "The argument may be any numeric value. The integer part of arg is "
    "determined and returned. In standard TCL, the integer range returned by "
    "this function is unlimited (arbitrary precision), but Feather uses 64-bit "
    "integers, so this is equivalent to int().");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- exp --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "exp", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the exponential of arg, defined as e**arg. If the result would "
    "cause an overflow, an error is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- floor --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "floor", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the largest integral floating-point value (i.e. with a zero "
    "fractional part) not greater than arg. The argument may be any numeric value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- fmod --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<x>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "fmod", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the floating-point remainder of the division of x by y. If y is "
    "0, an error is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- hypot --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<x>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "hypot", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Computes the length of the hypotenuse of a right-angled triangle, "
    "approximately \"sqrt [expr {x*x+y*y}]\" except for being more numerically "
    "stable when the two arguments have substantially different magnitudes.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- int --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "int", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "The argument may be any numeric value. The integer part of arg is "
    "determined, and then the low order bits of that integer value up to the "
    "machine word size are returned as an integer value. In Feather, all "
    "integers are 64-bit, so this is equivalent to wide().");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- isfinite --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "isfinite", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if the floating-point number arg is finite. That is, if it is "
    "zero, subnormal, or normal. Returns 0 if the number is infinite or NaN. "
    "Throws an error if arg cannot be promoted to a floating-point value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- isinf --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "isinf", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if the floating-point number arg is infinite. Returns 0 if the "
    "number is finite or NaN. Throws an error if arg cannot be promoted to a "
    "floating-point value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- isnan --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "isnan", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if the floating-point number arg is Not-a-Number. Returns 0 if "
    "the number is finite or infinite. Throws an error if arg cannot be "
    "promoted to a floating-point value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- isnormal --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "isnormal", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if the floating-point number arg is normal. Returns 0 if the "
    "number is zero, subnormal, infinite or NaN. Throws an error if arg cannot "
    "be promoted to a floating-point value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- issubnormal --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "issubnormal", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if the floating-point number arg is subnormal, i.e., the result "
    "of gradual underflow. Returns 0 if the number is zero, normal, infinite "
    "or NaN. Throws an error if arg cannot be promoted to a floating-point value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- isunordered --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<x>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "isunordered", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns 1 if x and y cannot be compared for ordering, that is, if either "
    "one is NaN. Returns 0 if both values can be ordered, that is, if they are "
    "both chosen from among the set of zero, subnormal, normal and infinite "
    "values. Throws an error if either x or y cannot be promoted to a "
    "floating-point value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- log --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "log", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the natural logarithm of arg. Arg must be a positive value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- log10 --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "log10", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the base 10 logarithm of arg. Arg must be a positive value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- max --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?arg?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "max", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Accepts one or more numeric arguments. Returns the one argument with the "
    "greatest value. Preserves integer type if all arguments are integers, "
    "otherwise returns floating-point.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- min --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?arg?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "min", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Accepts one or more numeric arguments. Returns the one argument with the "
    "least value. Preserves integer type if all arguments are integers, "
    "otherwise returns floating-point.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- pow --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<x>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<y>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "pow", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Computes the value of x raised to the power y. If x is negative, y must "
    "be an integer value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- round --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "round", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "If arg is an integer value, returns arg, otherwise converts arg to integer "
    "by rounding and returns the converted value.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- sin --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "sin", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the sine of arg, measured in radians.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- sinh --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "sinh", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the hyperbolic sine of arg. If the result would cause an overflow, "
    "an error is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- sqrt --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "sqrt", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "The argument may be any non-negative numeric value. Returns a floating-point "
    "value that is the square root of arg. May return Inf when the argument is "
    "a numeric value that exceeds the square of the maximum value of the "
    "floating-point range.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- tan --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "tan", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the tangent of arg, measured in radians.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- tanh --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "tanh", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "Returns the hyperbolic tangent of arg.");
  spec = feather_usage_add(ops, interp, spec, e);

  /* --- wide --- */
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<arg>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "wide", subspec);
  e = feather_usage_long_help(ops, interp, e,
    "The argument may be any numeric value. The integer part of arg is "
    "determined, and then the low order 64 bits of that integer value are "
    "returned as an integer value. In Feather, all integers are 64-bit, "
    "so this is equivalent to int().");
  spec = feather_usage_add(ops, interp, spec, e);

  /* Add examples */
  e = feather_usage_example(ops, interp,
    "expr {sin(0.5)}",
    "Use sin within an expression (returns approximately 0.479)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "tcl::mathfunc::sqrt 16",
    "Call sqrt directly (returns 4.0)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "expr {max(1, 5, 3)}",
    "Find maximum of multiple values (returns 5)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  /* Add SEE ALSO section */
  e = feather_usage_section(ops, interp, "See Also",
    "expr");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "tcl::mathfunc", spec);
}
