---
name: add-tests
description: |
  Use this skill when asked to add more tests for a feature
  to ensure that our implementation behaves like TCL 9.
---

# Adding Tests for a Feature

## Step-by-Step Process

### 1. Review the Manual Page

For each command in the feature, use the view-manual skill to review the documentation.

You MUST NOT use web fetch, that information is outdated.

Extract key information:

- All options and flags
- Edge cases mentioned in documentation
- Return values and error conditions
- Related commands

### 2. Review Existing Tests

List current tests for the feature:

```
cat /Users/dhamidi/projects/tclc/spec/tests/<feature>/<command>-*.tcl
```

Identify gaps:

- Options not tested
- Edge cases not covered
- Combinations of options not tested
- Boundary conditions

### 3. Create New Test Files

Write new tests to `spec/tests/<feature>/<command>-N.M.tcl`:

- Use next available version number (e.g., if 2.x exists, create 3.x)
- One focused test per file
- Include a comment describing what the test covers
- Use `puts` to output results for comparison

Example test file:

```tcl
# Test: lsort -nocase option
puts [lsort -nocase {Banana apple Cherry}]
```

### 4. Generate Oracle

Run tclsh to capture expected output:

```bash
make oracle FEATURE=<feature>
```

This runs each test through the reference TCL interpreter and saves expected output to `harness/oracle/<feature>.json`.

### 5. Run Differential Tests

Compare our implementation against the oracle:

```bash
make diff FEATURE=<feature>
```

Review failures to identify:

- Missing functionality
- Incorrect behavior
- Edge cases not handled

### 6. Fix Failures

For each failure:

1. Read the test file and understand what it's testing
2. Check the expected vs actual output in `harness/results/<feature>.json`
3. Locate the relevant code:
   - Core builtins: `core/builtins.c`
   - Host callbacks: `hosts/c/host.c`, `hosts/c/object.c`
4. Implement the fix
5. Rebuild: `make build`
6. Re-run tests: `make diff FEATURE=<feature>`

### 7. Verify All Features

After fixes, run tests for all features to ensure no regressions:

```bash
make diff FEATURE=control
make diff FEATURE=error
make diff FEATURE=lexer
make diff FEATURE=parser
make diff FEATURE=proc
make diff FEATURE=subst
make diff FEATURE=trycatch
make diff FEATURE=variables
make diff FEATURE=lists
```

## Test Categories to Cover

For each command, consider tests for:

1. **Basic functionality** - Simple, expected usage
2. **Options/flags** - Each option individually
3. **Option combinations** - Multiple options together
4. **Edge cases**:
   - Empty input
   - Single element
   - Special characters (`{`, `}`, `\`, `"`, `$`, `[`, `]`, `;`, `#`)
   - Whitespace (spaces, tabs, newlines)
   - Boundary indices (`end`, `end-N`, negative, out of bounds)
5. **Roundtrip tests** - Create then extract (e.g., `list` then `lindex`)
6. **Error conditions** - Wrong number of args, invalid input

## File Locations

| Item | Location |
|------|----------|
| Test files | `spec/tests/<feature>/<command>-N.M.tcl` |
| Oracle | `harness/oracle/<feature>.json` |
| Results | `harness/results/<feature>.json` |
| Core builtins | `core/builtins.c` |
| Host callbacks | `hosts/c/host.c` |
| Object/list handling | `hosts/c/object.c` |

## Naming Convention

Test files: `<command>-<major>.<minor>.tcl`

- Major version: increments for each test pass (1.x, 2.x, 3.x)
- Minor version: increments for related tests within a pass

Example sequence:

- `lsort-1.0.tcl` through `lsort-1.9.tcl` (first pass)
- `lsort-2.0.tcl` through `lsort-2.3.tcl` (second pass)
- `lsort-3.0.tcl` through `lsort-3.5.tcl` (third pass)
