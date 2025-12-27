# expr

Evaluates a mathematical or logical expression.

## Syntax

```tcl
expr arg ?arg ...?
```

## Parameters

- **arg**: One or more arguments forming the expression

## Description

The `expr` command evaluates its arguments as a mathematical expression and returns the result. Arguments are concatenated with spaces before evaluation.

### Operators (lowest to highest precedence)

| Precedence | Operators | Description |
|------------|-----------|-------------|
| 1 | `? :` | Ternary conditional |
| 2 | `\|\|` | Logical OR |
| 3 | `&&` | Logical AND |
| 4 | `\|` | Bitwise OR |
| 5 | `^` | Bitwise XOR |
| 6 | `&` | Bitwise AND |
| 7 | `== != eq ne` | Equality (numeric and string) |
| 8 | `< <= > >= lt le gt ge in ni` | Relational |
| 9 | `<< >>` | Bit shift |
| 10 | `+ -` | Addition, subtraction |
| 11 | `* / %` | Multiplication, division, modulo |
| 12 | `**` | Exponentiation |
| 13 | `~ ! - +` | Unary operators |

### Data Types

- **Integers**: Decimal, hex (`0x`), octal (`0o`), binary (`0b`)
- **Floats**: Standard notation with optional exponent
- **Strings**: Quoted with double quotes

### Math Functions

`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `sqrt`, `abs`, `ceil`, `floor`, `round`, `log`, `log10`, `exp`, `pow`, `min`, `max`, `rand`, `srand`, `int`, `double`, `wide`, `bool`

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
</script>

<FeatherPlayground :code="basicArithmetic" />

<FeatherPlayground :code="variablesInExpressions" />

<FeatherPlayground :code="comparisonAndLogical" />

<FeatherPlayground :code="ternaryOperator" />

<FeatherPlayground :code="mathFunctions" />

<FeatherPlayground :code="stringComparison" />

## See Also

- [eval](./eval)
- [info](./info)
