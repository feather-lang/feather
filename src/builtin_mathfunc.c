#include "feather.h"
#include "internal.h"

/**
 * tcl::mathfunc::exp - Integer exponential function
 *
 * Returns floor(e^arg) for integer argument.
 * Since we don't support floating point, this is an integer approximation.
 *
 * Usage: tcl::mathfunc::exp arg
 */
FeatherResult feather_builtin_mathfunc_exp(const FeatherHostOps *ops, FeatherInterp interp,
                                    FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  if (argc != 1) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"tcl::mathfunc::exp value\"", 50);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj arg = ops->list.at(interp, args, 0);
  int64_t n;
  if (ops->integer.get(interp, arg, &n) != TCL_OK) {
    size_t len;
    const char *str = ops->string.get(interp, arg, &len);
    FeatherObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
    FeatherObj part2 = ops->string.intern(interp, str, len);
    FeatherObj part3 = ops->string.intern(interp, "\"", 1);
    FeatherObj msg = ops->string.concat(interp, part1, part2);
    msg = ops->string.concat(interp, msg, part3);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Compute floor(e^n) for integer n
  // Using lookup table for common values, iterative for others
  // e ≈ 2.718281828...
  int64_t result;

  if (n < 0) {
    // e^(-n) < 1 for n > 0, so floor is 0
    result = 0;
  } else if (n == 0) {
    result = 1;  // e^0 = 1
  } else if (n <= 20) {
    // Lookup table for floor(e^n) for n = 1..20
    static const int64_t exp_table[] = {
      2,           // e^1 = 2.718...
      7,           // e^2 = 7.389...
      20,          // e^3 = 20.085...
      54,          // e^4 = 54.598...
      148,         // e^5 = 148.413...
      403,         // e^6 = 403.428...
      1096,        // e^7 = 1096.633...
      2980,        // e^8 = 2980.957...
      8103,        // e^9 = 8103.083...
      22026,       // e^10 = 22026.465...
      59874,       // e^11 = 59874.141...
      162754,      // e^12 = 162754.791...
      442413,      // e^13 = 442413.392...
      1202604,     // e^14 = 1202604.284...
      3269017,     // e^15 = 3269017.372...
      8886110,     // e^16 = 8886110.520...
      24154952,    // e^17 = 24154952.753...
      65659969,    // e^18 = 65659969.137...
      178482300,   // e^19 = 178482300.963...
      485165195    // e^20 = 485165195.409...
    };
    result = exp_table[n - 1];
  } else {
    // For larger values, use iterative multiplication
    // e^n ≈ (e^10)^(n/10) * e^(n%10)
    // But this can overflow quickly, so cap at e^43 (largest that fits in int64)
    if (n > 43) {
      // Overflow - return max int64
      result = 9223372036854775807LL;
    } else {
      // Iterative approximation using e ≈ 2718/1000
      // Actually, let's just extend the table calculation
      result = 485165195;  // e^20
      for (int64_t i = 20; i < n; i++) {
        // Multiply by e ≈ 2.718 using integer math: result = result * 2718 / 1000
        result = result * 2718 / 1000;
      }
    }
  }

  FeatherObj result_obj = ops->integer.create(interp, result);
  ops->interp.set_result(interp, result_obj);
  return TCL_OK;
}
