/*
 * Feather Core for Go
 *
 * This file pulls in the feather C interpreter as part of the Go build.
 * It replaces the need for a separate libfeather.a build step.
 */

#include "../src/feather_amalgamation.c"
