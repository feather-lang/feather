# GitHub Issue Content

**Title:** Performance Benchmark Results - 2026-01-03

**Labels:** performance, benchmarks

**Body:**

---

See BENCHMARK_RESULTS.md for the full benchmark report.

This issue documents the performance benchmark results run on 2026-01-03 against the Go reference implementation. All 49 benchmarks passed successfully.

## Quick Summary

- **Total Benchmarks:** 49
- **Success Rate:** 100%
- **Total Runtime:** 45.54 seconds

## Top Performers

🏆 **Fastest Operations (>50K ops/sec):**
1. `expr simple addition` - 51,274 ops/sec
2. `string length long` - 50,738 ops/sec
3. `string length short` - 50,173 ops/sec

## Categories Tested

- Control Flow (9 benchmarks)
- Dictionary Operations (10 benchmarks)
- Expression Evaluation (10 benchmarks)
- List Operations (8 benchmarks)
- String Operations (10 benchmarks)

See [BENCHMARK_RESULTS.md](./BENCHMARK_RESULTS.md) for complete details including:
- All benchmark results with timing data
- Performance insights for each category
- Top performers across all operations
- System information and conclusions

## How to Create This Issue

Run the helper script:
```bash
./.github-issue-create.sh
```

Or create manually on GitHub with the above title, labels, and body content from BENCHMARK_RESULTS.md.
