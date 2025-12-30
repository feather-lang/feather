# expr - Expression Evaluation

## Summary of Our Implementation

Feather implements a comprehensive expression evaluator in `src/builtin_expr.c`. The implementation uses a recursive descent parser with proper operator precedence and supports:

- Full arithmetic operations with integer and floating-point numbers
- Boolean and comparison operators
- Bitwise operations
- Variable substitution (`$var`, `${var}`)
- Command substitution (`[cmd args]`)
- Ternary conditional operator (`?:`)
- Math function calls (via `tcl::mathfunc::` namespace)
- Short-circuit evaluation for `&&`, `||`, and `?:`
- Comments starting with `#`

## TCL Features We Support

### Operands

- **Numeric literals**: integers (decimal, hex `0x`, binary `0b`, octal `0o`), floating-point (`3.14`, `.5`, `1e10`, `3.14e-5`)
- **Digit separators**: underscores in numeric literals (`100_000_000`, `0xffff_ffff`)
- **Boolean literals**: `true`, `false`, `yes`, `no`, `on`, `off` (case-insensitive)
- **Special float values**: `Inf`, `-Inf`, `NaN` recognized
- **Variables**: `$name`, `${name}`, including namespace-qualified names
- **Command substitution**: `[command args]`
- **Braced strings**: `{literal string}`
- **Quoted strings**: `"string with $var and [cmd] substitution"`
- **Parenthesized expressions**: `(expr)`
- **Comments**: `#` to end of line

### Operators (in precedence order, lowest to highest)

| Operator(s) | Description | Supported |
|-------------|-------------|-----------|
| `?:` | Ternary conditional (right-to-left) | Yes |
| `\|\|` | Logical OR (short-circuit) | Yes |
| `&&` | Logical AND (short-circuit) | Yes |
| `\|` | Bitwise OR | Yes |
| `^` | Bitwise XOR | Yes |
| `&` | Bitwise AND | Yes |
| `==` `!=` | Numeric equality (with string fallback) | Yes |
| `eq` `ne` | String equality | Yes |
| `<` `>` `<=` `>=` | Numeric comparison (with string fallback) | Yes |
| `lt` `gt` `le` `ge` | String comparison | Yes |
| `in` `ni` | List containment | Yes |
| `<<` `>>` | Bitwise shift | Yes |
| `+` `-` | Addition, subtraction | Yes |
| `*` `/` `%` | Multiplication, division, modulo | Yes |
| `**` | Exponentiation (right-to-left) | Yes |
| `-` `+` `~` `!` | Unary minus, plus, bitwise NOT, logical NOT | Yes |

### Math Functions (via `tcl::mathfunc::`)

| Function | Supported |
|----------|-----------|
| `abs(x)` | Yes |
| `acos(x)` | Yes |
| `asin(x)` | Yes |
| `atan(x)` | Yes |
| `atan2(y, x)` | Yes |
| `ceil(x)` | Yes |
| `cos(x)` | Yes |
| `cosh(x)` | Yes |
| `double(x)` | Yes |
| `exp(x)` | Yes |
| `floor(x)` | Yes |
| `fmod(x, y)` | Yes |
| `hypot(x, y)` | Yes |
| `int(x)` | Yes |
| `isinf(x)` | Yes |
| `isnan(x)` | Yes |
| `log(x)` | Yes |
| `log10(x)` | Yes |
| `pow(x, y)` | Yes |
| `round(x)` | Yes |
| `sin(x)` | Yes |
| `sinh(x)` | Yes |
| `sqrt(x)` | Yes |
| `tan(x)` | Yes |
| `tanh(x)` | Yes |
| `wide(x)` | Yes |

## TCL Features We Do NOT Support

### Missing Math Functions

| Function | Description |
|----------|-------------|
| `bool(arg)` | Convert to boolean (0 or 1) |
| `entier(x)` | Integer part with unlimited precision (differs from `int` which truncates to machine word) |
| `isfinite(x)` | Returns 1 if finite (zero, subnormal, or normal) |
| `isnormal(x)` | Returns 1 if normal (not zero, subnormal, infinite, or NaN) |
| `isqrt(x)` | Integer square root with arbitrary precision |
| `issubnormal(x)` | Returns 1 if subnormal (gradual underflow) |
| `isunordered(x, y)` | Returns 1 if either is NaN (cannot be ordered) |
| `max(arg, ...)` | Maximum of multiple arguments |
| `min(arg, ...)` | Minimum of multiple arguments |
| `rand()` | Random number in (0, 1) |
| `srand(seed)` | Seed random number generator |

### Missing Operand Features

- **Decimal prefix `0d`**: TCL allows `0d` prefix for explicit decimal (e.g., `0d123`). Feather does not recognize this.

## Notes on Implementation Differences

### Integer Division and Modulo

TCL specifies that integer division truncates toward negative infinity (floor division), and the modulo result has the same sign as the divisor. For example:
- `-57 / 10` should be `-6`
- `-57 % 10` should be `3`

Feather uses C's native integer division which truncates toward zero. This may produce different results for negative operands.

### Exponentiation Limits

TCL documents a maximum exponent value of 268435455 when the base is an integer greater than 1. Feather does not enforce this limit explicitly and may overflow silently or produce incorrect results for very large exponents.

### Arbitrary Precision

TCL's `entier` and `isqrt` functions support arbitrary precision integers. Feather uses fixed 64-bit integers throughout, so very large integer results may overflow.

### Type Preservation

TCL is careful to preserve integer types when possible (e.g., `5 / 4` returns integer `1`). Feather generally follows this behavior but may differ in edge cases, particularly with the `int()` and `wide()` functions which always convert through double.

### Error Messages

Error message formats may differ from standard TCL. Feather produces errors like:
- `"syntax error in expression \"...\"`
- `"expected integer but got \"...\"`
- `"invalid bareword \"...\"`
- `"divide by zero"`

### NaN Handling

Feather treats NaN as a domain error in most contexts, similar to TCL. The `isnan()` function can be used to check for NaN without triggering an error.

### String to Number Conversion

When operands cannot be converted to numbers, comparison operators (`<`, `>`, `<=`, `>=`, `==`, `!=`) fall back to string comparison, matching TCL's behavior. String-specific operators (`lt`, `gt`, `le`, `ge`, `eq`, `ne`) always use string comparison.
