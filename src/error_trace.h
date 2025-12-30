#ifndef FEATHER_ERROR_TRACE_H
#define FEATHER_ERROR_TRACE_H

#include "feather.h"

/**
 * feather_error_is_active checks if error propagation is in progress.
 *
 * Returns 1 if ::tcl::errors::active is "1", 0 otherwise.
 */
int feather_error_is_active(const FeatherHostOps *ops, FeatherInterp interp);

/**
 * feather_error_init initializes error state when error/throw is called.
 *
 * Sets ::tcl::errors::active to "1" and builds initial errorinfo/errorstack.
 * This should only be called if error is not already active.
 *
 * @param ops The host operations
 * @param interp The interpreter
 * @param message The error message
 * @param cmd The command name that raised the error
 * @param args The arguments to the command (as a list)
 */
void feather_error_init(const FeatherHostOps *ops, FeatherInterp interp,
                        FeatherObj message, FeatherObj cmd, FeatherObj args);

/**
 * feather_error_append_frame appends a stack frame during error propagation.
 *
 * Called when exiting a proc frame with TCL_ERROR. Adds information about
 * the procedure call to both -errorinfo and -errorstack.
 *
 * @param ops The host operations
 * @param interp The interpreter
 * @param procName The name of the procedure
 * @param args The arguments passed to the procedure (as a list)
 * @param line The line number in the procedure where the error occurred
 */
void feather_error_append_frame(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj procName, FeatherObj args, size_t line);

/**
 * feather_error_finalize copies accumulated error state to return options.
 *
 * Called when catch/try catches the error. Transfers the accumulated
 * -errorinfo, -errorstack, and -errorline from ::tcl::errors:: variables
 * to the interpreter's return options. Also sets the global ::errorInfo
 * and ::errorCode variables.
 *
 * Resets the error state (sets active to "0").
 *
 * @param ops The host operations
 * @param interp The interpreter
 */
void feather_error_finalize(const FeatherHostOps *ops, FeatherInterp interp);

#endif
