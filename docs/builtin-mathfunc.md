# Feather mathfunc Implementation

This document compares Feather's implementation of `tcl::mathfunc::*` functions
with the standard TCL 8.5+ implementation.

## Summary of Our Implementation

Feather implements math functions in `src/builtin_mathfunc.c`. The implementation
provides:

- Unary math functions via a helper that extracts one double argument
- Binary math functions via a helper that extracts two double arguments
- Type conversion functions (int, double, wide)
- Classification functions (isnan, isinf)

All math operations are delegated to the host through `FeatherHostOps` using
the `FEATHER_MATH_*` operation codes.

## TCL Features We Support

### Trigonometric Functions
- `sin(arg)` - sine in radians
- `cos(arg)` - cosine in radians
- `tan(arg)` - tangent in radians
- `asin(arg)` - arc sine
- `acos(arg)` - arc cosine
- `atan(arg)` - arc tangent
- `atan2(y, x)` - two-argument arc tangent

### Hyperbolic Functions
- `sinh(arg)` - hyperbolic sine
- `cosh(arg)` - hyperbolic cosine
- `tanh(arg)` - hyperbolic tangent

### Exponential and Logarithmic Functions
- `exp(arg)` - exponential (e^arg)
- `log(arg)` - natural logarithm
- `log10(arg)` - base-10 logarithm
- `sqrt(arg)` - square root
- `pow(x, y)` - power function

### Rounding Functions
- `floor(arg)` - largest integer not greater than arg
- `ceil(arg)` - smallest integer not less than arg
- `round(arg)` - round to nearest integer (returns integer type)
- `abs(arg)` - absolute value (preserves int/double type)

### Other Math Functions
- `fmod(x, y)` - floating-point remainder
- `hypot(x, y)` - hypotenuse calculation

### Type Conversion Functions
- `double(arg)` - convert to floating-point
- `int(arg)` - convert to machine-word-sized integer
- `wide(arg)` - convert to 64-bit integer

### Classification Functions
- `isnan(arg)` - test if Not-a-Number
- `isinf(arg)` - test if infinite

## TCL Features We Do NOT Support

### Missing Math Functions

| Function | Description |
|----------|-------------|
| `bool(arg)` | Convert to boolean (0 or 1). Accepts numeric values and boolean strings like "true", "false", "yes", "no". |
| `entier(arg)` | Convert to arbitrary-precision integer. Unlike `int` and `wide`, has unlimited range. |
| `isqrt(arg)` | Integer square root with arbitrary precision. Returns exact integer part of sqrt. |
| `max(arg, ...)` | Return maximum of one or more numeric arguments. Variadic function. |
| `min(arg, ...)` | Return minimum of one or more numeric arguments. Variadic function. |
| `rand()` | Return pseudo-random float in range (0,1). Zero-argument function. |
| `srand(arg)` | Seed the random number generator. Returns first random number from that seed. |

### Missing Classification Functions

| Function | Description |
|----------|-------------|
| `isfinite(arg)` | Returns 1 if arg is zero, subnormal, or normal (not infinite, not NaN). |
| `isnormal(arg)` | Returns 1 if arg is a normal floating-point number (not zero, subnormal, infinite, or NaN). |
| `issubnormal(arg)` | Returns 1 if arg is a subnormal (denormalized) floating-point number. |
| `isunordered(x, y)` | Returns 1 if either x or y is NaN (cannot be ordered). Binary function. |

## Notes on Implementation Differences

### `abs(arg)` Type Preservation
Our implementation correctly preserves the input type: integer input returns integer,
double input returns double. This matches TCL behavior.

### `round(arg)` Return Type
Our implementation returns an integer type, which matches TCL behavior where `round`
always produces an integer result.

### `int(arg)` vs `wide(arg)`
In our implementation, both `int` and `wide` produce 64-bit integers. In standard TCL:
- `int` truncates to machine word size (32 or 64 bits depending on platform)
- `wide` always produces 64-bit integers

This difference is unlikely to matter on 64-bit platforms.

### `entier(arg)` vs `int(arg)`
TCL's `entier` function provides arbitrary-precision integer conversion, which is
important for bignum support. Our implementation lacks this, so very large numbers
may lose precision when converted to integers.

### Random Number Generation
TCL's `rand()` and `srand()` functions provide per-interpreter random number
generation. These are not implemented in Feather.

### Variadic Functions
TCL's `max` and `min` accept any number of arguments (1 or more). Implementing
these would require different argument handling than the current unary/binary
helper pattern.

### Boolean Conversion
TCL's `bool` function accepts not just numeric values but also boolean strings
like "true", "false", "yes", "no", "on", "off". This requires string parsing
logic beyond simple numeric conversion.
