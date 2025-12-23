---
name: compare-to-reference
description: |
  Compare test results between our implementation (gcl) and the reference TCL interpreter (oracle).
  Use when debugging discrepancies or verifying behavior matches standard TCL.
---

# Compare to Reference

This skill runs test cases against both our tclc implementation and the reference TCL interpreter to identify behavioral differences.

## Arguments

The skill accepts arguments in this format:
```
/compare-to-reference <test-file> [--name <pattern>]
```

- `<test-file>` - Path to test file or directory (required)
- `--name <pattern>` - Regex pattern to filter specific test cases (optional)

## Execution Steps

### 1. Ensure Both Hosts Are Built

```bash
mise run build:gcl
mise run build:oracle
```

### 2. Run Tests Against Both Implementations

Run the harness against our implementation:
```bash
harness run --host bin/gcl --verbose <test-file> [--name <pattern>]
```

Run the harness against the reference TCL:
```bash
harness run --host bin/oracle --verbose <test-file> [--name <pattern>]
```

### 3. Compare Results

For each test case, compare:
- **Return code**: TCL_OK vs TCL_ERROR
- **Result value**: The actual output
- **Error message**: If applicable

### 4. Report Differences

Present a summary table:

| Test Case | gcl | oracle | Match? |
|-----------|-----|--------|--------|
| suite > test1 | TCL_OK: "value" | TCL_OK: "value" | Yes |
| suite > test2 | TCL_ERROR: "msg" | TCL_OK: "different" | **NO** |

### 5. Investigate Discrepancies

For any mismatches:
1. Show the test script that produced different results
2. Run the script manually under both interpreters to see full output
3. Suggest which behavior is correct based on TCL semantics

## Example Usage

Compare all expr tests:
```
/compare-to-reference testcases/m2/
```

Compare a specific test case:
```
/compare-to-reference testcases/m2/expr-comparisons.html --name "greater than"
```

Debug a single failing test:
```
/compare-to-reference testcases/m4/ --name "break in nested"
```

## Quick Manual Comparison

For ad-hoc comparison without test files:

```bash
# Our implementation
echo 'expr {1 + 2}' | bin/gcl

# Reference TCL
echo 'expr {1 + 2}' | bin/oracle
```

## Notes

- The oracle embeds real TCL 9.x via libtcl
- Some differences are expected for features we intentionally omit
- When behaviors differ, document whether tclc should match or diverge
