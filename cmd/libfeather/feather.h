/*
 * feather.h - C API for the Feather TCL interpreter
 *
 * Feather is a small, embeddable TCL interpreter. This header provides
 * the C interface for embedding Feather in applications.
 *
 * Build the shared library with:
 *   go build -buildmode=c-shared -o libfeather.so ./cmd/libfeather
 *
 * Link with: -lfeather -L/path/to/lib
 */

#ifndef FEATHER_H
#define FEATHER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Interpreter handle */
typedef size_t FeatherInterp;

/* Object handle - references a TCL value (string, int, list, dict, etc.) */
typedef size_t FeatherObj;

/* Parse status codes */
typedef enum {
    FEATHER_PARSE_OK         = 0,
    FEATHER_PARSE_INCOMPLETE = 1,
    FEATHER_PARSE_ERROR      = 2
} FeatherParseStatus;

/* Result codes for FeatherEval and FeatherCall */
typedef enum {
    FEATHER_OK    = 0,
    FEATHER_ERROR = 1
} FeatherResult;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/*
 * Command callback for custom commands registered from C.
 *
 * Parameters:
 *   data    - User data pointer passed to FeatherRegister
 *   interp  - Interpreter handle
 *   argc    - Number of arguments (not including command name)
 *   argv    - Argument handles
 *   result  - Output: result handle on success
 *   err     - Output: error handle on failure
 *
 * Returns: 0 on success, non-zero on error
 */
typedef int (*FeatherCmd)(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err);

/*
 * Foreign type callbacks for custom object types.
 */
typedef void* (*FeatherForeignNewFunc)(void *userData);
typedef int (*FeatherForeignInvokeFunc)(void *instance, FeatherInterp interp,
                                        const char *method, size_t argc,
                                        FeatherObj *argv, FeatherObj *result,
                                        FeatherObj *err);
typedef void (*FeatherForeignDestroyFunc)(void *instance);

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/*
 * Create a new interpreter instance.
 * Returns: Interpreter handle, or 0 on failure
 */
FeatherInterp FeatherNew(void);

/*
 * Close an interpreter and free all resources.
 */
void FeatherClose(FeatherInterp interp);

/* ============================================================================
 * Evaluation
 * ============================================================================ */

/*
 * Check if a script is syntactically complete.
 *
 * Returns: FEATHER_PARSE_OK, FEATHER_PARSE_INCOMPLETE, or FEATHER_PARSE_ERROR
 */
FeatherParseStatus FeatherParse(FeatherInterp interp, const char *script, size_t length);

/*
 * Parse with detailed information.
 *
 * Parameters:
 *   result   - Output: parse result info (e.g., "{INCOMPLETE 5 17}")
 *   errorObj - Output: error message on parse error
 *
 * Returns: FEATHER_PARSE_OK, FEATHER_PARSE_INCOMPLETE, or FEATHER_PARSE_ERROR
 */
FeatherParseStatus FeatherParseInfo(FeatherInterp interp, const char *script, size_t length,
                                    FeatherObj *result, FeatherObj *errorObj);

/*
 * Evaluate a TCL script.
 *
 * Parameters:
 *   script - TCL script to evaluate
 *   length - Length of script in bytes
 *   result - Output: result handle (valid until next FeatherEval/FeatherCall)
 *
 * Returns: FEATHER_OK on success, FEATHER_ERROR on failure
 *          On error, result contains the error message
 */
FeatherResult FeatherEval(FeatherInterp interp, const char *script, size_t length,
                          FeatherObj *result);

/*
 * Call a TCL command with handle arguments.
 *
 * Unlike FeatherEval, this passes arguments directly without TCL parsing,
 * so strings with special characters (unbalanced braces, $, [, etc.) are
 * handled correctly without escaping.
 *
 * Parameters:
 *   argc   - Number of elements in argv (must be >= 1)
 *   argv   - Array of handles: argv[0] is command name, rest are arguments
 *   result - Output: result handle (valid until next FeatherEval/FeatherCall)
 *
 * Returns: FEATHER_OK on success, FEATHER_ERROR on failure
 *          On error, result contains the error message
 *
 * Example:
 *   FeatherObj argv[] = {
 *       FeatherString(interp, "list", 4),
 *       FeatherString(interp, "hello { world", 13),  // unbalanced brace OK
 *       FeatherInt(interp, 42)
 *   };
 *   FeatherObj result;
 *   if (FeatherCall(interp, 3, argv, &result) == FEATHER_OK) {
 *       // result is a list: {hello { world} 42
 *   }
 */
FeatherResult FeatherCall(FeatherInterp interp, size_t argc, FeatherObj *argv,
                          FeatherObj *result);

/* ============================================================================
 * Object Creation
 * ============================================================================ */

/*
 * Create a string object.
 */
FeatherObj FeatherString(FeatherInterp interp, const char *s, size_t length);

/*
 * Create an integer object.
 */
FeatherObj FeatherInt(FeatherInterp interp, int64_t val);

/*
 * Create a double object.
 */
FeatherObj FeatherDouble(FeatherInterp interp, double val);

/*
 * Create a list object from an array of handles.
 */
FeatherObj FeatherList(FeatherInterp interp, size_t argc, FeatherObj *argv);

/*
 * Create an empty dict object.
 */
FeatherObj FeatherDict(FeatherInterp interp);

/* ============================================================================
 * Type Conversion
 * ============================================================================ */

/*
 * Get integer value from an object.
 * Returns def if conversion fails.
 */
int64_t FeatherAsInt(FeatherInterp interp, FeatherObj obj, int64_t def);

/*
 * Get double value from an object.
 * Returns def if conversion fails.
 */
double FeatherAsDouble(FeatherInterp interp, FeatherObj obj, double def);

/*
 * Get boolean value from an object.
 * Returns def if conversion fails.
 */
int FeatherAsBool(FeatherInterp interp, FeatherObj obj, int def);

/* ============================================================================
 * String Operations
 * ============================================================================ */

/*
 * Get string length in bytes.
 */
size_t FeatherLen(FeatherInterp interp, FeatherObj obj);

/*
 * Get byte at index.
 * Returns -1 if index is out of bounds.
 */
int FeatherByteAt(FeatherInterp interp, FeatherObj obj, size_t index);

/*
 * Compare two objects for equality.
 * Returns 1 if equal, 0 otherwise.
 */
int FeatherEq(FeatherInterp interp, FeatherObj a, FeatherObj b);

/*
 * Compare two objects lexicographically.
 * Returns -1 if a < b, 0 if a == b, 1 if a > b.
 */
int FeatherCmp(FeatherInterp interp, FeatherObj a, FeatherObj b);

/*
 * Copy string bytes to a buffer.
 * Returns number of bytes copied (may be less than string length if buffer is small).
 */
size_t FeatherCopy(FeatherInterp interp, FeatherObj obj, char *buf, size_t length);

/* ============================================================================
 * List Operations
 * ============================================================================ */

/*
 * Get number of elements in a list.
 */
size_t FeatherListLen(FeatherInterp interp, FeatherObj list);

/*
 * Get element at index.
 * Returns 0 if index is out of bounds or obj is not a list.
 */
FeatherObj FeatherListAt(FeatherInterp interp, FeatherObj list, size_t index);

/*
 * Create a new list with item appended.
 * Returns the new list (original is unchanged).
 */
FeatherObj FeatherListPush(FeatherInterp interp, FeatherObj list, FeatherObj item);

/* ============================================================================
 * Dict Operations
 * ============================================================================ */

/*
 * Get number of key-value pairs in a dict.
 */
size_t FeatherDictLen(FeatherInterp interp, FeatherObj dict);

/*
 * Get value for a key.
 * Returns 0 if key does not exist.
 */
FeatherObj FeatherDictGet(FeatherInterp interp, FeatherObj dict, FeatherObj key);

/*
 * Create a new dict with key set to value.
 * Returns the new dict (original is unchanged).
 */
FeatherObj FeatherDictSet(FeatherInterp interp, FeatherObj dict,
                          FeatherObj key, FeatherObj value);

/*
 * Check if dict contains a key.
 * Returns 1 if key exists, 0 otherwise.
 */
int FeatherDictHas(FeatherInterp interp, FeatherObj dict, FeatherObj key);

/*
 * Get list of all keys in a dict.
 */
FeatherObj FeatherDictKeys(FeatherInterp interp, FeatherObj dict);

/* ============================================================================
 * Variables
 * ============================================================================ */

/*
 * Set a variable value.
 */
void FeatherSetVar(FeatherInterp interp, const char *name, FeatherObj val);

/*
 * Get a variable value.
 * Returns 0 if variable does not exist or is empty.
 */
FeatherObj FeatherGetVar(FeatherInterp interp, const char *name);

/* ============================================================================
 * Command Registration
 * ============================================================================ */

/*
 * Register a custom command implemented in C.
 */
void FeatherRegister(FeatherInterp interp, const char *name,
                     FeatherCmd fn, void *data);

/* ============================================================================
 * Foreign Type Registration
 * ============================================================================ */

/*
 * Register a foreign type with constructor and method callbacks.
 *
 * After registration, TCL code can create instances with:
 *   set obj [TypeName new]
 *   $obj methodName arg1 arg2
 *   $obj destroy
 *
 * Returns: FEATHER_OK on success, FEATHER_ERROR on failure
 */
FeatherResult FeatherRegisterForeign(FeatherInterp interp, const char *typeName,
                                     FeatherForeignNewFunc newFn,
                                     FeatherForeignInvokeFunc invokeFn,
                                     FeatherForeignDestroyFunc destroyFn,
                                     void *userData);

/*
 * Register a method name for a foreign type (for introspection).
 *
 * Returns: FEATHER_OK on success, FEATHER_ERROR on failure
 */
FeatherResult FeatherRegisterForeignMethod(FeatherInterp interp, const char *typeName,
                                           const char *methodName);

#ifdef __cplusplus
}
#endif

#endif /* FEATHER_H */
