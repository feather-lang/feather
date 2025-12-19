---
name: review-implementation
description: |
  Use this skill when reviewing the code of the core C implementation.
  Produces an audit scorecard against architecture and style rules.
---

# Core Implementation Review

This skill performs an audit of C code in the `core/` directory against the project's architecture constraints and code style rules.

## Rules Reference

### Architecture Rules (ARCH-*)

| ID | Rule | Description |
|----|------|-------------|
| ARCH-1 | No Preprocessor Constants | Do not use `#define` for constants. The preprocessor is invisible to the compiler and makes generating bindings difficult. |
| ARCH-2 | No Allocation in C | Do not call `malloc`, `free`, or any standard library allocation functions. All dynamic memory is managed by the Go host through callbacks. |
| ARCH-3 | Minimal Standard Library | Only use `<stddef.h>` (size_t, NULL), `<stdint.h>` (fixed-width integers), and `<math.h>` (for expr). No other libc headers. |
| ARCH-4 | Host-Managed Data Structures | The C core only sees opaque handles (`TclObj*`, `TclInterp*`, etc.) and calls through function pointers. Do not peek inside host structures. |
| ARCH-5 | Trampoline Evaluation | The eval loop must be non-recursive to support coroutines. No recursive descent through `eval()`. |
| ARCH-6 | Arena Allocation for Temporaries | Use arena allocation (via host callbacks) for parse/eval temporaries. Arena provides LIFO bulk free. |
| ARCH-7 | UTF-8 Strings | All strings in the C core are UTF-8. The host handles encoding conversion at I/O boundaries. |
| ARCH-8 | Return Host-Allocated Objects | Substitution and other operations that produce strings must return host-allocated `TclObj*`, not C-allocated buffers. |
| ARCH-9 | Error Propagation | Always propagate return codes. Check `TclResult` and return early on non-OK. Add error info where appropriate. |

### Code Style Rules (STYLE-*)

| ID | Rule | Description |
|----|------|-------------|
| STYLE-1 | Enums for Constants | Use `enum` for integer constants, not `#define`. Enums are proper C symbols visible to tooling and binding generators. |
| STYLE-2 | Enums for Flags | Use `enum` for flag values and bitmasks. Example: `enum { FLAG_A = 1, FLAG_B = 2, FLAG_C = 4 };` |
| STYLE-3 | Builtin File Naming | Each builtin command should live in its own file: `core/builtin_<command_name>.c` (e.g., `builtin_set.c`, `builtin_proc.c`). |
| STYLE-4 | Common Utilities in base.c | Common utility functions (string helpers, parsing utilities) must be placed in `core/base.c`, not duplicated across files. |
| STYLE-5 | No libc Reimplementation | Do not reimplement libc functions inline. If you need functionality like `strlen`, `memcpy`, `strcmp`, etc., add a utility to `core/base.c`. |

### Duplication Rules (DUP-*)

| ID | Rule | Description |
|----|------|-------------|
| DUP-1 | String Length Calculation | Any code that iterates to find string length should use a shared utility. |
| DUP-2 | Memory Comparison | Any byte-by-byte comparison loops should use a shared utility. |
| DUP-3 | String Comparison | Any character-by-character string comparison should use a shared utility. |
| DUP-4 | Integer Parsing | Any code that parses integers from strings should use a shared utility. |
| DUP-5 | Whitespace Skipping | Any code that skips whitespace should use a shared utility. |
| DUP-6 | Character Classification | Any `isdigit`/`isalpha`/`isspace` style checks should use shared utilities. |

## Review Process

### Step 1: Identify Files to Review

List all C files in `core/`:

```bash
ls -la core/*.c core/*.h
```

### Step 2: Check Each Rule

For each file, check compliance with all rules. Use grep and read tools to examine:

1. **Preprocessor usage**: `grep -n '#define' core/*.c core/*.h`
2. **Allocation calls**: `grep -n 'malloc\|free\|calloc\|realloc' core/*.c`
3. **Forbidden headers**: Check `#include` directives for non-allowed headers
4. **Inline string operations**: Look for loops that replicate `strlen`, `memcpy`, `strcmp`
5. **Builtin organization**: Check if builtins are properly separated into individual files

### Step 3: Generate Scorecard

Produce a scorecard in this format:

```
## Implementation Review Scorecard

### File: core/<filename>.c

| Rule | Status | Notes |
|------|--------|-------|
| ARCH-1 | PASS/FAIL/NA | Details... |
| ARCH-2 | PASS/FAIL/NA | Details... |
| ... | ... | ... |

### Summary

- Total Rules Checked: N
- Passed: N
- Failed: N
- Not Applicable: N
- Compliance: XX%

### Critical Violations

1. [RULE-ID] Description of violation at file:line
2. ...

### Warnings

1. [RULE-ID] Potential issue at file:line
2. ...

### Suggested Next Steps

1. [ ] First action item
2. [ ] Second action item
3. ...
```

## Common Violations to Watch For

### ARCH-1 / STYLE-1 Violations

```c
// VIOLATION: Using #define for constants
#define MAX_WORDS 128
#define TCL_OK 0

// CORRECT: Using enum
enum { MAX_WORDS = 128 };
typedef enum { TCL_OK = 0, TCL_ERROR = 1 } TclResult;
```

### ARCH-2 Violations

```c
// VIOLATION: Direct allocation
char *buf = malloc(size);
free(buf);

// CORRECT: Use host callbacks
char *buf = host->arenaAlloc(arena, size);
// No free needed - arena handles it
```

### ARCH-3 Violations

```c
// VIOLATION: Using forbidden headers
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// CORRECT: Only allowed headers
#include <stddef.h>
#include <stdint.h>
#include <math.h>  // Only for expr.c
```

### STYLE-3 Violations

```c
// VIOLATION: All builtins in one file (builtins.c)
static TclResult cmdSet(...) { ... }
static TclResult cmdProc(...) { ... }
static TclResult cmdIf(...) { ... }

// CORRECT: Separate files
// core/builtin_set.c
// core/builtin_proc.c
// core/builtin_if.c
```

### DUP-* Violations

```c
// VIOLATION: Inline strlen reimplementation
size_t len = 0;
while (s[len] != '\0') len++;

// CORRECT: Use utility from base.c
size_t len = tcl_strlen(s);
```

```c
// VIOLATION: Inline integer parsing
int val = 0;
while (*p >= '0' && *p <= '9') {
    val = val * 10 + (*p - '0');
    p++;
}

// CORRECT: Use utility from base.c
int val;
p = tcl_parse_int(p, &val);
```

## File Locations

| Item | Location |
|------|----------|
| Core C files | `core/*.c` |
| Core headers | `core/*.h`, `core/internal.h`, `core/tclc.h` |
| Base utilities | `core/base.c` |
| Builtin commands | `core/builtin_*.c` |
| Architecture doc | `ARCHITECTURE.md` |

## Utilities Expected in core/base.c

These functions should exist in `core/base.c` for shared use:

```c
// String utilities
size_t tcl_strlen(const char *s);
int tcl_strcmp(const char *a, const char *b);
int tcl_strncmp(const char *a, const char *b, size_t n);
void tcl_memcpy(void *dst, const void *src, size_t n);
void tcl_memset(void *dst, int c, size_t n);
int tcl_memcmp(const void *a, const void *b, size_t n);

// Character classification
int tcl_isdigit(int c);
int tcl_isalpha(int c);
int tcl_isalnum(int c);
int tcl_isspace(int c);

// Parsing utilities
const char *tcl_skip_whitespace(const char *s);
const char *tcl_parse_int(const char *s, int64_t *out);
const char *tcl_parse_double(const char *s, double *out);
```
