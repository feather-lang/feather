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
  FeatherObj part1 = ops->string.intern(interp, "expected boolean value but got \"", 32);
  FeatherObj part2 = ops->string.intern(interp, "\"", 1);
  FeatherObj msg = ops->string.concat(interp, part1, arg);
  msg = ops->string.concat(interp, msg, part2);
  ops->interp.set_result(interp, msg);
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
