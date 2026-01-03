# Feather Performance Benchmarks

This document describes the performance benchmark system for Feather.

## Overview

The benchmark system is designed to:
- Measure performance of Feather operations across different host implementations
- Work with any host (Go, JavaScript/WASM, etc.) just like the test harness
- Minimize spawning overhead by running all benchmarks in a single process
- Provide consistent, reproducible performance measurements

## Running Benchmarks

### Quick Start

```bash
# Run benchmarks with the Go host
mise bench

# Run benchmarks with the JavaScript/WASM host
mise bench:js

# Run all benchmarks (both hosts)
mise bench:all

# Run specific benchmark file
mise bench benchmarks/list-ops.html
```

### Manual Invocation

```bash
# Build the benchmark runner
mise build:bench

# Run benchmarks
bin/bench -host bin/feather-tester benchmarks/*.html
bin/bench -host js/tester.js benchmarks/*.html
```

## Benchmark File Format

Benchmarks are defined in XML/HTML files, similar to test cases. Each file contains a `<benchmark-suite>` with one or more `<benchmark>` elements.

### Basic Structure

```xml
<benchmark-suite name="List Operations">

  <benchmark name="lindex small list" warmup="100" iterations="10000">
    <script>
      lindex {a b c d e} 2
    </script>
  </benchmark>

  <benchmark name="lsort with setup" warmup="20" iterations="500">
    <setup>
      set items {}
      for {set i 100} {$i > 0} {incr i -1} {
        lappend items $i
      }
    </setup>
    <script>
      lsort -integer $items
    </script>
  </benchmark>

</benchmark-suite>
```

### Benchmark Attributes

- `name` (required): Descriptive name for the benchmark
- `warmup` (optional): Number of warmup iterations before measurement (default: 0)
- `iterations` (optional): Number of measured iterations (default: 1000)

### Benchmark Elements

- `<setup>`: Script to run once before iterations (optional)
  - Used to prepare data structures or variables
  - Not included in timing measurements
  - Runs before warmup iterations

- `<script>`: The code to benchmark (required)
  - Executed for both warmup and measured iterations
  - Timing is measured for each iteration

## How It Works

### Minimizing Overhead

Unlike traditional benchmarking that spawns a new process for each benchmark, the Feather benchmark system:

1. **Single Process Execution**: All benchmarks in a suite run in a single interpreter instance
2. **Batch Communication**: The benchmark runner sends all benchmarks to the host at once via JSON
3. **In-Process Timing**: The host measures timing internally using high-resolution timers
4. **Efficient Reporting**: Results are streamed back as JSON over fd 3

This design minimizes:
- Process spawning overhead
- Interpreter initialization overhead
- Inter-process communication overhead
- File I/O overhead

### Benchmark Protocol

1. The `bench` command parses benchmark files and sends benchmark data as JSON to the host's stdin
2. The host (feather-tester or tester.js) runs with `--benchmark` flag
3. For each benchmark:
   - Host runs the setup script once (if provided)
   - Host runs warmup iterations (not measured)
   - Host runs measured iterations with high-resolution timing
   - Host calculates statistics (total, avg, min, max, ops/sec)
   - Host sends results as JSON to fd 3
4. The `bench` command receives results and formats output

### Timing Details

- **Go host**: Uses `time.Now()` and `time.Since()` for nanosecond precision
- **JavaScript host**: Uses `process.hrtime.bigint()` for nanosecond precision
- Timing includes only the script execution, not setup or warmup
- Each iteration is timed individually to capture min/max statistics

## Output Format

Benchmark results include:

```
=== Benchmark Suite: List Operations ===

PASS: lindex small list
  Iterations: 10000
  Total time: 45.23ms
  Avg time:   4.52µs/op
  Min time:   3.89µs
  Max time:   12.34µs
  Ops/sec:    221238.94

PASS: lsort 100 integers
  Iterations: 500
  Total time: 125.67ms
  Avg time:   251.34µs/op
  Min time:   234.12µs
  Max time:   456.78µs
  Ops/sec:    3978.45

--- Summary ---
Total: 2  Success: 2  Failed: 0
```

## Writing Good Benchmarks

### Best Practices

1. **Use Warmup Iterations**: Always include warmup to let the interpreter reach steady state
   ```xml
   <benchmark name="example" warmup="100" iterations="1000">
   ```

2. **Choose Appropriate Iteration Counts**:
   - Fast operations (< 1µs): 10,000+ iterations
   - Medium operations (1-100µs): 1,000-5,000 iterations
   - Slow operations (> 100µs): 100-500 iterations

3. **Isolate Setup from Measurement**: Use `<setup>` for preparation
   ```xml
   <benchmark name="lsearch large list" warmup="50" iterations="1000">
     <setup>
       <!-- This runs once, not measured -->
       set items {}
       for {set i 0} {$i < 1000} {incr i} {
         lappend items "item$i"
       }
     </setup>
     <script>
       <!-- Only this is measured -->
       lsearch $items "item500"
     </script>
   </benchmark>
   ```

4. **Benchmark Realistic Workloads**: Measure operations as they would be used in practice

5. **Test Multiple Scales**: Include benchmarks for small and large data sets
   ```xml
   <benchmark name="lindex small list" ...>
     <script>lindex {a b c d e} 2</script>
   </benchmark>

   <benchmark name="lindex large list" ...>
     <setup>
       set items {}
       for {set i 0} {$i < 1000} {incr i} {
         lappend items "item$i"
       }
     </setup>
     <script>lindex $items 500</script>
   </benchmark>
   ```

### Common Pitfalls

1. **Don't Benchmark Constant Expressions**:
   ```xml
   <!-- Bad: This will be optimized -->
   <script>expr {1 + 2}</script>

   <!-- Good: Use variables -->
   <setup>set a 1; set b 2</setup>
   <script>expr {$a + $b}</script>
   ```

2. **Don't Include Setup in Timing**: Always use `<setup>` for data preparation

3. **Don't Use Too Few Iterations**: Variance increases with fewer iterations

4. **Don't Benchmark Side Effects**: Each iteration should be independent
   ```xml
   <!-- Bad: Results depend on previous iterations -->
   <setup>set counter 0</setup>
   <script>incr counter</script>

   <!-- Good: Each iteration is independent -->
   <setup>set value 5</setup>
   <script>expr {$value * 2}</script>
   ```

## Implementation Notes

### Files

- `harness/benchmark_model.go`: Data structures for benchmarks and results
- `harness/benchmark_parser.go`: Parses benchmark XML files
- `harness/benchmark_runner.go`: Executes benchmarks against hosts
- `harness/benchmark_reporter.go`: Formats and displays results
- `harness/cmd/bench/main.go`: Command-line benchmark runner
- `cmd/feather-tester/main.go`: Go host with `--benchmark` mode
- `js/tester.js`: JavaScript host with `--benchmark` mode

### Future Enhancements

Potential improvements:
- Comparative benchmarks (compare two hosts side-by-side)
- Memory profiling support
- Statistical analysis (standard deviation, outlier detection)
- Benchmark result history tracking
- Performance regression detection
- Continuous benchmarking in CI/CD
