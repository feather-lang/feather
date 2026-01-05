# expr - Expression Evaluation

Evaluates a mathematical or logical expression.

## Syntax

```tcl
expr arg ?arg ...?
```

## Parameters

- **arg**: One or more arguments forming the expression

## Description

The `expr` command evaluates its arguments as a mathematical expression and returns the result. Arguments are concatenated with spaces before evaluation.

## Summary of Our Implementation

Feather implements a comprehensive expression evaluator. The implementation uses a recursive descent parser with proper operator precedence and supports:

- Full arithmetic operations with integer and floating-point numbers
- Boolean and comparison operators
- Bitwise operations
- Variable substitution (`$var`, `${var}`)
- Command substitution (`[cmd args]`)
- Ternary conditional operator (`?:`)
- Math function calls (via `tcl::mathfunc::` namespace)
- Short-circuit evaluation for `&&`, `||`, and `?:`
- Comments starting with `#`

## Supported Features

### Operands

- **Numeric literals**: integers (decimal, hex `0x`, binary `0b`, octal `0o`), floating-point (`3.14`, `.5`, `1e10`, `3.14e-5`)
- **Digit separators**: underscores in numeric literals (`100_000_000`, `0xffff_ffff`)
- **Boolean literals**: `true`, `false`, `yes`, `no`, `on`, `off` (case-insensitive)
- **Special float values**: `Inf`, `-Inf`, `NaN` recognized
- **Decimal prefix**: `0d` prefix for explicit decimal (e.g., `0d123`)
- **Variables**: `$name`, `${name}`, including namespace-qualified names
- **Command substitution**: `[command args]`
- **Braced strings**: `{literal string}`
- **Quoted strings**: `"string with $var and [cmd] substitution"`
- **Parenthesized expressions**: `(expr)`
- **Comments**: `#` to end of line

### Operators (in precedence order, lowest to highest)

| Operator(s) | Description |
|-------------|-------------|
| `?:` | Ternary conditional (right-to-left) |
| `\|\|` | Logical OR (short-circuit) |
| `&&` | Logical AND (short-circuit) |
| `\|` | Bitwise OR |
| `^` | Bitwise XOR |
| `&` | Bitwise AND |
| `==` `!=` | Numeric equality (with string fallback) |
| `eq` `ne` | String equality |
| `<` `>` `<=` `>=` | Numeric comparison (with string fallback) |
| `lt` `gt` `le` `ge` | String comparison |
| `in` `ni` | List containment |
| `<<` `>>` | Bitwise shift |
| `+` `-` | Addition, subtraction |
| `*` `/` `%` | Multiplication, division, modulo |
| `**` | Exponentiation (right-to-left) |
| `-` `+` `~` `!` | Unary minus, plus, bitwise NOT, logical NOT |

### Math Functions (via `tcl::mathfunc::`)

| Function | Description |
|----------|-------------|
| `abs(x)` | Absolute value |
| `acos(x)` | Arc cosine |
| `asin(x)` | Arc sine |
| `atan(x)` | Arc tangent |
| `atan2(y, x)` | Arc tangent of y/x |
| `bool(arg)` | Convert to boolean (0 or 1) |
| `ceil(x)` | Ceiling |
| `cos(x)` | Cosine |
| `cosh(x)` | Hyperbolic cosine |
| `double(x)` | Convert to double |
| `entier(x)` | Convert to integer (same as `int()` in Feather) |
| `exp(x)` | Exponential |
| `floor(x)` | Floor |
| `fmod(x, y)` | Floating-point modulo |
| `hypot(x, y)` | Hypotenuse |
| `int(x)` | Convert to integer |
| `isfinite(x)` | Test if finite |
| `isinf(x)` | Test if infinite |
| `isnan(x)` | Test if NaN |
| `isnormal(x)` | Test if normal |
| `issubnormal(x)` | Test if subnormal |
| `isunordered(x, y)` | Test if either is NaN |
| `log(x)` | Natural logarithm |
| `log10(x)` | Base-10 logarithm |
| `max(arg, ...)` | Maximum of arguments |
| `min(arg, ...)` | Minimum of arguments |
| `pow(x, y)` | Power |
| `round(x)` | Round to nearest integer |
| `sin(x)` | Sine |
| `sinh(x)` | Hyperbolic sine |
| `sqrt(x)` | Square root |
| `tan(x)` | Tangent |
| `tanh(x)` | Hyperbolic tangent |
| `wide(x)` | Convert to wide integer |

## Unsupported Features

These functions are NOT implemented in Feather:

| Function | Reason |
|----------|--------|
| `isqrt(x)` | Requires arbitrary precision integers (bignums), which Feather does not support |
| `rand()` | Random number generation is outside Feather's scope as an embeddable interpreter |
| `srand(seed)` | Random number generation is outside Feather's scope as an embeddable interpreter |

## Implementation Notes

### Integer Division and Modulo

TCL specifies that integer division truncates toward negative infinity (floor division), and the modulo result has the same sign as the divisor. Feather uses C's native integer division which truncates toward zero. This may produce different results for negative operands.

### Exponentiation Limits

TCL documents a maximum exponent value of 268435455 when the base is an integer greater than 1. Feather does not enforce this limit explicitly and may overflow silently or produce incorrect results for very large exponents.

### Arbitrary Precision

TCL's `entier` and `isqrt` functions support arbitrary precision integers. Feather uses fixed 64-bit integers throughout, so very large integer results may overflow.

### Type Preservation

TCL is careful to preserve integer types when possible (e.g., `5 / 4` returns integer `1`). Feather generally follows this behavior but may differ in edge cases.

### String to Number Conversion

When operands cannot be converted to numbers, comparison operators (`<`, `>`, `<=`, `>=`, `==`, `!=`) fall back to string comparison, matching TCL's behavior. String-specific operators (`lt`, `gt`, `le`, `ge`, `eq`, `ne`) always use string comparison.

## Examples

<script setup>
import FeatherPlayground from '../../.vitepress/components/FeatherPlayground.vue'

const basicArithmetic = `# Basic arithmetic
puts [expr {2 + 3 * 4}]
puts [expr {(2 + 3) * 4}]
puts [expr {2 ** 10}]`

const variablesInExpressions = `# Variables in expressions
set x 10
set y 3
puts "Sum: [expr {$x + $y}]"
puts "Division: [expr {$x / $y}]"
puts "Modulo: [expr {$x % $y}]"`

const comparisonAndLogical = `# Comparison and logical operators
set a 5
set b 10
puts [expr {$a < $b}]
puts [expr {$a == 5 && $b == 10}]
puts [expr {$a > $b || $b > $a}]`

const ternaryOperator = `# Ternary operator
set score 85
set grade [expr {$score >= 90 ? "A" : $score >= 80 ? "B" : "C"}]
puts "Grade: $grade"`

const mathFunctions = `# Math functions
puts "sqrt(16) = [expr {sqrt(16)}]"
puts "abs(-5) = [expr {abs(-5)}]"
puts "sin(0) = [expr {sin(0)}]"
puts "max(3, 7, 2) = [expr {max(3, 7, 2)}]"`

const stringComparison = `# String comparison
set name "alice"
puts [expr {"alice" eq $name}]
puts [expr {"bob" ne $name}]
puts [expr {"a" in "abc"}]`

const numericFormats = `# Numeric formats
puts "Hex: [expr {0xff}]"
puts "Binary: [expr {0b1010}]"
puts "With separators: [expr {1_000_000}]"
puts "Boolean: [expr {true && !false}]"`
</script>

<FeatherPlayground :tcl="basicArithmetic" />

<FeatherPlayground :tcl="variablesInExpressions" />

<FeatherPlayground :tcl="comparisonAndLogical" />

<FeatherPlayground :tcl="ternaryOperator" />

<FeatherPlayground :tcl="mathFunctions" />

<FeatherPlayground :tcl="stringComparison" />

<FeatherPlayground :tcl="numericFormats" />

## See Also

- [eval](./eval)
- [info](./info)
