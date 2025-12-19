# TCL Core Implementation Feedback Loop

This document describes the automated feedback loop for implementing the TCL 9 core interpreter in C with a Go host.

## Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     SPECIFICATION LAYER                         │
│  TCL 9 man pages + test suite + real tclsh as oracle           │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      FEATURE QUEUE                              │
│  Ordered by dependencies: lexer → subst → expr → control → ... │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
        ┌─────────────────────────────────────────────┐
        │              PER-FEATURE LOOP               │
        │                                             │
        │  ┌───────────────────────────────────┐      │
        │  │ 1. Extract tests for feature      │      │
        │  └───────────────┬───────────────────┘      │
        │                  ▼                          │
        │  ┌───────────────────────────────────┐      │
        │  │ 2. Generate oracle expectations   │      │
        │  │    (run against real tclsh)       │      │
        │  └───────────────┬───────────────────┘      │
        │                  ▼                          │
        │  ┌───────────────────────────────────┐      │
        │  │ 3. Agent implements/fixes feature │◄──┐  │
        │  └───────────────┬───────────────────┘   │  │
        │                  ▼                       │  │
        │  ┌───────────────────────────────────┐   │  │
        │  │ 4. Run differential tests         │   │  │
        │  └───────────────┬───────────────────┘   │  │
        │                  ▼                       │  │
        │  ┌───────────────────────────────────┐   │  │
        │  │ 5. All pass?                      │   │  │
        │  │    No ─────────────────────────────┘  │
        │  │    Yes ──► Commit & next feature      │
        │  └───────────────────────────────────┘   │
        └─────────────────────────────────────────────┘
```

## Harness Tool

The harness is a Go program that orchestrates testing. Build it with:

```bash
make harness
```

Or run commands directly:

```bash
# Build and run in one step
make oracle FEATURE=lexer
make diff FEATURE=lexer
make loop FEATURE=lexer
```

### Commands

| Command | Description |
|---------|-------------|
| `harness oracle [feature]` | Generate expected outputs from tclsh |
| `harness diff <feature>` | Compare implementation vs oracle |
| `harness prompt <feature>` | Generate agent prompt for failing tests |
| `harness loop <feature>` | Run interactive feedback loop |
| `harness features` | List all features and status |
| `harness deps <feature>` | Show dependencies for a feature |

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `TCLSH` | Path to reference TCL interpreter | `tclsh` |
| `TCLC_INTERP` | Path to our implementation | (none) |

## Test Strategy

### Dual-Layer Testing

**Layer 1: C Unit Tests (Fast Iteration)**

Mock host implementation in pure C for testing parser, evaluator, and control flow logic without CGO overhead.

```
core/
├── tclc.c           # Implementation
├── tclc.h           # Interface
└── test/
    ├── mock_host.c  # Minimal mock callbacks
    ├── test_lexer.c
    ├── test_parser.c
    └── ...
```

Run with: `make test-c`

**Layer 2: Integration Tests (Oracle Comparison)**

Full Go host with real tclsh as oracle.

```
harness/
├── oracle/          # Expected outputs from tclsh
│   └── lexer.json
├── results/         # Actual outputs from our impl
│   └── lexer.json
└── *.go             # Harness implementation
```

Run with: `make diff FEATURE=lexer`

### Oracle Generation

Before implementing a feature, generate expected outputs:

```bash
# Generate oracle for a specific feature
make oracle FEATURE=lexer

# This runs:
# 1. Finds test cases in spec/tests/lexer/
# 2. Runs each against real tclsh
# 3. Records stdout, stderr, return code
# 4. Saves to harness/oracle/lexer.json
```

### Differential Testing

After implementation:

```bash
# Run differential tests for a feature
make diff FEATURE=lexer

# This runs:
# 1. Loads oracle from harness/oracle/lexer.json
# 2. Runs same tests against our interpreter (TCLC_INTERP)
# 3. Compares output character-by-character
# 4. Reports mismatches with diff
# 5. Saves results to harness/results/lexer.json
```

## Agent Workflow

### 1. Feature Selection

Pick the next feature from the queue (respecting dependencies):

```bash
make features
```

### 2. Generate Oracle

```bash
make oracle FEATURE=lexer
```

### 3. Run Feedback Loop

```bash
make loop FEATURE=lexer
```

This will:
1. Run differential tests
2. If tests fail, generate `prompt.md`
3. Wait for you to make changes
4. Press Enter to re-run
5. Repeat until all tests pass

### 4. Agent Prompt

The generated `prompt.md` contains:
- Feature description and dependencies
- Failing test details with diffs
- Expected vs actual output
- Files to modify

## Directory Structure

```
tclc/
├── ARCHITECTURE.md          # System design
├── Makefile                 # Orchestration
│
├── core/                    # C implementation
│   ├── tclc.h              # Host interface
│   └── *.c                 # Implementation files
│
├── hosts/go/               # Go host implementation
│   └── *.go
│
├── spec/                    # Specifications
│   ├── features.yaml       # Feature queue with deps
│   └── tests/              # Test cases
│       └── lexer/
│           ├── words-1.1.tcl
│           └── ...
│
└── harness/                 # Testing infrastructure (Go)
    ├── *.go                # Harness source
    ├── go.mod
    ├── oracle/             # Expected outputs (generated)
    └── results/            # Actual outputs (generated)
```

## Writing Tests

Tests are `.tcl` files in `spec/tests/<feature>/`. Each file is a complete TCL script:

```tcl
# spec/tests/lexer/words-1.1.tcl
# Test: basic word splitting

puts hello
puts hello world
puts    hello    world
```

The oracle records the exact output from tclsh. Your implementation must match exactly.

## Exit Criteria

A feature is complete when:

1. All differential tests pass (output matches oracle exactly)
2. No regressions in previously completed features
3. Code compiles without warnings (`-Wall -Wextra -Werror`)

## Troubleshooting

### "No oracle found"

Run: `make oracle FEATURE=<feature>`

### "No tests found"

Add `.tcl` files to `spec/tests/<feature>/`

### Tests pass locally but fail in CI

Check for:
- Platform-specific behavior (line endings, paths)
- Timing-dependent output
- Locale/encoding differences
