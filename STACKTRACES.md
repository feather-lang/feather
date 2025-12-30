# Plan: Implement Stack Traces for Error Handling

## Problem

Feather's error handling is missing automatic stack trace accumulation. When an error propagates up through procedure calls, TCL should build:

- `-errorinfo`: Human-readable stack trace
- `-errorstack`: Machine-readable `{INNER cmd CALL {proc args} ...}` list
- `-errorline`: Line number where error originated

## Architecture Decision

**Full C implementation.** All stack trace logic lives in the C core:

- State stored as TCL variables in `::tcl::errors` namespace (introspectable)
- Namespace created in `feather_interp_init` (like `::tcl::trace`)
- Helper functions in new `src/error_trace.c`
- No Go changes required

**Benefits:**
- JS/WASM host gets stack traces automatically
- Logic co-located with commands that trigger it
- Follows existing pattern (`::tcl::trace`)
- Portable across all host implementations

---

## Files to Modify/Create

| File | Changes |
|------|---------|
| `src/interp.c` | Create `::tcl::errors` namespace in `feather_interp_init` |
| `src/error_trace.c` | **New file** - helper functions |
| `src/error_trace.h` | **New file** - header declarations |
| `src/builtin_error.c` | Call `feather_error_init` |
| `src/builtin_throw.c` | Call `feather_error_init` |
| `src/builtin_proc.c` | Call `feather_error_append_frame` |
| `src/builtin_catch.c` | Call `feather_error_finalize` |
| `src/builtin_try.c` | Call `feather_error_finalize` |

**No Go files need to be modified.**

---

## Implementation Steps

### Step 1: Create `::tcl::errors` Namespace

**File: `src/interp.c`** - Add to `feather_interp_init`:

```c
void feather_interp_init(const FeatherHostOps *ops, FeatherInterp interp) {
    ops = feather_get_ops(ops);

    // ... existing builtin registration ...

    // ... existing ::tcl::trace setup ...

    // Create ::tcl::errors namespace and initialize error state variables
    FeatherObj errorsNs = ops->string.intern(interp, "::tcl::errors", 13);
    ops->ns.create(interp, errorsNs);

    FeatherObj activeName = ops->string.intern(interp, "active", 6);
    FeatherObj infoName = ops->string.intern(interp, "info", 4);
    FeatherObj stackName = ops->string.intern(interp, "stack", 5);
    FeatherObj lineName = ops->string.intern(interp, "line", 4);

    ops->ns.set_var(interp, errorsNs, activeName, ops->string.intern(interp, "0", 1));
    ops->ns.set_var(interp, errorsNs, infoName, ops->string.intern(interp, "", 0));
    ops->ns.set_var(interp, errorsNs, stackName, ops->list.create(interp));
    ops->ns.set_var(interp, errorsNs, lineName, ops->integer.create(interp, 0));
}
```

### Step 2: Create Error Trace Header

**File: `src/error_trace.h`** (new):

```c
#ifndef FEATHER_ERROR_TRACE_H
#define FEATHER_ERROR_TRACE_H

#include "feather.h"

/**
 * feather_error_is_active checks if error propagation is in progress.
 */
int feather_error_is_active(const FeatherHostOps *ops, FeatherInterp interp);

/**
 * feather_error_init initializes error state when error/throw is called.
 * Sets ::tcl::errors::active to "1" and builds initial errorinfo/errorstack.
 */
void feather_error_init(const FeatherHostOps *ops, FeatherInterp interp,
                        FeatherObj message, FeatherObj cmd, FeatherObj args);

/**
 * feather_error_append_frame appends a stack frame during error propagation.
 * Called when exiting a proc frame with TCL_ERROR.
 */
void feather_error_append_frame(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj procName, FeatherObj args, size_t line);

/**
 * feather_error_finalize copies accumulated error state to return options.
 * Called when catch/try catches the error.
 */
void feather_error_finalize(const FeatherHostOps *ops, FeatherInterp interp);

#endif
```

### Step 3: Create Error Trace Implementation

**File: `src/error_trace.c`** (new):

```c
#include "feather.h"
#include "error_trace.h"
#include "internal.h"

#define S(lit) (lit), feather_strlen(lit)

// Get error namespace variable
static FeatherObj get_error_var(const FeatherHostOps *ops, FeatherInterp interp,
                                const char *name) {
    FeatherObj ns = ops->string.intern(interp, S("::tcl::errors"));
    FeatherObj varName = ops->string.intern(interp, name, feather_strlen(name));
    return ops->ns.get_var(interp, ns, varName);
}

// Set error namespace variable
static void set_error_var(const FeatherHostOps *ops, FeatherInterp interp,
                          const char *name, FeatherObj value) {
    FeatherObj ns = ops->string.intern(interp, S("::tcl::errors"));
    FeatherObj varName = ops->string.intern(interp, name, feather_strlen(name));
    ops->ns.set_var(interp, ns, varName, value);
}

int feather_error_is_active(const FeatherHostOps *ops, FeatherInterp interp) {
    ops = feather_get_ops(ops);
    FeatherObj val = get_error_var(ops, interp, "active");
    return val != 0 && feather_obj_eq_literal(ops, interp, val, "1");
}

void feather_error_init(const FeatherHostOps *ops, FeatherInterp interp,
                        FeatherObj message, FeatherObj cmd, FeatherObj args) {
    ops = feather_get_ops(ops);

    // Set active = "1"
    set_error_var(ops, interp, "active", ops->string.intern(interp, S("1")));

    // Build initial errorinfo: "message\n    while executing\n\"cmd args...\""
    FeatherObj builder = ops->string.builder_new(interp, 256);
    ops->string.builder_append_obj(interp, builder, message);
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\n    while executing\n\"")));
    ops->string.builder_append_obj(interp, builder, cmd);

    size_t argc = ops->list.length(interp, args);
    for (size_t i = 0; i < argc; i++) {
        ops->string.builder_append_byte(interp, builder, ' ');
        ops->string.builder_append_obj(interp, builder, ops->list.at(interp, args, i));
    }
    ops->string.builder_append_byte(interp, builder, '"');

    set_error_var(ops, interp, "info", ops->string.builder_finish(interp, builder));

    // Initialize errorstack: {INNER {cmd args...}}
    FeatherObj stack = ops->list.create(interp);
    stack = ops->list.push(interp, stack, ops->string.intern(interp, S("INNER")));

    FeatherObj callEntry = ops->list.create(interp);
    callEntry = ops->list.push(interp, callEntry, cmd);
    for (size_t i = 0; i < argc; i++) {
        callEntry = ops->list.push(interp, callEntry, ops->list.at(interp, args, i));
    }
    stack = ops->list.push(interp, stack, callEntry);
    set_error_var(ops, interp, "stack", stack);

    // Set errorline from current frame
    size_t line = ops->frame.get_line(interp, ops->frame.level(interp));
    set_error_var(ops, interp, "line", ops->integer.create(interp, (int64_t)line));
}

void feather_error_append_frame(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj procName, FeatherObj args, size_t line) {
    ops = feather_get_ops(ops);

    // Append to errorinfo
    FeatherObj currentInfo = get_error_var(ops, interp, "info");

    FeatherObj builder = ops->string.builder_new(interp, 256);
    ops->string.builder_append_obj(interp, builder, currentInfo);

    // "\n    (procedure \"procName\" line N)"
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\n    (procedure \"")));
    ops->string.builder_append_obj(interp, builder, procName);
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\" line ")));

    // Convert line number to string
    char lineBuf[32];
    size_t lineLen = 0;
    size_t tmp = line;
    do {
        lineBuf[lineLen++] = '0' + (tmp % 10);
        tmp /= 10;
    } while (tmp > 0);
    // Reverse
    for (size_t i = 0; i < lineLen / 2; i++) {
        char c = lineBuf[i];
        lineBuf[i] = lineBuf[lineLen - 1 - i];
        lineBuf[lineLen - 1 - i] = c;
    }
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, lineBuf, lineLen));
    ops->string.builder_append_byte(interp, builder, ')');

    // "\n    invoked from within\n\"procName args...\""
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\n    invoked from within\n\"")));
    ops->string.builder_append_obj(interp, builder, procName);

    size_t argc = ops->list.length(interp, args);
    for (size_t i = 0; i < argc; i++) {
        ops->string.builder_append_byte(interp, builder, ' ');
        ops->string.builder_append_obj(interp, builder, ops->list.at(interp, args, i));
    }
    ops->string.builder_append_byte(interp, builder, '"');

    set_error_var(ops, interp, "info", ops->string.builder_finish(interp, builder));

    // Append CALL to errorstack
    FeatherObj stack = get_error_var(ops, interp, "stack");
    stack = ops->list.push(interp, stack, ops->string.intern(interp, S("CALL")));

    FeatherObj callEntry = ops->list.create(interp);
    callEntry = ops->list.push(interp, callEntry, procName);
    for (size_t i = 0; i < argc; i++) {
        callEntry = ops->list.push(interp, callEntry, ops->list.at(interp, args, i));
    }
    stack = ops->list.push(interp, stack, callEntry);
    set_error_var(ops, interp, "stack", stack);
}

void feather_error_finalize(const FeatherHostOps *ops, FeatherInterp interp) {
    ops = feather_get_ops(ops);

    // Get accumulated state
    FeatherObj info = get_error_var(ops, interp, "info");
    FeatherObj stack = get_error_var(ops, interp, "stack");
    FeatherObj line = get_error_var(ops, interp, "line");

    // Get current return options and add error fields
    FeatherObj opts = ops->interp.get_return_options(interp, TCL_ERROR);
    if (ops->list.is_nil(interp, opts)) {
        opts = ops->list.create(interp);
        opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-code")));
        opts = ops->list.push(interp, opts, ops->integer.create(interp, 1));
    }

    opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-errorinfo")));
    opts = ops->list.push(interp, opts, info);
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-errorstack")));
    opts = ops->list.push(interp, opts, stack);
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-errorline")));
    opts = ops->list.push(interp, opts, line);

    ops->interp.set_return_options(interp, opts);

    // Set global ::errorInfo variable
    FeatherObj globalNs = ops->string.intern(interp, S("::"));
    ops->ns.set_var(interp, globalNs, ops->string.intern(interp, S("errorInfo")), info);

    // Set global ::errorCode variable (from opts or default NONE)
    FeatherObj errorCode = ops->string.intern(interp, S("NONE"));
    // TODO: extract -errorcode from opts if present
    ops->ns.set_var(interp, globalNs, ops->string.intern(interp, S("errorCode")), errorCode);

    // Clear error state
    set_error_var(ops, interp, "active", ops->string.intern(interp, S("0")));
}
```

### Step 4: Modify `builtin_error.c`

**File: `src/builtin_error.c`**:

```c
#include "error_trace.h"

FeatherResult feather_builtin_error(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
    // ... existing argument validation ...

    FeatherObj message = ops->list.at(interp, args, 0);

    // Build return options (existing code)
    // ...

    ops->interp.set_return_options(interp, options);
    ops->interp.set_result(interp, message);

    // Initialize error state if not already active
    if (!feather_error_is_active(ops, interp)) {
        feather_error_init(ops, interp, message, cmd, args);
    }

    return TCL_ERROR;
}
```

### Step 5: Modify `builtin_throw.c`

**File: `src/builtin_throw.c`** - Same pattern as error:

```c
#include "error_trace.h"

// At end of feather_builtin_throw, before return TCL_ERROR:
if (!feather_error_is_active(ops, interp)) {
    feather_error_init(ops, interp, message, cmd, args);
}
```

### Step 6: Modify `builtin_proc.c`

**File: `src/builtin_proc.c`** - In `feather_invoke_proc`:

```c
#include "error_trace.h"

FeatherResult feather_invoke_proc(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj name, FeatherObj args) {
    // ... existing parameter binding ...

    // Evaluate the body
    FeatherResult result = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

    // Append stack frame if error in progress
    if (result == TCL_ERROR && feather_error_is_active(ops, interp)) {
        size_t line = ops->frame.get_line(interp, ops->frame.level(interp));
        feather_error_append_frame(ops, interp, name, args, line);
    }

    // Pop the call frame
    ops->frame.pop(interp);

    // ... existing TCL_RETURN handling ...

    return result;
}
```

### Step 7: Modify `builtin_catch.c`

**File: `src/builtin_catch.c`**:

```c
#include "error_trace.h"

FeatherResult feather_builtin_catch(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
    // ... existing script evaluation ...

    FeatherResult code = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);

    // ... existing TCL_RETURN handling ...

    // Finalize error state before getting options
    if (code == TCL_ERROR && feather_error_is_active(ops, interp)) {
        feather_error_finalize(ops, interp);
    }

    // Get the result and options (existing code)
    FeatherObj result = ops->interp.get_result(interp);
    // ...
}
```

### Step 8: Modify `builtin_try.c`

**File: `src/builtin_try.c`** - Same pattern, finalize before handling error:

```c
#include "error_trace.h"

// When an error is caught (before executing handler):
if (code == TCL_ERROR && feather_error_is_active(ops, interp)) {
    feather_error_finalize(ops, interp);
}
```

---

## TCL Introspection

Scripts can inspect error state during propagation:

```tcl
# Check if error propagation is active
set ::tcl::errors::active

# Read accumulated stack trace so far
set ::tcl::errors::info

# Read errorstack list
set ::tcl::errors::stack

# Read original error line
set ::tcl::errors::line
```

---

## Expected Output

```tcl
proc foo {} { bar }
proc bar {} { error "oops" }
catch {foo} result opts
puts [dict get $opts -errorinfo]
```

Output:
```
oops
    while executing
"error oops"
    (procedure "bar" line 1)
    invoked from within
"bar"
    (procedure "foo" line 1)
    invoked from within
"foo"
```

`-errorstack`: `{INNER {error oops} CALL {bar} CALL {foo}}`

---

## Testing

1. Add test case in `testcases/` for stack traces
2. Verify `-errorinfo` format matches TCL 8.5+
3. Verify `-errorstack` contains CALL/INNER entries
4. Verify `-errorline` is populated
5. Verify `::errorInfo` and `::errorCode` globals are set
6. Verify `::tcl::errors::*` variables are accessible
7. Run `mise test` and `mise test:js` to ensure both hosts work
