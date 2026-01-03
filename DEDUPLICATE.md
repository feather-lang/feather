# Code Deduplication Plan for src/

This document outlines a refactoring plan to eliminate code duplication in the `src/` directory.

## Desired End-State

After completing this refactoring:

1. **Single source of truth for character classification**
   - All character predicates (`is_whitespace`, `is_hex_digit`, `is_varname_char`, etc.) live in `charclass.h`
   - No file defines its own local versions of these predicates
   - Two whitespace variants exist: `feather_is_whitespace()` (space/tab only for TCL word separation) and `feather_is_whitespace_full()` (all 6 whitespace chars for string operations)

2. **Single source of truth for hex digit conversion**
   - `feather_hex_value()` in `charclass.h` is used everywhere
   - No local `*_hex_value()` functions exist

3. **Unified boolean condition evaluation**
   - A single `feather_eval_bool_condition()` function handles expr-based boolean evaluation
   - `builtin_if.c`, `builtin_while.c`, `builtin_for.c` all use this shared function
   - `feather_obj_to_bool_literal()` is consistently used for literal boolean parsing

4. **Shared error message construction**
   - Common error patterns like "expected X but got Y" use helper functions
   - Reduces boilerplate and ensures consistent error formatting

5. **Shared iteration logic for foreach/lmap**
   - Common loop setup, variable binding, and break/continue handling extracted
   - `foreach` and `lmap` differ only in result accumulation

6. **Shared UTF-8 encoding/decoding**
   - UTF-8 functions available in a shared header for any file that needs them

---

## Refactoring Steps

### R1: Remove duplicate `is_command_terminator()` from parse.c

**Files:** `src/parse.c`

**Current state:** `parse.c` defines `is_command_terminator()` at line 12-14, but already includes `charclass.h` which has identical `feather_is_command_terminator()`.

**Changes:**
1. Delete `is_command_terminator()` function (lines 12-14)
2. Delete `is_word_terminator()` function (lines 16-18) - use `feather_is_word_terminator()` instead
3. Replace all calls to `is_command_terminator()` with `feather_is_command_terminator()`
4. Replace all calls to `is_word_terminator()` with `feather_is_word_terminator()`

**Verification:** `mise test` passes

---

### R2: Remove duplicate hex digit functions

**Files:** `src/parse.c`, `src/builtin_subst.c`, `src/builtin_scan.c`

**Current state:** Three files define their own `*_is_hex_digit()` and `*_hex_value()` functions identical to those in `charclass.h`.

**Changes:**

1. In `src/parse.c`:
   - Delete `parse_is_hex_digit()` (lines 20-22)
   - Delete `parse_hex_value()` (lines 39-44)
   - Replace calls with `feather_is_hex_digit()` and `feather_hex_value()`

2. In `src/builtin_subst.c`:
   - Add `#include "charclass.h"` if not present
   - Delete `subst_is_hex_digit()` (lines 74-76)
   - Delete `subst_hex_value()` (lines 78-83)
   - Replace calls with `feather_is_hex_digit()` and `feather_hex_value()`

3. In `src/builtin_scan.c`:
   - Verify `#include "charclass.h"` is present
   - Delete `scan_is_hex_digit()` (lines 58-60)
   - Delete `scan_hex_value()` (lines 66-70)
   - Replace calls with `feather_is_hex_digit()` and `feather_hex_value()`

**Verification:** `mise test` passes

---

### R3: Remove duplicate varname character functions

**Files:** `src/parse.c`, `src/builtin_expr.c`, `src/builtin_subst.c`

**Current state:** Three files define their own varname character predicates identical to `feather_is_varname_char()` in `charclass.h`.

**Changes:**

1. In `src/parse.c`:
   - Delete `is_varname_char_base()` (lines 25-28)
   - Replace calls with `feather_is_varname_char()`

2. In `src/builtin_expr.c`:
   - Add `#include "charclass.h"` if not present
   - Delete `is_varname_char()` (around line 311)
   - Replace calls with `feather_is_varname_char()`

3. In `src/builtin_subst.c`:
   - Delete `subst_is_varname_char()` (around line 272)
   - Replace calls with `feather_is_varname_char()`

**Verification:** `mise test` passes

---

### R4: Add `feather_is_whitespace_full()` to charclass.h

**Files:** `src/charclass.h`

**Current state:** `feather_is_whitespace()` only checks space/tab. Multiple files define their own "full" whitespace functions that include newlines, vertical tab, and form feed.

**Changes:**

1. Add to `charclass.h` after `feather_is_whitespace()`:
   ```c
   /* Full whitespace check including all TCL whitespace characters.
    * Use this for string operations (trim, scan, etc.).
    * Use feather_is_whitespace() for word separation in parsing. */
   static inline int feather_is_whitespace_full(int ch) {
     return ch == ' ' || ch == '\t' || ch == '\n' ||
            ch == '\r' || ch == '\v' || ch == '\f';
   }
   ```

**Verification:** Build succeeds

---

### R5: Remove duplicate full whitespace functions

**Files:** `src/builtin_string.c`, `src/builtin_scan.c`, `src/builtin_concat.c`

**Depends on:** R4

**Current state:** Three files define their own whitespace predicates.

**Changes:**

1. In `src/builtin_string.c`:
   - Add `#include "charclass.h"` if not present
   - Delete `string_is_whitespace()` (lines 6-8)
   - Replace calls with `feather_is_whitespace_full()`

2. In `src/builtin_scan.c`:
   - Delete `scan_is_whitespace()` (lines 54-56)
   - Replace calls with `feather_is_whitespace_full()`

3. In `src/builtin_concat.c`:
   - Add `#include "charclass.h"`
   - Delete `concat_is_whitespace()` (lines 5-7)
   - Replace calls with `feather_is_whitespace_full()`

4. In `src/builtin_expr.c`:
   - Update `expr_skip_whitespace()` to use `feather_is_whitespace_full()` for the character check (keeping the comment-skipping logic)

**Verification:** `mise test` passes

---

### R6: Update builtin_if.c to use `feather_obj_to_bool_literal()`

**Files:** `src/builtin_if.c`

**Current state:** `eval_condition()` manually checks for `true/false/yes/no` with 4 separate `feather_obj_eq_literal()` calls instead of using the existing `feather_obj_to_bool_literal()` helper.

**Changes:**

1. Replace lines 20-36 in `eval_condition()`:
   ```c
   // Before (manual checks):
   if (feather_obj_eq_literal(ops, interp, resultObj, "true")) {
     *result = 1;
     return TCL_OK;
   }
   if (feather_obj_eq_literal(ops, interp, resultObj, "false")) {
     *result = 0;
     return TCL_OK;
   }
   // ... etc for yes/no
   ```

   With:
   ```c
   // After (use helper):
   if (feather_obj_to_bool_literal(ops, interp, resultObj, result)) {
     return TCL_OK;
   }
   ```

**Verification:** `mise test` passes

---

### R7: Extract shared `feather_eval_bool_condition()` function

**Files:** `src/internal.h`, `src/builtin_if.c`, `src/builtin_while.c`, `src/builtin_for.c`, new `src/eval_helpers.c`

**Depends on:** R6

**Current state:** Three nearly identical `eval_*_condition()` functions exist in `builtin_if.c`, `builtin_while.c`, and `builtin_for.c`.

**Changes:**

1. Add declaration to `src/internal.h`:
   ```c
   /**
    * feather_eval_bool_condition evaluates an expression and converts to boolean.
    *
    * Calls expr builtin, then checks for boolean literals (true/false/yes/no)
    * or converts integer result to boolean (0 = false, non-zero = true).
    *
    * On success, stores 0 or 1 in *result and returns TCL_OK.
    * On error (invalid boolean), sets error message and returns TCL_ERROR.
    */
   FeatherResult feather_eval_bool_condition(const FeatherHostOps *ops,
                                              FeatherInterp interp,
                                              FeatherObj condition,
                                              int *result);
   ```

2. Create `src/eval_helpers.c` with the implementation (move from `builtin_while.c`)

3. In `src/builtin_if.c`:
   - Delete `eval_condition()` function
   - Replace calls with `feather_eval_bool_condition()`

4. In `src/builtin_while.c`:
   - Delete `eval_while_condition()` function
   - Replace calls with `feather_eval_bool_condition()`

5. In `src/builtin_for.c`:
   - Delete `eval_for_condition()` function
   - Replace calls with `feather_eval_bool_condition()`

**Verification:** `mise test` passes

---

### R8: Add `feather_error_expected()` helper

**Files:** `src/internal.h`

**Current state:** 21+ call sites construct "expected X but got Y" error messages with identical 3-part concatenation boilerplate.

**Changes:**

1. Add to `src/internal.h`:
   ```c
   /**
    * feather_error_expected constructs an error message of the form:
    * "expected <type> but got \"<value>\""
    *
    * Sets the interpreter result to the error message.
    */
   static inline void feather_error_expected(const FeatherHostOps *ops,
                                              FeatherInterp interp,
                                              const char *type,
                                              FeatherObj got) {
     // Calculate length: "expected " + type + " but got \"" = 9 + len + 10 = 19 + len
     size_t type_len = feather_strlen(type);
     char prefix[64];
     // Build "expected <type> but got \""
     size_t i = 0;
     const char *p1 = "expected ";
     while (*p1) prefix[i++] = *p1++;
     const char *t = type;
     while (*t) prefix[i++] = *t++;
     const char *p2 = " but got \"";
     while (*p2) prefix[i++] = *p2++;
     prefix[i] = '\0';

     FeatherObj part1 = ops->string.intern(interp, prefix, i);
     FeatherObj part3 = ops->string.intern(interp, "\"", 1);
     FeatherObj msg = ops->string.concat(interp, part1, got);
     msg = ops->string.concat(interp, msg, part3);
     ops->interp.set_result(interp, msg);
   }
   ```

**Verification:** Build succeeds

---

### R9: Use `feather_error_expected()` throughout codebase

**Files:** Multiple builtin files

**Depends on:** R8

**Current state:** 21+ files have the 3-part error concatenation pattern.

**Changes:** Replace pattern in each file:
```c
// Before:
FeatherObj part1 = ops->string.intern(interp, "expected integer but got \"", 26);
FeatherObj part3 = ops->string.intern(interp, "\"", 1);
FeatherObj msg = ops->string.concat(interp, part1, value);
msg = ops->string.concat(interp, msg, part3);
ops->interp.set_result(interp, msg);

// After:
feather_error_expected(ops, interp, "integer", value);
```

**Files to update:**
- `builtin_string.c` (3 occurrences)
- `builtin_format.c` (2 occurrences)
- `builtin_dict.c` (2 occurrences)
- `builtin_incr.c` (2 occurrences)
- `builtin_info.c` (2 occurrences)
- `builtin_return.c` (4 occurrences)
- `builtin_lrepeat.c` (1 occurrence)
- `builtin_expr.c` (1 occurrence)
- `builtin_mathfunc.c` (1 occurrence)
- `eval_helpers.c` (1 occurrence - for boolean errors after R7)

**Verification:** `mise test` passes

---

### R10: Extract shared foreach/lmap iteration logic

**Files:** `src/builtin_foreach.c`, `src/builtin_lmap.c`, `src/internal.h`

**Current state:** `foreach` and `lmap` share ~70 lines of identical iteration setup, variable binding, and loop control handling. They differ only in result accumulation.

**Changes:**

1. Add callback type and helper to `src/internal.h`:
   ```c
   /**
    * Callback invoked after each iteration body executes successfully.
    * For lmap: appends body result to accumulator list.
    * For foreach: does nothing (NULL callback).
    */
   typedef void (*FeatherIterCallback)(const FeatherHostOps *ops,
                                        FeatherInterp interp,
                                        FeatherObj bodyResult,
                                        void *ctx);

   /**
    * feather_foreach_impl implements the shared foreach/lmap iteration logic.
    *
    * cmdName: "foreach" or "lmap" (for error messages)
    * callback: called after each successful body evaluation (NULL to skip)
    * ctx: passed to callback
    */
   FeatherResult feather_foreach_impl(const FeatherHostOps *ops,
                                       FeatherInterp interp,
                                       FeatherObj args,
                                       const char *cmdName,
                                       FeatherIterCallback callback,
                                       void *ctx);
   ```

2. Create implementation in `src/eval_helpers.c` (or new `src/iter_helpers.c`)

3. Simplify `builtin_foreach.c`:
   ```c
   FeatherResult feather_builtin_foreach(...) {
     (void)cmd;
     FeatherResult rc = feather_foreach_impl(ops, interp, args, "foreach", NULL, NULL);
     if (rc == TCL_OK) {
       ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
     }
     return rc;
   }
   ```

4. Simplify `builtin_lmap.c`:
   ```c
   static void lmap_callback(const FeatherHostOps *ops, FeatherInterp interp,
                              FeatherObj bodyResult, void *ctx) {
     FeatherObj *result = (FeatherObj *)ctx;
     ops->list.push(interp, *result, bodyResult);
   }

   FeatherResult feather_builtin_lmap(...) {
     (void)cmd;
     FeatherObj result = ops->list.create(interp);
     FeatherResult rc = feather_foreach_impl(ops, interp, args, "lmap", lmap_callback, &result);
     if (rc == TCL_OK) {
       ops->interp.set_result(interp, result);
     }
     return rc;
   }
   ```

**Verification:** `mise test` passes

---

### R11: Move UTF-8 functions to shared header

**Files:** `src/unicode.h` (new), `src/builtin_subst.c`, `src/builtin_scan.c`

**Current state:** UTF-8 encode/decode functions are defined locally in `builtin_subst.c` and `builtin_scan.c`.

**Changes:**

1. Create `src/unicode.h`:
   ```c
   #ifndef FEATHER_UNICODE_H
   #define FEATHER_UNICODE_H

   #include <stdint.h>
   #include <stddef.h>

   /**
    * Encode a Unicode codepoint as UTF-8.
    * Returns number of bytes written (1-4).
    * Invalid codepoints produce U+FFFD replacement character.
    */
   static inline size_t feather_utf8_encode(uint32_t codepoint, char *buf) {
     // ... (move from builtin_subst.c)
   }

   /**
    * Decode a UTF-8 codepoint from a byte sequence.
    * Returns codepoint value, or -1 on error.
    * Sets *bytes_read to number of bytes consumed.
    */
   static inline int64_t feather_utf8_decode(const unsigned char *buf,
                                              size_t len,
                                              size_t *bytes_read) {
     // ... (simplified version of decode logic)
   }

   #endif
   ```

2. Update `builtin_subst.c`:
   - Add `#include "unicode.h"`
   - Delete local `encode_utf8()` function
   - Replace calls with `feather_utf8_encode()`

3. Update `builtin_scan.c`:
   - Add `#include "unicode.h"`
   - Refactor `decode_utf8_at_pos()` to use `feather_utf8_decode()` or keep as wrapper

**Verification:** `mise test` passes

---

## Summary

| Step | Description | Lines Removed | Complexity |
|------|-------------|---------------|------------|
| R1 | Remove duplicate terminators from parse.c | ~6 | Easy |
| R2 | Remove duplicate hex functions | ~24 | Easy |
| R3 | Remove duplicate varname functions | ~12 | Easy |
| R4 | Add `feather_is_whitespace_full()` | +6 | Easy |
| R5 | Remove duplicate whitespace functions | ~12 | Easy |
| R6 | Use `feather_obj_to_bool_literal()` in if.c | ~14 | Easy |
| R7 | Extract `feather_eval_bool_condition()` | ~80 | Medium |
| R8 | Add `feather_error_expected()` helper | +15 | Easy |
| R9 | Use error helper throughout | ~80 | Easy (tedious) |
| R10 | Extract foreach/lmap shared logic | ~60 | Medium |
| R11 | Move UTF-8 to shared header | ~0 (reorganize) | Easy |

**Total estimated reduction:** ~290 lines of duplicated code

**Recommended order:** R1-R6 (quick wins), then R7-R9 (error handling), then R10-R11 (larger refactors)
