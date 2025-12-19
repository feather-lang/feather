# Agent Prompt Generation

This document describes the prompt generation system for invoking a coding agent to implement or fix TCL features.

## Usage

Generate a prompt for a specific feature:

```bash
# Via Makefile (recommended)
make prompt FEATURE=subst

# Direct harness invocation
./build/harness prompt subst

# Save to file
make prompt FEATURE=subst > prompt.md
```

## Testing Commands

```bash
# Run differential tests for a single feature
make diff FEATURE=subst

# Run differential tests for ALL features
make diff-all

# Interactive feedback loop (runs diff, shows failures, waits for changes)
make loop FEATURE=subst
```

## Prompt Structure

The generated prompt includes:

1. **Feature metadata** - ID, description, dependencies
2. **Test status** - Pass/fail counts
3. **Failing test details** - For each failing test:
   - The TCL script being tested
   - Expected output (from tclsh oracle)
   - Actual output (from our interpreter)
   - Diff between expected and actual
4. **Implementation guidelines** - Key constraints
5. **Source files** - Files the agent may modify
6. **Reference files** - Oracle and header files

## Example Output

```markdown
# TCL Core Implementation Task

## Feature: subst

**Description:** Variable, command, and backslash substitution

**Dependencies:** lexer, parser

**Status:** 12/15 tests passing

---

## Current Test Results

### Failing Tests

#### Test: subst-1.7

**Script:**
```tcl
set x "world"
subst {Hello $x}
```

**Expected output:**
```
Hello world
```

**Expected return code:** 0

**Actual output:**
```
Hello $x
```

**Actual return code:** 0

**Diff:**
```diff
-Hello world
+Hello $x
```

---

## Implementation Guidelines

1. **Match TCL 9 semantics exactly** - The oracle (tclsh9) is the source of truth
2. **No memory allocation in C** - Use host callbacks for all dynamic memory
3. **Error messages must match** - TCL has specific error message formats
4. **Handle edge cases** - Empty strings, negative indices, Unicode, etc.

## Files You May Modify

- `core/subst.c` - Substitution implementation

## Your Task

1. Analyze the failing tests and identify the root cause
2. Implement or fix the relevant code in the C core
3. Ensure your changes don't break other tests
4. Run `make diff FEATURE=subst` to check against oracle

Focus on making tests pass one at a time. Commit working increments.
```

## Implementation

The prompt generation is implemented in Go at `harness/prompt.go`. It:

1. Loads feature metadata from `spec/features.yaml`
2. Loads test results from `harness/results/<feature>.json`
3. Loads oracle data from `harness/oracle/<feature>.json`
4. Formats the prompt with failing test details

## Workflow

1. Generate oracle expectations: `make oracle FEATURE=<feature>`
2. Run tests: `make diff FEATURE=<feature>` or `make diff-all`
3. Generate prompt for failures: `make prompt FEATURE=<feature>`
4. Fix issues and repeat until all tests pass
