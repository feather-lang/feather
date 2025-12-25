# Plan: Full Floating Point Support via Host Ops

## Goal

Add comprehensive floating-point support by extending `FeatherDoubleOps` with classification, formatting, and math operations, enabling proper IEEE 754 compliance with minimal host burden.

**Current state:**
- `FeatherDoubleOps` only has `create` and `get` — no special value handling
- `builtin_mathfunc.c` uses integer approximations (e.g., lookup tables for `exp`)
- `builtin_format.c` cannot detect `Inf` or `NaN` (no stdlib access)
- Math functions like `sin`, `cos`, `sqrt`, `log`, `pow` are not implemented

**Desired end state:**
- `FeatherDoubleOps` provides 3 additional operations: `classify`, `format`, `math`
- Host can detect special values (`Inf`, `-Inf`, `NaN`) via `classify`
- Host handles all transcendental math via single `math` dispatcher
- All standard TCL math functions work via `tcl::mathfunc::*`
- `format` command handles `%e`, `%f`, `%g` specifiers including special values

**Design principle:**
- C can do basic double arithmetic (`+`, `-`, `*`, `/`) natively
- Host is only needed for: classification (isnan/isinf), formatting special values, transcendentals (libm)
- Single `math` dispatcher avoids explosion of function pointers

**Files involved:**
- `src/feather.h` — extend `FeatherDoubleOps` with 3 new ops + 2 enums
- `src/builtin_mathfunc.c` — rewrite to use `ops->dbl.math`
- `src/builtin_format.c` — use `ops->dbl.format` for special values
- `src/builtin_expr.c` — use `ops->dbl.classify` for Inf/NaN detection
- `interp/interp.go` — implement new double ops in Go host
- `js/feather.js` — implement new double ops in JS host

**Benefits:**
- Full IEEE 754 floating point support
- Only 3 new function pointers (minimal host burden)
- Extensible via enum (new math ops don't require API changes)

---

## M1: Extend FeatherDoubleOps in feather.h

Add enums and 3 new operations to `FeatherDoubleOps`.

**Tasks:**

1. Add classification result enum:
   ```c
   typedef enum FeatherDoubleClass {
     FEATHER_DBL_NORMAL    = 0,   // Finite, non-zero
     FEATHER_DBL_ZERO      = 1,   // Positive or negative zero
     FEATHER_DBL_INF       = 2,   // Positive infinity
     FEATHER_DBL_NEG_INF   = 3,   // Negative infinity
     FEATHER_DBL_NAN       = 4,   // Not a number
   } FeatherDoubleClass;
   ```

2. Add math operation enum:
   ```c
   typedef enum FeatherMathOp {
     // Unary operations (use 'a' parameter, ignore 'b')
     FEATHER_MATH_SQRT,
     FEATHER_MATH_EXP,
     FEATHER_MATH_LOG,
     FEATHER_MATH_LOG10,
     FEATHER_MATH_SIN,
     FEATHER_MATH_COS,
     FEATHER_MATH_TAN,
     FEATHER_MATH_ASIN,
     FEATHER_MATH_ACOS,
     FEATHER_MATH_ATAN,
     FEATHER_MATH_SINH,
     FEATHER_MATH_COSH,
     FEATHER_MATH_TANH,
     FEATHER_MATH_FLOOR,
     FEATHER_MATH_CEIL,
     FEATHER_MATH_ROUND,
     FEATHER_MATH_ABS,
     
     // Binary operations (use both 'a' and 'b')
     FEATHER_MATH_POW,
     FEATHER_MATH_ATAN2,
     FEATHER_MATH_FMOD,
     FEATHER_MATH_HYPOT,
   } FeatherMathOp;
   ```

3. Extend `FeatherDoubleOps`:
   ```c
   typedef struct FeatherDoubleOps {
     // Existing
     FeatherObj (*create)(FeatherInterp interp, double val);
     FeatherResult (*get)(FeatherInterp interp, FeatherObj obj, double *out);
     
     // NEW: Classify a double value
     FeatherDoubleClass (*classify)(double val);
     
     // NEW: Format to string, handling special values (Inf, NaN)
     FeatherObj (*format)(FeatherInterp interp, double val, char specifier, int precision);
     
     // NEW: Compute math operation, returns TCL_ERROR for domain errors
     FeatherResult (*math)(FeatherInterp interp, FeatherMathOp op,
                          double a, double b, double *out);
   } FeatherDoubleOps;
   ```

**Verification:** Header compiles cleanly.

---

## M2: Implement new FeatherDoubleOps in Go host

Add implementations for the 3 new operations in `interp/interp.go`.

**Tasks:**

1. Implement `classify`:
   ```go
   func dblClassify(val float64) int {
     if math.IsNaN(val) { return 4 }  // FEATHER_DBL_NAN
     if math.IsInf(val, 1) { return 2 }  // FEATHER_DBL_INF
     if math.IsInf(val, -1) { return 3 }  // FEATHER_DBL_NEG_INF
     if val == 0 { return 1 }  // FEATHER_DBL_ZERO
     return 0  // FEATHER_DBL_NORMAL
   }
   ```

2. Implement `format`:
   ```go
   func (i *Interp) dblFormat(val float64, spec byte, prec int) *Obj {
     switch i.dblClassify(val) {
     case 4: return i.intern("NaN")
     case 2: return i.intern("Inf")
     case 3: return i.intern("-Inf")
     }
     // Normal formatting with strconv.FormatFloat
   }
   ```

3. Implement `math` dispatcher:
   ```go
   func (i *Interp) dblMath(op int, a, b float64) (float64, error) {
     switch op {
     case FEATHER_MATH_SQRT: return math.Sqrt(a), nil
     case FEATHER_MATH_SIN: return math.Sin(a), nil
     case FEATHER_MATH_POW: return math.Pow(a, b), nil
     // ... all ops
     }
   }
   ```

**Verification:** `mise test` passes.

---

## M3: Implement new FeatherDoubleOps in JS host

Add implementations in `js/feather.js`.

**Tasks:**

1. Implement `classify`:
   ```javascript
   function dblClassify(val) {
     if (Number.isNaN(val)) return 4;
     if (val === Infinity) return 2;
     if (val === -Infinity) return 3;
     if (val === 0) return 1;
     return 0;
   }
   ```

2. Implement `format` with special value handling

3. Implement `math` dispatcher using `Math.*` functions

**Verification:** `mise test:js` passes.

---

## M4: Rewrite builtin_mathfunc.c to use host ops

Replace integer approximations with `ops->dbl.math`.

**Tasks:**

1. Rewrite `tcl::mathfunc::exp`:
   ```c
   FeatherResult feather_builtin_mathfunc_exp(...) {
     double arg, result;
     if (ops->dbl.get(interp, argObj, &arg) != TCL_OK) { /* error */ }
     if (ops->dbl.math(interp, FEATHER_MATH_EXP, arg, 0, &result) != TCL_OK) {
       return TCL_ERROR;
     }
     ops->interp.set_result(interp, ops->dbl.create(interp, result));
     return TCL_OK;
   }
   ```

2. Add new math function builtins using the same pattern:
   - `sqrt`, `pow`, `log`, `log10`
   - `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
   - `sinh`, `cosh`, `tanh`
   - `floor`, `ceil`, `round`
   - `abs`/`fabs`, `fmod`, `hypot`
   - `double`, `int`, `wide` (type conversions)
   - `isnan`, `isinf` (using `ops->dbl.classify`)

3. Register all in `feather_interp_init`

4. Delete the old integer lookup table code

**Verification:** `expr {sin(3.14159/2)}` returns ~1.0.

---

## M5: Update builtin_format.c for special values

Use `ops->dbl.format` for floating-point formatting.

**Tasks:**

1. Replace `double_to_str` with host delegation:
   ```c
   // For %e, %f, %g specifiers:
   formatted = ops->dbl.format(interp, dblVal, spec.specifier, precision);
   ```

2. Remove manual floating-point formatting code (~150 lines)

**Verification:** `format %f [expr {1.0/0.0}]` returns "Inf".

---

## M6: Update builtin_expr.c for Inf/NaN handling

Use `ops->dbl.classify` to detect and propagate special values.

**Tasks:**

1. After division, check for special results:
   ```c
   double result = a / b;
   FeatherDoubleClass cls = ops->dbl.classify(result);
   if (cls == FEATHER_DBL_NAN || cls == FEATHER_DBL_INF || cls == FEATHER_DBL_NEG_INF) {
     // Return as-is (TCL allows Inf/NaN in expressions)
   }
   ```

2. In exponentiation, use `ops->dbl.math(FEATHER_MATH_POW, ...)` for non-integer exponents

3. Ensure special values propagate correctly through expressions

**Verification:** `expr {1.0/0}` → `Inf`. `expr {0.0/0}` → `NaN`.

---

## M7: Add test cases

Create test cases for floating-point behavior.

**Tasks:**

1. Add `testcases/expr-float.html`:
   - Basic arithmetic: `expr {1.5 + 2.5}` → `4.0`
   - Division producing Inf: `expr {1.0/0}` → `Inf`
   - NaN propagation: `expr {0.0/0.0 + 1}` → `NaN`

2. Add `testcases/mathfunc.html`:
   - Trig functions: `expr {sin(0)}` → `0.0`
   - Exponential: `expr {exp(1)}` → `2.718...`
   - Edge cases: `expr {sqrt(-1)}` → `NaN`

3. Add `testcases/format-float.html`:
   - `format %.2f 3.14159` → `3.14`
   - `format %e [expr {1.0/0}]` → `Inf`

**Verification:** All test cases pass in both Go and JS hosts.

---

## M8: Documentation and cleanup

**Tasks:**

1. Document the enums and new ops in `src/feather.h`
2. Update `WASM.md` for the 3 new imports
3. Ensure `mise build`, `mise test`, `mise test:js` all pass

**Verification:** All tests pass. Documentation is accurate.
