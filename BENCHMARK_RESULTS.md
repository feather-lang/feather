# Feather Performance Benchmark Results

> **Date:** January 3, 2026
> **Host:** Go reference implementation
> **Total Benchmarks:** 49
> **Success Rate:** 100% (49/49 passed)

## Overview

This report contains comprehensive performance benchmarks for the Feather TCL interpreter, covering five main categories: Control Flow, Dictionary Operations, Expression Evaluation, List Operations, and String Operations.

---

## Control Flow (9 benchmarks)

| Benchmark | Iterations | Avg Time | Ops/sec |
|-----------|-----------|----------|---------|
| for loop 100 iterations | 1,000 | 2.67ms/op | 374.70 |
| for loop 1000 iterations | 200 | 30.49ms/op | 32.80 |
| while loop 100 iterations | 1,000 | 2.87ms/op | 348.30 |
| foreach 10 items | 5,000 | 77.67µs/op | 12,874.49 |
| foreach 100 items | 1,000 | 3.73ms/op | 268.24 |
| if true simple | 10,000 | 40.99µs/op | 24,398.58 |
| if false with else | 10,000 | 50.70µs/op | 19,725.03 |
| switch exact match first | 5,000 | 72.62µs/op | 13,770.12 |
| switch exact match last | 5,000 | 78.64µs/op | 12,716.17 |

**Key Insights:**
- Fast control structures: `if` statements process ~20-24K ops/sec
- `foreach` is highly efficient for small lists (12.8K ops/sec for 10 items)
- Loop overhead is consistent and predictable

---

## Dictionary Operations (10 benchmarks)

| Benchmark | Iterations | Avg Time | Ops/sec |
|-----------|-----------|----------|---------|
| dict create small | 10,000 | 32.12µs/op | 31,133.25 |
| dict get | 10,000 | 21.85µs/op | 45,772.87 |
| dict set | 5,000 | 25.75µs/op | 38,837.97 |
| dict exists true | 10,000 | 25.96µs/op | 38,526.74 |
| dict exists false | 10,000 | 23.78µs/op | 42,048.61 |
| dict size small | 10,000 | 21.95µs/op | 45,566.39 |
| dict keys | 5,000 | 21.33µs/op | 46,877.93 |
| dict values | 5,000 | 22.66µs/op | 44,124.78 |
| dict create large (100 keys) | 500 | 962.21µs/op | 1,039.28 |
| dict get large dict | 1,000 | 24.18µs/op | 41,363.34 |

**Key Insights:**
- Dictionary lookups are very fast: ~42-47K ops/sec
- `dict get` performance scales well (only 10% slower on 100-key dicts)
- Creating large dictionaries: ~1K ops/sec (expected for 100-key creation)
- All basic dict operations complete in <30µs

---

## Expression Evaluation (10 benchmarks)

| Benchmark | Iterations | Avg Time | Ops/sec |
|-----------|-----------|----------|---------|
| expr simple addition | 10,000 | 19.50µs/op | 51,274.16 |
| expr complex arithmetic | 5,000 | 32.89µs/op | 30,407.15 |
| expr with variables | 5,000 | 26.15µs/op | 38,242.38 |
| expr comparison | 10,000 | 20.75µs/op | 48,204.39 |
| expr logical and | 10,000 | 21.95µs/op | 45,558.09 |
| expr sqrt | 5,000 | 46.60µs/op | 21,459.23 |
| expr pow | 5,000 | 55.31µs/op | 18,080.89 |
| expr sin | 5,000 | 50.09µs/op | 19,962.47 |
| expr string eq | 10,000 | 26.27µs/op | 38,064.79 |
| expr string ne | 10,000 | 25.13µs/op | 39,793.08 |

**Key Insights:**
- Simple expressions are blazing fast: 51K ops/sec
- Math functions (sqrt, pow, sin) are efficient: 18-21K ops/sec
- String comparisons perform well: ~38-40K ops/sec
- Variable access adds minimal overhead (~26µs vs 19µs)

---

## List Operations (8 benchmarks)

| Benchmark | Iterations | Avg Time | Ops/sec |
|-----------|-----------|----------|---------|
| lindex small list | 10,000 | 29.82µs/op | 33,533.42 |
| llength small list | 10,000 | 32.08µs/op | 31,174.01 |
| lappend to variable | 5,000 | 380.71µs/op | 2,626.70 |
| lindex large list (1000 items) | 1,000 | 265.32µs/op | 3,769.08 |
| llength large list (1000 items) | 1,000 | 243.66µs/op | 4,104.08 |
| lsort 100 integers | 500 | 121.93µs/op | 8,201.29 |
| lsearch linear 100 items | 1,000 | 834.00µs/op | 1,199.03 |
| lreverse 100 items | 1,000 | 834.86µs/op | 1,197.81 |

**Key Insights:**
- Small list operations are fast: ~30-33K ops/sec
- `lappend` shows interesting performance characteristics (likely due to list reallocation)
- `lsort` is efficient: 8.2K ops/sec for 100 integers
- Linear search on 100 items: ~1.2K ops/sec (expected for O(n) operation)

---

## String Operations (10 benchmarks)

| Benchmark | Iterations | Avg Time | Ops/sec |
|-----------|-----------|----------|---------|
| string length short | 10,000 | 19.93µs/op | 50,173.10 |
| string length long | 5,000 | 19.71µs/op | 50,738.24 |
| string toupper | 5,000 | 20.63µs/op | 48,468.40 |
| string tolower | 5,000 | 20.85µs/op | 47,963.93 |
| string index | 10,000 | 23.12µs/op | 43,245.11 |
| string range | 10,000 | 27.43µs/op | 36,449.79 |
| string match simple | 5,000 | 26.02µs/op | 38,436.41 |
| string match complex | 2,000 | 29.82µs/op | 33,528.92 |
| append short strings | 5,000 | 25.54µs/op | 39,151.20 |
| split on space | 2,000 | 63.25µs/op | 15,811.03 |

**Key Insights:**
- String length is very fast and O(1): ~50K ops/sec regardless of string size
- Case conversion operations: ~48K ops/sec
- Pattern matching is efficient: 33-38K ops/sec
- String splitting: ~15.8K ops/sec

---

## Top Performers

🏆 **Fastest Operations (>50K ops/sec):**
1. `expr simple addition` - 51,274 ops/sec
2. `string length long` - 50,738 ops/sec
3. `string length short` - 50,173 ops/sec

⚡ **Highly Efficient (40-50K ops/sec):**
- `expr comparison` - 48,204 ops/sec
- `string toupper` - 48,468 ops/sec
- `dict keys` - 46,878 ops/sec
- `dict get` - 45,773 ops/sec
- `dict size small` - 45,566 ops/sec
- `expr logical and` - 45,558 ops/sec
- `dict exists false` - 42,049 ops/sec

---

## Performance Categories

Based on these benchmarks, we can categorize Feather's performance:

- **Ultra-fast (>40K ops/sec):** Basic expressions, string length, dict lookups
- **Fast (20-40K ops/sec):** Most dict operations, string operations, simple control flow
- **Moderate (5-20K ops/sec):** Math functions, foreach loops, sorting
- **Expected overhead (1-5K ops/sec):** Large list operations, linear searches, complex data structure creation

---

## System Information

- **Platform:** Linux 4.4.0
- **Date:** 2026-01-03
- **Implementation:** Go reference implementation
- **Total Runtime:** 45.54 seconds
- **Benchmark Framework:** Custom XML-based benchmark system with minimal spawning overhead

---

## Conclusion

The Feather interpreter demonstrates excellent performance across all tested operations:

✅ **Strengths:**
- Exceptionally fast expression evaluation
- Highly efficient dictionary operations
- Excellent string operation performance
- Consistent and predictable performance characteristics

📊 **Performance is competitive for:**
- Control flow operations
- List operations on small-to-medium datasets
- Mathematical computations

🎯 **Next Steps:**
- Continue monitoring performance as new features are added
- Consider optimization opportunities for `lappend` and large list operations
- Expand benchmark suite to cover more real-world use cases
- Track performance trends over time to detect regressions
