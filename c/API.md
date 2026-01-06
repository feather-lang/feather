# Feather C API

This document describes the C API for embedding Feather in C applications.

## Design Principles

1. **Handle-based**: All values are opaque `FeatherObj` handles
2. **Arena allocation**: Objects live until the next `FeatherEval()` call
3. **No manual memory management**: No retain/release, no freeing strings
4. **Minimal**: Core operations only, no convenience macros
5. **Consistent**: All operations take `FeatherInterp` + `FeatherObj` parameters

## API Reference

```c
#ifndef FEATHER_H
#define FEATHER_H

#include <stddef.h>
#include <stdint.h>

typedef size_t FeatherInterp;
typedef size_t FeatherObj;

// =============================================================================
// Lifecycle
// =============================================================================

// Create a new interpreter
FeatherInterp FeatherNew(void);

// Destroy interpreter and free all resources
void FeatherClose(FeatherInterp interp);

// =============================================================================
// Evaluation
// =============================================================================

// Evaluate a script. Returns 0 on success, 1 on error.
// On success, *result contains the result value.
// On error, *result contains the error message.
// Objects from previous eval become invalid.
int FeatherEval(FeatherInterp interp, const char *script, size_t len, FeatherObj *result);

// =============================================================================
// Object Creation
// =============================================================================

// Create string from C data (copies into interpreter)
FeatherObj FeatherString(FeatherInterp interp, const char *s, size_t len);

// Create integer
FeatherObj FeatherInt(FeatherInterp interp, int64_t val);

// Create double
FeatherObj FeatherDouble(FeatherInterp interp, double val);

// Create list from array
FeatherObj FeatherList(FeatherInterp interp, size_t argc, FeatherObj *argv);

// Create empty dict
FeatherObj FeatherDict(FeatherInterp interp);

// =============================================================================
// Type Conversion
// =============================================================================

// Get integer value, or default if conversion fails
int64_t FeatherAsInt(FeatherInterp interp, FeatherObj obj, int64_t def);

// Get double value, or default if conversion fails
double FeatherAsDouble(FeatherInterp interp, FeatherObj obj, double def);

// Get boolean value, or default if conversion fails
int FeatherAsBool(FeatherInterp interp, FeatherObj obj, int def);

// =============================================================================
// String Operations
// =============================================================================

// Get string length in bytes
size_t FeatherLen(FeatherInterp interp, FeatherObj obj);

// Get byte at index, returns -1 if out of bounds
int FeatherByteAt(FeatherInterp interp, FeatherObj obj, size_t index);

// Compare two objects for equality (string comparison)
// Returns 1 if equal, 0 if not equal
int FeatherEq(FeatherInterp interp, FeatherObj a, FeatherObj b);

// Compare two objects (string comparison)
// Returns <0 if a<b, 0 if a==b, >0 if a>b
int FeatherCmp(FeatherInterp interp, FeatherObj a, FeatherObj b);

// Copy string bytes to caller-provided buffer
// Returns number of bytes copied (may be less than len)
size_t FeatherCopy(FeatherInterp interp, FeatherObj obj, char *buf, size_t len);

// =============================================================================
// List Operations
// =============================================================================

// Get list length (0 if not a valid list)
size_t FeatherListLen(FeatherInterp interp, FeatherObj list);

// Get element at index (0 if out of bounds)
FeatherObj FeatherListAt(FeatherInterp interp, FeatherObj list, size_t index);

// Append item to list, returns new list
FeatherObj FeatherListPush(FeatherInterp interp, FeatherObj list, FeatherObj item);

// =============================================================================
// Dict Operations
// =============================================================================

// Get dict size (0 if not a valid dict)
size_t FeatherDictLen(FeatherInterp interp, FeatherObj dict);

// Get value by key (0 if not found)
FeatherObj FeatherDictGet(FeatherInterp interp, FeatherObj dict, FeatherObj key);

// Set key-value pair, returns new dict
FeatherObj FeatherDictSet(FeatherInterp interp, FeatherObj dict, FeatherObj key, FeatherObj val);

// Check if key exists (1 if exists, 0 if not)
int FeatherDictHas(FeatherInterp interp, FeatherObj dict, FeatherObj key);

// Get all keys as a list
FeatherObj FeatherDictKeys(FeatherInterp interp, FeatherObj dict);

// =============================================================================
// Variables
// =============================================================================

// Set a variable by name
void FeatherSetVar(FeatherInterp interp, const char *name, FeatherObj val);

// Get a variable by name (0 if not found)
FeatherObj FeatherGetVar(FeatherInterp interp, const char *name);

// =============================================================================
// Command Registration
// =============================================================================

// Command callback signature
// - data: user data passed to FeatherRegister
// - interp: interpreter handle
// - argc: number of arguments (excluding command name)
// - argv: argument handles (does NOT include command name)
// - result: set to result value on success
// - err: set to error object on failure (create with FeatherString)
// Returns 0 on success, non-zero on error
typedef int (*FeatherCmd)(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err);

// Register a command
void FeatherRegister(FeatherInterp interp, const char *name, FeatherCmd fn, void *data);

// =============================================================================
// Foreign Types
// =============================================================================

// Foreign type create callback - returns new instance
typedef void* (*FeatherForeignNewFunc)(void *userData);

// Foreign type method invoke callback
// - instance: the foreign object instance
// - interp: interpreter handle
// - method: method name being called
// - argc: number of arguments (excluding method name)
// - argv: argument handles
// - result: set to result value on success
// - err: set to error object on failure (create with FeatherString)
// Returns 0 on success, non-zero on error
typedef int (*FeatherForeignInvokeFunc)(void *instance, FeatherInterp interp,
                                         const char *method, size_t argc, FeatherObj *argv,
                                         FeatherObj *result, FeatherObj *err);

// Foreign type destroy callback - frees instance
typedef void (*FeatherForeignDestroyFunc)(void *instance);

// Register a foreign type
void FeatherRegisterForeign(FeatherInterp interp, const char *name,
                            FeatherForeignNewFunc newFn,
                            FeatherForeignInvokeFunc invokeFn,
                            FeatherForeignDestroyFunc destroyFn,
                            void *userData);

// Register a method for a foreign type (for introspection)
void FeatherRegisterForeignMethod(FeatherInterp interp, const char *typeName, const char *methodName);

// =============================================================================
// Parse Info
// =============================================================================

#define FEATHER_PARSE_OK 0
#define FEATHER_PARSE_INCOMPLETE 1
#define FEATHER_PARSE_ERROR 2

// Check parse status of a script
// Returns FEATHER_PARSE_OK, FEATHER_PARSE_INCOMPLETE, or FEATHER_PARSE_ERROR
// On any status, *result contains info about the parse
// On error, *errorObj contains error details
int FeatherParseInfo(FeatherInterp interp, const char *script, size_t len,
                     FeatherObj *result, FeatherObj *errorObj);

#endif
```

## Memory Model

```
┌─────────────────────────────────────────────────────────────────┐
│                        Interpreter                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Arena: all FeatherObj handles point here                  │  │
│  │                                                           │  │
│  │  Cleared at the START of each FeatherEval() call          │  │
│  │                                                           │  │
│  │  Objects created during eval/callbacks live here          │  │
│  │  Result of eval lives here until next eval                │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Permanent: foreign objects only                           │  │
│  │                                                           │  │
│  │  Created via FeatherRegisterForeign                       │  │
│  │  Destroyed via explicit destroy method                    │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

**Validity rules:**
- Handles returned by `FeatherEval()` are valid until the next `FeatherEval()` call
- Handles created during a command callback are valid until the next `FeatherEval()` call
- Handle `0` represents nil/not-found

## Usage Example

```c
#include "feather.h"
#include <stdio.h>
#include <string.h>

// Helper to create error objects
static FeatherObj make_error(FeatherInterp interp, const char *msg) {
    return FeatherString(interp, msg, strlen(msg));
}

// Command: add a b -> returns sum
// Note: argc excludes command name, argv[0] is first argument
int cmd_add(void *data, FeatherInterp interp,
            size_t argc, FeatherObj *argv,
            FeatherObj *result, FeatherObj *err) {
    (void)data;

    if (argc != 2) {
        *err = make_error(interp, "usage: add a b");
        return 1;
    }

    double a = FeatherAsDouble(interp, argv[0], 0.0);
    double b = FeatherAsDouble(interp, argv[1], 0.0);

    *result = FeatherDouble(interp, a + b);
    return 0;
}

// Command: camera subcommand ?args...?
int cmd_camera(void *data, FeatherInterp interp,
               size_t argc, FeatherObj *argv,
               FeatherObj *result, FeatherObj *err) {
    FeatherObj *state = (FeatherObj *)data;

    if (argc < 1) {
        *err = make_error(interp, "usage: camera subcommand ?args?");
        return 1;
    }

    FeatherObj subcmd = argv[0];
    FeatherObj fov_key = FeatherString(interp, "fov", 3);
    FeatherObj pos_key = FeatherString(interp, "position", 8);

    // camera fov ?value?
    if (FeatherEq(interp, subcmd, FeatherString(interp, "fov", 3))) {
        if (argc == 1) {
            *result = FeatherDictGet(interp, *state, fov_key);
            return 0;
        }
        if (argc == 2) {
            *state = FeatherDictSet(interp, *state, fov_key, argv[1]);
            *result = argv[1];
            return 0;
        }
    }

    // camera position x y z
    if (FeatherEq(interp, subcmd, FeatherString(interp, "position", 8))) {
        if (argc == 4) {
            FeatherObj pos = FeatherList(interp, 3, &argv[1]);
            *state = FeatherDictSet(interp, *state, pos_key, pos);
            *result = pos;
            return 0;
        }
    }

    *err = make_error(interp, "unknown subcommand");
    return 1;
}

int main(void) {
    FeatherInterp interp = FeatherNew();

    // Register commands
    FeatherRegister(interp, "add", cmd_add, NULL);

    // Camera state stored as dict
    FeatherObj camera = FeatherDict(interp);
    camera = FeatherDictSet(interp, camera,
                            FeatherString(interp, "fov", 3),
                            FeatherDouble(interp, 45.0));
    FeatherRegister(interp, "camera", cmd_camera, &camera);

    // Evaluate script
    const char *script = "camera fov 60";
    FeatherObj result;
    int status = FeatherEval(interp, script, strlen(script), &result);

    if (status == 0) {
        double fov = FeatherAsDouble(interp, result, 0.0);
        printf("Result: %f\n", fov);
    } else {
        char buf[256];
        size_t n = FeatherCopy(interp, result, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        fprintf(stderr, "Error: %s\n", buf);
    }

    FeatherClose(interp);
    return 0;
}
```

## Implementation Outline

The implementation replaces `cmd/libfeather/exports.go` with a simplified version.

### Changes to Interpreter (`interp_core.go`)

1. **Remove auto-clear from eval**: Remove `defer i.resetScratch()` from `eval()` method

2. **Add public methods**:
   ```go
   // RegisterObj stores an object in the arena, returns handle
   func (i *Interp) RegisterObj(obj *Obj) FeatherObj

   // GetObject retrieves an object by handle
   func (i *Interp) GetObject(h FeatherObj) *Obj

   // ClearArena clears all arena objects (called at start of eval)
   func (i *Interp) ClearArena()
   ```

3. **Use existing scratch arena**: The interpreter already has `scratch` map and `registerObjScratch()` - just expose them

### Changes to Export Layer (`cmd/libfeather/exports.go`)

1. **Remove object storage**: Delete `objects`, `nextObjID`, `objRefCount` from `exportState`

2. **Remove ref counting functions**: Delete `storeObj`, `getObj`, `retainObj`, `releaseObj`

3. **Remove exported ref counting**: Delete `FeatherRetain`, `FeatherRelease`

4. **Simplify exports**: Each export function becomes a thin wrapper:
   ```go
   //export FeatherString
   func FeatherString(interp C.size_t, s *C.char, length C.size_t) C.size_t {
       i := getInterp(interp)
       obj := i.String(C.GoStringN(s, C.int(length)))
       return C.size_t(i.RegisterObj(obj))
   }

   //export FeatherAsDouble
   func FeatherAsDouble(interp C.size_t, obj C.size_t, def C.double) C.double {
       i := getInterp(interp)
       o := i.GetObject(feather.FeatherObj(obj))
       if o == nil {
           return def
       }
       val, err := o.Double()
       if err != nil {
           return def
       }
       return C.double(val)
   }

   //export FeatherEval
   func FeatherEval(interp C.size_t, script *C.char, length C.size_t, result *C.size_t) C.int {
       i := getInterp(interp)
       i.ClearArena() // Clear previous eval's objects

       obj, err := i.Eval(C.GoStringN(script, C.int(length)))
       if err != nil {
           errObj := i.String(err.Error())
           *result = C.size_t(i.RegisterObj(errObj))
           return 1
       }

       *result = C.size_t(i.RegisterObj(obj))
       return 0
   }
   ```

### Migration of `c/tester.c`

1. Update to use new API (most changes are simplifications)
2. Remove `FeatherFreeString()` calls
3. Use `FeatherCopy()` when string bytes are needed for printf/comparison

### Testing

1. `mise test` must pass (uses Go host)
2. `mise test:c` must pass (uses C host via libfeather)
3. Foreign type tests must work (Counter type in tester.c)
