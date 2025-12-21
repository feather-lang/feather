#ifndef INCLUDE_TCLC_INTERNAL
#define INCLUDE_TCLC_INTERNAL

#include "tclc.h"

// Internal forward declarations go here

/**
 * tcl_lookup_builtin looks up a builtin command by name.
 * Returns NULL if no builtin with that name exists.
 */
TclBuiltinCmd tcl_lookup_builtin(const char *name, size_t len);

#endif
