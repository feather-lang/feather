/**
 * @file feather_api.h
 * @brief C API for working with Feather interpreter objects directly
 *
 * This header provides a C-idiomatic interface to Feather's core data types.
 * All operations work with opaque FeatherObj handles, allowing C code to
 * manipulate lists, dicts, strings, and numbers without string conversions.
 *
 * Usage Pattern:
 *   - Create objects with feather_*_create() functions
 *   - Manipulate objects using type-specific operations
 *   - Extract primitive C values using feather_*_get() or feather_*_data()
 *   - Objects remain valid until the interpreter is closed
 *
 * Example:
 *   FeatherObj list = feather_list_create(interp);
 *   feather_list_push(interp, list, feather_int_create(interp, 42));
 *   FeatherObj elem = feather_list_at(interp, list, 0);
 *   int64_t val;
 *   if (feather_int_get(interp, elem, &val) == FEATHER_OK) {
 *       printf("Value: %lld\n", val);
 *   }
 */

#ifndef FEATHER_API_H
#define FEATHER_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

/** Opaque interpreter handle */
typedef size_t FeatherInterp;

/** Opaque object handle - represents any Feather value */
typedef size_t FeatherObj;

/** Result codes for operations that can fail */
typedef enum {
    FEATHER_OK = 0,     /**< Operation succeeded */
    FEATHER_ERROR = 1   /**< Operation failed */
} FeatherResult;

/* ==========================================================================
 * Integer Operations
 * ========================================================================== */

/**
 * Create an integer object.
 *
 * @param interp  Interpreter handle
 * @param val     Integer value
 * @return        New integer object handle, or 0 on error
 */
FeatherObj feather_int_create(FeatherInterp interp, int64_t val);

/**
 * Extract integer value from an object.
 *
 * The object will be shimmered to integer representation if needed.
 * Fails if the object cannot be interpreted as an integer.
 *
 * @param interp  Interpreter handle
 * @param obj     Object to convert
 * @param out     Pointer to receive the integer value
 * @return        FEATHER_OK on success, FEATHER_ERROR if not an integer
 */
FeatherResult feather_int_get(FeatherInterp interp, FeatherObj obj, int64_t *out);

/* ==========================================================================
 * Double (Floating-Point) Operations
 * ========================================================================== */

/**
 * Create a double (floating-point) object.
 *
 * @param interp  Interpreter handle
 * @param val     Double value
 * @return        New double object handle, or 0 on error
 */
FeatherObj feather_double_create(FeatherInterp interp, double val);

/**
 * Extract double value from an object.
 *
 * The object will be shimmered to double representation if needed.
 *
 * @param interp  Interpreter handle
 * @param obj     Object to convert
 * @param out     Pointer to receive the double value
 * @return        FEATHER_OK on success, FEATHER_ERROR if not a double
 */
FeatherResult feather_double_get(FeatherInterp interp, FeatherObj obj, double *out);

/** Classification of double values */
typedef enum {
    FEATHER_DBL_NORMAL = 0,     /**< Normal finite number */
    FEATHER_DBL_SUBNORMAL = 1,  /**< Subnormal (denormalized) number */
    FEATHER_DBL_ZERO = 2,       /**< Zero (positive or negative) */
    FEATHER_DBL_INF = 3,        /**< Positive infinity */
    FEATHER_DBL_NEG_INF = 4,    /**< Negative infinity */
    FEATHER_DBL_NAN = 5         /**< Not a number */
} FeatherDoubleClass;

/**
 * Classify a double value.
 *
 * @param val  Double value to classify
 * @return     Classification enum value
 */
FeatherDoubleClass feather_double_classify(double val);

/**
 * Format a double as a string object.
 *
 * @param interp    Interpreter handle
 * @param val       Value to format
 * @param spec      Format specifier: 'e', 'E', 'f', 'F', 'g', 'G'
 * @param precision Number of decimal places (-1 for default)
 * @param alt       Non-zero for alternate form (always show decimal point)
 * @return          String object with formatted number
 */
FeatherObj feather_double_format(FeatherInterp interp, double val, char spec, int precision, int alt);

/** Math operation codes for feather_double_math */
typedef enum {
    FEATHER_MATH_SQRT = 0,
    FEATHER_MATH_EXP = 1,
    FEATHER_MATH_LOG = 2,
    FEATHER_MATH_LOG10 = 3,
    FEATHER_MATH_SIN = 4,
    FEATHER_MATH_COS = 5,
    FEATHER_MATH_TAN = 6,
    FEATHER_MATH_ASIN = 7,
    FEATHER_MATH_ACOS = 8,
    FEATHER_MATH_ATAN = 9,
    FEATHER_MATH_SINH = 10,
    FEATHER_MATH_COSH = 11,
    FEATHER_MATH_TANH = 12,
    FEATHER_MATH_FLOOR = 13,
    FEATHER_MATH_CEIL = 14,
    FEATHER_MATH_ROUND = 15,
    FEATHER_MATH_ABS = 16,
    FEATHER_MATH_POW = 17,      /* Binary: a^b */
    FEATHER_MATH_ATAN2 = 18,    /* Binary: atan2(a, b) */
    FEATHER_MATH_FMOD = 19,     /* Binary: fmod(a, b) */
    FEATHER_MATH_HYPOT = 20     /* Binary: hypot(a, b) */
} FeatherMathOp;

/**
 * Perform a math operation on doubles.
 *
 * For unary operations, parameter b is ignored.
 *
 * @param interp  Interpreter handle
 * @param op      Math operation to perform
 * @param a       First operand
 * @param b       Second operand (for binary ops only)
 * @param out     Pointer to receive the result
 * @return        FEATHER_OK on success, FEATHER_ERROR on failure
 */
FeatherResult feather_double_math(FeatherInterp interp, FeatherMathOp op, double a, double b, double *out);

/* ==========================================================================
 * String Operations
 * ========================================================================== */

/**
 * Create a string object from C data.
 *
 * @param interp  Interpreter handle
 * @param s       Pointer to string data
 * @param len     Length in bytes (use strlen() for null-terminated strings)
 * @return        New string object handle, or 0 on error
 */
FeatherObj feather_string_create(FeatherInterp interp, const char *s, size_t len);

/**
 * Get string content as a newly allocated C string.
 *
 * The caller is responsible for freeing the returned string with
 * feather_string_free().
 *
 * @param interp  Interpreter handle
 * @param str     String object
 * @return        Newly allocated null-terminated C string (must be freed)
 */
char *feather_string_get(FeatherInterp interp, FeatherObj str);

/**
 * Get string content without copying.
 *
 * Returns a pointer directly into the object's storage. The pointer
 * is valid until the next operation that might modify the object.
 *
 * @param interp  Interpreter handle
 * @param str     String object
 * @param len     Pointer to receive the byte length (may be NULL)
 * @return        Pointer to string data (NOT null-terminated in general)
 */
const char *feather_string_data(FeatherInterp interp, FeatherObj str, size_t *len);

/**
 * Free a string returned by feather_string_get().
 *
 * @param s  String to free (may be NULL)
 */
void feather_string_free(char *s);

/**
 * Get the byte at a specific index.
 *
 * @param interp  Interpreter handle
 * @param str     String object
 * @param index   Byte index (0-based)
 * @return        Byte value (0-255), or -1 if index is out of bounds
 */
int feather_string_byte_at(FeatherInterp interp, FeatherObj str, size_t index);

/**
 * Get the byte length of a string.
 *
 * @param interp  Interpreter handle
 * @param str     String object
 * @return        Length in bytes
 */
size_t feather_string_byte_length(FeatherInterp interp, FeatherObj str);

/**
 * Create a substring by byte indices.
 *
 * @param interp  Interpreter handle
 * @param str     Source string
 * @param start   Start byte index (inclusive)
 * @param end     End byte index (exclusive)
 * @return        New string object containing the slice
 */
FeatherObj feather_string_slice(FeatherInterp interp, FeatherObj str, size_t start, size_t end);

/**
 * Concatenate two strings.
 *
 * @param interp  Interpreter handle
 * @param a       First string
 * @param b       Second string
 * @return        New string containing a + b
 */
FeatherObj feather_string_concat(FeatherInterp interp, FeatherObj a, FeatherObj b);

/**
 * Compare two strings lexicographically.
 *
 * @param interp  Interpreter handle
 * @param a       First string
 * @param b       Second string
 * @return        <0 if a<b, 0 if a==b, >0 if a>b
 */
int feather_string_compare(FeatherInterp interp, FeatherObj a, FeatherObj b);

/**
 * Check if two strings are equal.
 *
 * @param interp  Interpreter handle
 * @param a       First string
 * @param b       Second string
 * @return        1 if equal, 0 if not
 */
int feather_string_equal(FeatherInterp interp, FeatherObj a, FeatherObj b);

/**
 * Match a string against a glob pattern.
 *
 * Supports * (any sequence) and ? (single character).
 *
 * @param interp  Interpreter handle
 * @param pattern Pattern string
 * @param str     String to match
 * @param nocase  Non-zero for case-insensitive matching
 * @return        1 if matches, 0 if not
 */
int feather_string_match(FeatherInterp interp, FeatherObj pattern, FeatherObj str, int nocase);

/**
 * Create a new string builder for efficient string construction.
 *
 * @param interp    Interpreter handle
 * @param capacity  Initial capacity hint (0 for default)
 * @return          Builder handle (actually a FeatherObj)
 */
FeatherObj feather_string_builder_new(FeatherInterp interp, size_t capacity);

/**
 * Append a single byte to a string builder.
 *
 * @param interp   Interpreter handle
 * @param builder  Builder handle
 * @param b        Byte to append (0-255)
 */
void feather_string_builder_append_byte(FeatherInterp interp, FeatherObj builder, int b);

/**
 * Append an object's string representation to a builder.
 *
 * @param interp   Interpreter handle
 * @param builder  Builder handle
 * @param str      Object to append
 */
void feather_string_builder_append_obj(FeatherInterp interp, FeatherObj builder, FeatherObj str);

/**
 * Finish building and get the resulting string.
 *
 * The builder should not be used after calling this function.
 *
 * @param interp   Interpreter handle
 * @param builder  Builder handle
 * @return         Final string object
 */
FeatherObj feather_string_builder_finish(FeatherInterp interp, FeatherObj builder);

/* ==========================================================================
 * List Operations
 * ========================================================================== */

/**
 * Check if an object handle is nil (null).
 *
 * @param interp  Interpreter handle
 * @param obj     Object to check
 * @return        1 if nil, 0 if not
 */
int feather_list_is_nil(FeatherInterp interp, FeatherObj obj);

/**
 * Create a new empty list.
 *
 * @param interp  Interpreter handle
 * @return        New list object handle
 */
FeatherObj feather_list_create(FeatherInterp interp);

/**
 * Convert an object to a list.
 *
 * If the object is already a list, returns a copy.
 * If the object is a string, parses it as a TCL list.
 *
 * @param interp  Interpreter handle
 * @param obj     Object to convert
 * @return        List object, or 0 on parse error
 */
FeatherObj feather_list_from(FeatherInterp interp, FeatherObj obj);

/**
 * Append an item to the end of a list.
 *
 * @param interp  Interpreter handle
 * @param list    List to modify
 * @param item    Item to append
 * @return        The list (for chaining)
 */
FeatherObj feather_list_push(FeatherInterp interp, FeatherObj list, FeatherObj item);

/**
 * Remove and return the last item from a list.
 *
 * @param interp  Interpreter handle
 * @param list    List to modify
 * @return        The removed item, or 0 if list is empty
 */
FeatherObj feather_list_pop(FeatherInterp interp, FeatherObj list);

/**
 * Get the length of a list.
 *
 * @param interp  Interpreter handle
 * @param list    List object
 * @return        Number of elements
 */
size_t feather_list_length(FeatherInterp interp, FeatherObj list);

/**
 * Get an element by index.
 *
 * @param interp  Interpreter handle
 * @param list    List object
 * @param index   Element index (0-based)
 * @return        Element at index, or 0 if out of bounds
 */
FeatherObj feather_list_at(FeatherInterp interp, FeatherObj list, size_t index);

/**
 * Create a sublist from first to last (inclusive).
 *
 * @param interp  Interpreter handle
 * @param list    Source list
 * @param first   First index (inclusive)
 * @param last    Last index (inclusive)
 * @return        New list containing the range
 */
FeatherObj feather_list_slice(FeatherInterp interp, FeatherObj list, size_t first, size_t last);

/**
 * Set an element at a specific index.
 *
 * @param interp  Interpreter handle
 * @param list    List to modify
 * @param index   Element index
 * @param value   New value
 * @return        FEATHER_OK on success, FEATHER_ERROR if index out of bounds
 */
FeatherResult feather_list_set_at(FeatherInterp interp, FeatherObj list, size_t index, FeatherObj value);

/* ==========================================================================
 * Dict Operations
 * ========================================================================== */

/**
 * Create a new empty dict.
 *
 * @param interp  Interpreter handle
 * @return        New dict object handle
 */
FeatherObj feather_dict_create(FeatherInterp interp);

/**
 * Check if an object is a dict.
 *
 * @param interp  Interpreter handle
 * @param obj     Object to check
 * @return        1 if it's a dict, 0 if not
 */
int feather_dict_is_dict(FeatherInterp interp, FeatherObj obj);

/**
 * Convert an object to a dict.
 *
 * If the object is already a dict, returns a copy.
 * If the object is a list or string, parses it as key-value pairs.
 *
 * @param interp  Interpreter handle
 * @param obj     Object to convert
 * @return        Dict object, or 0 on parse error
 */
FeatherObj feather_dict_from(FeatherInterp interp, FeatherObj obj);

/**
 * Get a value from a dict by key.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict object
 * @param key     Key to look up
 * @return        Value for key, or 0 if not found
 */
FeatherObj feather_dict_get(FeatherInterp interp, FeatherObj dict, FeatherObj key);

/**
 * Set a value in a dict.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict to modify
 * @param key     Key to set
 * @param value   Value to associate with key
 * @return        The dict (for chaining)
 */
FeatherObj feather_dict_set(FeatherInterp interp, FeatherObj dict, FeatherObj key, FeatherObj value);

/**
 * Check if a key exists in a dict.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict object
 * @param key     Key to check
 * @return        1 if key exists, 0 if not
 */
int feather_dict_exists(FeatherInterp interp, FeatherObj dict, FeatherObj key);

/**
 * Remove a key from a dict.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict to modify
 * @param key     Key to remove
 * @return        The dict (for chaining)
 */
FeatherObj feather_dict_remove(FeatherInterp interp, FeatherObj dict, FeatherObj key);

/**
 * Get the number of key-value pairs in a dict.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict object
 * @return        Number of entries
 */
size_t feather_dict_size(FeatherInterp interp, FeatherObj dict);

/**
 * Get all keys from a dict as a list.
 *
 * Keys are returned in insertion order.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict object
 * @return        List of keys
 */
FeatherObj feather_dict_keys(FeatherInterp interp, FeatherObj dict);

/**
 * Get all values from a dict as a list.
 *
 * Values are returned in key insertion order.
 *
 * @param interp  Interpreter handle
 * @param dict    Dict object
 * @return        List of values
 */
FeatherObj feather_dict_values(FeatherInterp interp, FeatherObj dict);

#ifdef __cplusplus
}
#endif

#endif /* FEATHER_API_H */
